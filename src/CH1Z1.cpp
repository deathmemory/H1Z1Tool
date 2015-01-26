/*****************************************************************************\
Copyright (C) 2013-2014 <martinjk at outloook dot com>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.
\*****************************************************************************/
#include <include/CH1Z1.h>
#include <include/D3Keys.h>
#include <include/H1Z1Def.h>

CH1Z1* CH1Z1::_instance = nullptr;

D3DXVECTOR3& GetMatrixAxis(D3DXMATRIX matrix, UINT i)
{
	return *(D3DXVECTOR3*)&matrix.m[i][0];
}

CH1Z1::CH1Z1(HANDLE proc) : 
	hH1Z1(proc)
{
	if (!proc)
		return;

	// Create basic ptr's
	ReadH1Z1(this->hH1Z1, (void*)(H1Z1_DEF_LATEST::CGame), &this->CGame, sizeof(DWORD64), NULL);
	ReadH1Z1(this->hH1Z1, (void*)(H1Z1_DEF_LATEST::CGraphic), &this->CGraphics, sizeof(DWORD64), NULL);

	ReadH1Z1(this->hH1Z1, (void*)(CGame + STATIC_CAST(H1Z1_DEF_LATEST::LocalPlayerOffset)), &this->LocalPlayer, sizeof(DWORD64), NULL);
	ReadH1Z1(this->hH1Z1, (void*)(CPlayer + STATIC_CAST(H1Z1_DEF_LATEST::CPlayerOffset_Position)), &this->vecPlayerPos, sizeof(CVector3), NULL);

	// Create player heading line
	D3DXCreateLine(p_Device, &this->dxLine);
	this->dxLine->SetWidth(2);
	this->dxLine->SetPattern(0xffffffff);
	D3DXCreateSprite(p_Device, &dxSprite);

	// Load & generate map texture
	wchar_t szExePath[MAX_PATH] = { 0 };
	GetModuleFileNameW(GetModuleHandle(NULL), szExePath, MAX_PATH);

	// Fix path in string
	for (size_t i = wcslen(szExePath); i > 0; --i)
	{
		if (szExePath[i] == L'\\')
		{
			szExePath[i + 1] = L'\0';
			break;
		}
	}

	std::wstring loc = szExePath;
	loc.append(L"map.png");

	D3DXCreateTextureFromFile(p_Device, loc.c_str(), &dxTexture);

	ReadH1Z1(this->hH1Z1, (void*)(this->CGraphics + STATIC_CAST(H1Z1_DEF_LATEST::CGraphicsOffset_ScreenWidth)), &this->_screenWidth, sizeof(this->_screenWidth), NULL);
	ReadH1Z1(this->hH1Z1, (void*)(this->CGraphics + STATIC_CAST(H1Z1_DEF_LATEST::CGraphicsOffset_ScreenHeight)), &this->_screenHeight, sizeof(this->_screenHeight), NULL);
}

CH1Z1::~CH1Z1()
{
	if(dxTexture)
		dxTexture->Release();
	
	if(dxLine)
		dxLine->Release();
	
	if(dxSprite)
		dxSprite->Release();

	if(p_Device)
		p_Device->Release();
}

void CH1Z1::ParseEntities()
{
#if _DEBUG_ITEMS
	DrawString("Entities nearby(300m)", 15, 120, 240, 240, 250, pFontSmall);
	DrawString("Players nearby(300m)", 515, 120, 240, 240, 250, pFontSmall);
	DrawString("Objects nearby(300m)", 915, 120, 240, 240, 250, pFontSmall);

	int entityOffset = 150;
	int playerOffset = 150;
	int objectOffset = 150;
#endif
	int warningOffset = 15;

	DWORD entityCount;
	ReadH1Z1(this->hH1Z1, (void*)(this->CGame + STATIC_CAST(H1Z1_DEF_LATEST::EntityPoolCount)), &entityCount, sizeof(DWORD), NULL);

	// Set the current object to localplayer so we parse every entity
	DWORD_PTR _obj = this->LocalPlayer;
	DWORD_PTR _namePtr;

	// Now parse all
	for (uint16 entity = 0; entity < entityCount-1; entity++)
	{
		// Read entity from memory
		ReadH1Z1(this->hH1Z1, (void*)(_obj + STATIC_CAST(H1Z1_DEF_LATEST::EntityTableOffset)), &_obj, sizeof(DWORD64), NULL);

		// Generate new entity/object for this iteration/loop
		H1Z1Def::CObject scopeobj;

		ReadH1Z1(this->hH1Z1, (void*)(_obj + STATIC_CAST(H1Z1_DEF_LATEST::CEntityOffset_Name)), &_namePtr, sizeof(DWORD64), NULL);
		ReadH1Z1(this->hH1Z1, (void*)(_namePtr), &scopeobj._name, sizeof(scopeobj._name), NULL);
		ReadH1Z1(this->hH1Z1, (void*)(_obj + +STATIC_CAST(H1Z1_DEF_LATEST::CEntityOffset_Position)), &scopeobj._position, sizeof(CVector3), NULL);
		ReadH1Z1(this->hH1Z1, (void*)(_obj + +STATIC_CAST(H1Z1_DEF_LATEST::CEntityOffset_Type)), &scopeobj._type, sizeof(int32), NULL);

		// Get the entity color
		auto color = this->GetEntityColor(scopeobj._type);
		scopeobj.R = std::get<1>(color);
		scopeobj.G = std::get<2>(color);
		scopeobj.B = std::get<3>(color);
		scopeobj.A = std::get<0>(color);

		// Ignore game designer placed stuff
		// Also disable empty strings and punji sticks/ wire and wood arrows
		if (scopeobj._type == (int32)H1Z1Def::EntityTypes::TYPE_Door
			|| scopeobj._type == (int32)H1Z1Def::EntityTypes::TYPE_Door2
			|| scopeobj._type == (int32)H1Z1Def::EntityTypes::TYPE_Door3
			|| scopeobj._type == (int32)H1Z1Def::EntityTypes::TYPE_FireHydrant
			|| scopeobj._name == ""
			|| scopeobj._type == (int32)H1Z1Def::EntityTypes::TYPE_AggressiveItems
			|| scopeobj._type == 55 /*unkown obj*/
			|| scopeobj._type == 44 /*wood arrow*/)
			continue;

		// Check if the entity type is in a valid range
		if (scopeobj._type < static_cast<int32>(H1Z1_DEF_LATEST::MAX_ENTITY_TYPE) && scopeobj._type >= 0)
		{
			char szString[256];

			float fDistance = (vecPlayerPos - scopeobj._position).Length();

			// Do not draw zombies to the entity list, just add a warning if they're close!
			if (scopeobj._type == (int32)H1Z1Def::EntityTypes::TYPE_Zombie
				|| scopeobj._type == (int32)H1Z1Def::EntityTypes::TYPE_Wolf
				|| scopeobj._type == (int32)H1Z1Def::EntityTypes::TYPE_Zombie2)
			{
#if _ATTACK_ALERT
				if (fDistance < 25.f)
				{
					RECT desktop = this->GetDesktop();

					char szMessage[128];
					sprintf_s(szMessage, "!>> Attention: There\'s a %s close to you <<!", scopeobj._name);

					DrawString(szMessage, desktop.right - (this->_screenWidth / 2) - 150, warningOffset, 255, 0, 0, pFontSmall);

					warningOffset += 15;

					// Draw the zombie in 3D so the player will see him
					CVector3 _vecScreen;
					scopeobj._position.fY += this->CalculateEntity3DModelOffset(scopeobj._type);
					bool bResult = this->WorldToScreen(scopeobj._position, _vecScreen);
					if (bResult)
					{
						sprintf_s(szString, ">> -%s- <<", scopeobj._name);

						DrawString(szString, _vecScreen.fX, _vecScreen.fY, 255, 50, 50, pFontSmall);
					}
				}
#endif
				continue;
			}

#if  _ATTACK_NEAR_PLAYER_ALERT
			if (scopeobj._type == (int32)H1Z1Def::EntityTypes::TYPE_Player)
			{
				if (fDistance < 80.f)
				{
					_IGNORE_PLAYERS
					{
						RECT desktop = this->GetDesktop();

						char szMessage[128];
						sprintf_s(szMessage, "!>> Attention: Player %s is close to you <<!", scopeobj._name);

						DrawString(szMessage, desktop.right - (this->_screenWidth / 2) - 150, warningOffset, 255, 0, 0, pFontSmall);

						warningOffset += 15;

						// Draw the zombie in 3D so the player will see him
						CVector3 _vecScreen;
						scopeobj._position.fY += this->CalculateEntity3DModelOffset(scopeobj._type);
						bool bResult = this->WorldToScreen(scopeobj._position, _vecScreen);
						if (bResult)
						{
							sprintf_s(szString, ">> !%s! <<", scopeobj._name);

							DrawString(szString, _vecScreen.fX, _vecScreen.fY, 255, 50, 50, pFontSmall);
						}
					}
				}
			}
#endif

			// Sort out types
			{
				if (scopeobj._type == (int32)H1Z1Def::EntityTypes::TYPE_Player)
					scopeobj._isPlayer = true;

				if (scopeobj._position.fX == 0.f
					&& scopeobj._position.fY == 0.f
					&& scopeobj._position.fZ == 0.f
					&& !scopeobj._isPlayer)
				{
					// Grab the original position
					ReadH1Z1(this->hH1Z1, (void*)(_obj + STATIC_CAST(H1Z1_DEF_LATEST::CEntityObjectOffset_Position)), &scopeobj._objectPosition, sizeof(CVector3), NULL);
					fDistance = (this->vecPlayerPos - scopeobj._objectPosition).Length();

					scopeobj._isObject = true;
				}

				if (!scopeobj._isPlayer && !scopeobj._isObject)
					scopeobj._isEntity = true;
			}

#if _DEBUG_ITEMS
			// Draw to list
			if (scopeobj._isObject)
			{
				sprintf_s(szString, "- %s, Type[%d], Position[%.2f, %.2f, %.2f], Distance[%.2fm]",
					scopeobj._name,
					scopeobj._type,
					scopeobj._objectPosition.fX,
					scopeobj._objectPosition.fY,
					scopeobj._objectPosition.fZ,
					fDistance);

				DrawString(szString, 915, objectOffset, 240, 240, 250, pFontSmaller);
				objectOffset += 15;
			}

			if (scopeobj._isPlayer)
			{
				sprintf_s(szString, "- %s, Position[%.2f, %.2f, %.2f], Distance[%.1fm]",
					scopeobj._name,
					scopeobj._position.fX,
					scopeobj._position.fY,
					scopeobj._position.fZ,
					fDistance);

				DrawString(szString, 515, playerOffset, 240, 240, 250, pFontSmaller);
				playerOffset += 15;
			}

			if (scopeobj._isEntity)
			{
				sprintf_s(szString, "- %s, Type[%d], Position[%.2f, %.2f, %.2f], Distance[%.2fm]",
					scopeobj._name,
					scopeobj._type,
					scopeobj._position.fX,
					scopeobj._position.fY,
					scopeobj._position.fZ,
					fDistance);

				DrawString(szString, 15, entityOffset, 240, 240, 250, pFontSmaller);
				entityOffset += 15;
			}
#endif

#if _3D_ENTITY_DISPLAY
			// Check if he's a player so we draw a big text with a hint
			if (!scopeobj._isObject) // player & entities
			{
				// Draw it on the screen(World 2 Screen)
				CVector3 _vecScreen;
				scopeobj._position.fY += this->CalculateEntity3DModelOffset(scopeobj._type);
				bool bIsOnScreen = this->WorldToScreen(scopeobj._position, _vecScreen);
				
				if (bIsOnScreen)
				{
					if (scopeobj._isPlayer)
						sprintf_s(szString, "Player: %s  (%2.fm)", scopeobj._name, fDistance);
					else
						sprintf_s(szString, "%s  (%2.fm)", scopeobj._name, fDistance);

					DrawString(szString, _vecScreen.fX, _vecScreen.fY, scopeobj.R, scopeobj.G, scopeobj.B, scopeobj._isPlayer ? pFontSmall : pFontSmaller);
				}
			}
			else // objects
			{
				// Draw it on the screen(World 2 Screen)
				CVector3 _vecScreen;
				scopeobj._objectPosition.fY += this->CalculateEntity3DModelOffset(scopeobj._type);
				bool bIsOnScreen = this->WorldToScreen(scopeobj._objectPosition, _vecScreen);

				if (bIsOnScreen)
				{
					sprintf_s(szString, "%s  (%2.fm)", scopeobj._name, fDistance);
					DrawString(szString, _vecScreen.fX, _vecScreen.fY, scopeobj.R, scopeobj.G, scopeobj.B, pFontSmaller);
				}

			}
#endif

#if _MINIMAP
			// Draw to minimap
			RECT desktop = GetDesktop();

			auto fWidth = 200;
			auto fHeight = 200;
			auto fX = (desktop.right - 20 - (fWidth/2));
			auto fY = (desktop.bottom - 75 - (fHeight/2));

			// Check if we're a lootable thing or whatever else
			CVector3 diff;
			if (scopeobj._isObject) // Parse objects
				diff = CVector3(scopeobj._objectPosition.fX - this->vecPlayerPos.fX,
					scopeobj._objectPosition.fY - this->vecPlayerPos.fY,
					scopeobj._objectPosition.fZ - this->vecPlayerPos.fZ);
			else // Parse entities, players etc.
				diff = CVector3(scopeobj._position.fX - this->vecPlayerPos.fX,
					scopeobj._position.fY - this->vecPlayerPos.fY,
					scopeobj._position.fZ - this->vecPlayerPos.fZ);

			// Check if we would be out of the minimap range
			if (diff.Length() <= 200)
			{
				if(diff.fX >= 0)
					fX += diff.fX > 95 ? 95 : diff.fX;
				else
					fX += diff.fX < -95 ? -95 : diff.fX;

				if (diff.fY >= 0)
					fY += diff.fY > 95 ? 95 : diff.fY;
				else
					fY += diff.fY < -95 ? -95 : diff.fY;

				if(scopeobj._type == (int32)H1Z1Def::EntityTypes::TYPE_Player)
					FillRGB(fX, fY, 4, 4, 255, 0, 0, 255);
				else if(scopeobj._type == (int32)H1Z1Def::EntityTypes::TYPE_OffRoader)
					FillRGB(fX, fY, 4, 4, 0, 0, 255, 255);
				else if(scopeobj._isObject) // Lootable objects
					FillRGB(fX, fY, 4, 4, 0, 255, 255, 255);
				else // other entities
					FillRGB(fX, fY, 4, 4, 0, 255, 0, 255);
			}
#endif
		}
		else
			return;
	}
}

void CH1Z1::Process()
{
	// Global over scope reachable needed:
	float fHeading = 0.f;

	{
		ReadH1Z1(this->hH1Z1, (void*)(this->LocalPlayer + STATIC_CAST(H1Z1_DEF_LATEST::CPlayerOffset_Position)), &this->vecPlayerPos, sizeof(CVector3), NULL);

		char szString[512] = { 0 };
		sprintf_s(szString, "World Position: %.2f, %.2f, %.2f", this->vecPlayerPos.fX, this->vecPlayerPos.fY, this->vecPlayerPos.fZ);
		DrawString(szString, 15, 50, 240, 240, 250, pFontSmaller);
	}

	{
		ReadH1Z1(this->hH1Z1, (void*)(this->LocalPlayer + STATIC_CAST(H1Z1_DEF_LATEST::CPlayerOffset_Heading)), &fHeading, sizeof(float), NULL);
		
		auto compass = this->CalculateWorldCompassHeading(fHeading);
		char szString[126] = { 0 };
		sprintf_s(szString, "Heading to %s [%.2f]", compass.c_str(), fHeading);
		DrawString(szString, 15, 65, 240, 240, 250, pFontSmaller);
	}

	{
		// Do not read the playerPos again as we've already read the position this frame
		CVector3 PleasentValley = CVector3(0, 0, -1200);
		
		float fRange = (PleasentValley - this->vecPlayerPos).Length();

		char szString[512] = { 0 };
		sprintf_s(szString, "Distance to Pleasant Valley: %.0fm", fRange);
		DrawString(szString, 15, 80, 240, 240, 250, pFontSmaller);
	}

	{
		ReadH1Z1(this->hH1Z1, (void*)(this->CGraphics + STATIC_CAST(H1Z1_DEF_LATEST::CGraphicsOffset_ScreenWidth)), &this->_screenWidth, sizeof(this->_screenWidth), NULL);
		ReadH1Z1(this->hH1Z1, (void*)(this->CGraphics + STATIC_CAST(H1Z1_DEF_LATEST::CGraphicsOffset_ScreenHeight)), &this->_screenHeight, sizeof(this->_screenHeight), NULL);
	}

#if _MINIMAP
	{
		// Minimap
		RECT desktop = this->GetDesktop();

		auto fWidth = 200;
		auto fHeight = 200;
		auto fX = (desktop.right - 20 - fWidth);
		auto fY = (desktop.bottom - 75);

		// Crosshair
		FillRGB(fX, fY - (fHeight / 2), fWidth, 1, 240, 240, 250, 255);
		FillRGB(fX + (fWidth / 2), fY-fHeight, 1, fHeight, 240, 240, 250, 255);

		// Border of map
		// Horicontal
		FillRGB(fX, fY - fHeight, 1, fHeight, 240, 240, 250, 255);
		FillRGB(fX+fWidth, fY - fHeight, 1, fHeight, 240, 240, 250, 255);

		// Vertical
		FillRGB(fX, fY - fHeight, fWidth, 1, 240, 240, 250, 255);
		FillRGB(fX, fY, fWidth, 1, 240, 240, 250, 255);

		// Draw local player pixel
		FillRGB((fX + (fWidth / 2)-1), (fY - (fHeight / 2)-1), 4, 4, 255, 0, 0, 255);

		// Draw compass
		DrawString("N", fX + (fWidth / 2) - 5, fY - fHeight - 20, 240, 240, 250, pFontSmall);
		DrawString("E", fX - 15 , fY - (fHeight / 2) - 8, 240, 240, 250, pFontSmall);
		DrawString("W", fX + fWidth + 5, fY - (fHeight / 2) - 5, 240, 240, 250, pFontSmall);
		DrawString("S", fX + (fWidth / 2) - 5, fY + 5, 240, 240, 250, pFontSmall);

		// Draw player heading line
#pragma message("FIX CONVERSION!")
		D3DXVECTOR2 points[2];

		points[0] = D3DXVECTOR2((fX + (fWidth / 2) - 1), (fY - (fHeight / 2) - 1));
		points[1] = D3DXVECTOR2(fX + (100 * fHeading) , fY + (100 * fHeading));

		this->dxLine->Draw(points, 2, 0xffffffff);
	}
#endif

	// Prase entities
	this->ParseEntities();
}

void CH1Z1::DrawFullMap()
{
	RECT desktop;
	const HWND hDesktop = GetDesktopWindow();
	GetWindowRect(hDesktop, &desktop);

	// Draw fullscreen map
	this->dxSprite->Begin(D3DXSPRITE_ALPHABLEND);
	RECT rc = { (desktop.right / 4) / 2, (desktop.bottom / 4) / 2, (desktop.right / 4) * 3, (desktop.bottom / 4) * 3 };
	D3DXVECTOR2 spriteCentre = D3DXVECTOR2(32.0f, 32.0f);
	D3DXVECTOR2 trans = D3DXVECTOR2((desktop.right / 2) - desktop.right / 4, (desktop.bottom / 8));
	float rotation = 0.f;
	D3DXMATRIX mat;
	D3DXVECTOR2 scaling(0.4f, 0.4f);
	D3DXMatrixTransformation2D(&mat, NULL, 0.0, &scaling, &spriteCentre, rotation, &trans);
	this->dxSprite->SetTransform(&mat);
	this->dxSprite->Draw(this->dxTexture, NULL, NULL, NULL, 0xFFFFFFFF);
	this->dxSprite->End();

	/*
	// WHOLE WORLD MAP
	int32 einheit = 200 / 6000;

	int32 x = 0;
	if (scopeobj._position.fX < 0)
	x += -(scopeobj._position.fX);
	else
	x += 3000 + scopeobj._position.fX;

	x = (x * einheit);

	int32 y = 0;
	if (scopeobj._position.fY < 0)
	y += 3000 + scopeobj._position.fY;
	else
	y += (3000 - scopeobj._position.fX);

	x = (x * einheit);
	y = (y * einheit);

	fX += x;
	fY += y;

	FillRGB(fX, fY, 4, 4, 0, 255, 0, 255);
	*/
}

char* CH1Z1::GetEntityName(DWORD_PTR ptr)
{
	static char itemName[64];

	DWORD64 ItemNamePtr;
	ReadH1Z1(this->hH1Z1, (void*)(ptr + STATIC_CAST(H1Z1_DEF_LATEST::CEntityOffset_Name)), &ItemNamePtr, sizeof(DWORD64), NULL);
	ReadH1Z1(this->hH1Z1, (void*)(ItemNamePtr), &itemName, sizeof(itemName), NULL);

	return itemName;
}

std::string CH1Z1::CalculateWorldCompassHeading(float playerHeading)
{
	std::string auxHead;

	if (playerHeading < 1.9625 && playerHeading > 1.1775)
		auxHead = "N";
	else if (playerHeading < 1.1775 && playerHeading > 0.3925)
		auxHead = "NE";
	else if (playerHeading < 0.3925 && playerHeading > -0.3925)
		auxHead = "E";
	else if (playerHeading < -0.3925 && playerHeading > -1.1775)
		auxHead = "SE";
	else if (playerHeading < -1.1775 && playerHeading > -1.9625)
		auxHead = "S";
	else if (playerHeading < -1.9625 && playerHeading > -2.7475) 
		auxHead = "SW";
	else if (playerHeading < -2.7475 && playerHeading > -3.14)
		auxHead = "W";
	else 
		auxHead = "NW";

	return auxHead;
}

bool CH1Z1::WorldToScreen(const CVector3& World, CVector3& Out)
{
	DWORD_PTR CCamera;
	DWORD_PTR CCameraMatrix;

	ReadH1Z1(this->hH1Z1, (void*)(this->CGraphics + STATIC_CAST(H1Z1_DEF_LATEST::CGraphicsOffset_Camera)), &CCamera, sizeof(DWORD64), NULL);
	ReadH1Z1(this->hH1Z1, (void*)(CCamera + STATIC_CAST(H1Z1_DEF_LATEST::CGraphicsOffset_Camera__Matrix)), &CCameraMatrix, sizeof(DWORD64), NULL);

	CCameraMatrix += 0x10;

	D3DXMATRIX d3Matrix;
	ReadH1Z1(this->hH1Z1, (void*)(CCameraMatrix + STATIC_CAST(H1Z1_DEF_LATEST::CGraphicsOffset_D3DXMATRIX)), &d3Matrix, sizeof(D3DXMATRIX), NULL);

	D3DXMatrixTranspose(&d3Matrix, &d3Matrix);

	d3Matrix._21 *= -1;
	d3Matrix._22 *= -1;
	d3Matrix._23 *= -1;
	d3Matrix._24 *= -1;

	// Convert D3Vector3 to CVector3
	auto _tmp = GetMatrixAxis(d3Matrix, 3);
	float w = CVector3(_tmp.x, _tmp.y, _tmp.z).Dot(World) + d3Matrix.m[3][3];

	if (w < 0.098)
		return false;

	_tmp = GetMatrixAxis(d3Matrix, 0);
	float x = CVector3(_tmp.x, _tmp.y, _tmp.z).Dot(World) + d3Matrix.m[0][3];

	_tmp = GetMatrixAxis(d3Matrix, 1);
	float y = CVector3(_tmp.x, _tmp.y, _tmp.z).Dot(World) + d3Matrix.m[1][3];

	Out.fX = (this->_screenWidth / 2) * (1.0 + x / w);
	Out.fY = (this->_screenHeight / 2) * (1.0 - y / w);
	return true;
}

CVector3 CH1Z1::GetEntityDirection(DWORD64 entity)
{
	float fx = static_cast<float>(*(DWORD64 *)entity + 0x1F0);
	float fy = static_cast<float>(*(DWORD64 *)entity + 0x1F4);

	CVector3 r;
	r.fX = sinf(fx) * cosf(fy);
	r.fY = sin(fy);
	r.fZ = cosf(fy) * cosf(fx);

	return r;
}

std::tuple<BYTE, BYTE, BYTE, BYTE> CH1Z1::GetEntityColor(BYTE entityType)
{
	switch ((H1Z1Def::EntityTypes)entityType)
	{
		case H1Z1Def::EntityTypes::TYPE_OffRoader:
		case H1Z1Def::EntityTypes::TYPE_PickupTruck:
		case H1Z1Def::EntityTypes::TYPE_PoliceCar:
			return std::make_tuple(255, 100, 50, 255);

		default:
			return std::make_tuple(255, 240, 240, 250);
	}

	return std::make_tuple(255, 240, 240, 250);
}

float CH1Z1::CalculateEntity3DModelOffset(BYTE entityType)
{
	float fOffset = 0.0f;
	switch ((H1Z1Def::EntityTypes)entityType)
	{
		case H1Z1Def::EntityTypes::TYPE_OffRoader:
		case H1Z1Def::EntityTypes::TYPE_PickupTruck:
		case H1Z1Def::EntityTypes::TYPE_PoliceCar:
			fOffset = 1.0f;
			break;

		case H1Z1Def::EntityTypes::TYPE_WreckedTruck:
			fOffset = 2.0f;
			break;

		case H1Z1Def::EntityTypes::TYPE_WreckedVan:
			fOffset = 2.5f;
			break;

		case H1Z1Def::EntityTypes::TYPE_Zombie:
		case H1Z1Def::EntityTypes::TYPE_Wolf:
		case H1Z1Def::EntityTypes::TYPE_Zombie2:
		case H1Z1Def::EntityTypes::TYPE_Player:
			fOffset = 1.75f;
			break;

		default:
			break;
	}
	return fOffset;
}