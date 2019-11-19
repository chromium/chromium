// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/reg_utils.h"

#include <Windows.h>

#include <atlbase.h>

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "chrome/credential_provider/common/gcp_strings.h"

namespace credential_provider {

// Root registry key for GCP configuration and state.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define CREDENTIAL_PROVIDER_REGISTRY_KEY L"Software\\Google\\GCP"
#else
#define CREDENTIAL_PROVIDER_REGISTRY_KEY L"Software\\Chromium\\GCP"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

const wchar_t kGcpRootKeyName[] = CREDENTIAL_PROVIDER_REGISTRY_KEY;

const wchar_t kGcpUsersRootKeyName[] =
    CREDENTIAL_PROVIDER_REGISTRY_KEY L"\\Users";

const wchar_t kWinlogonUserListRegKey[] =
    L"SOFTWARE\\Microsoft\\Windows NT"
    L"\\CurrentVersion\\Winlogon\\SpecialAccounts\\UserList";

const wchar_t kLogonUiUserTileRegKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Authentication\\LogonUI"
    L"\\UserTile";

const wchar_t kMicrosoftCryptographyRegKey[] =
    L"SOFTWARE\\Microsoft\\Cryptography";
const wchar_t kMicrosoftCryptographyMachineGuidRegKey[] = L"MachineGuid";

namespace {

constexpr wchar_t kAccountPicturesRootRegKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AccountPicture\\Users";
constexpr wchar_t kImageRegKey[] = L"Image";

HRESULT SetMachineRegDWORD(const base::string16& key_name,
                           const base::string16& name,
                           DWORD value) {
  base::win::RegKey key;
  LONG sts = key.Create(HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_WRITE);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  sts = key.WriteValue(name.c_str(), value);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  return S_OK;
}

HRESULT SetMachineRegString(const base::string16& key_name,
                            const base::string16& name,
                            const base::string16& value) {
  base::win::RegKey key;
  LONG sts = key.Create(HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_WRITE);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  if (value.empty()) {
    sts = key.DeleteValue(name.c_str());
    if (sts == ERROR_FILE_NOT_FOUND)
      sts = ERROR_SUCCESS;
  } else {
    sts = key.WriteValue(name.c_str(), value.c_str());
  }

  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  return S_OK;
}

base::string16 GetImageRegKeyForSpecificSize(int image_size) {
  return base::StringPrintf(L"%ls%i", kImageRegKey, image_size);
}

base::string16 GetAccountPictureRegPathForUSer(const base::string16& user_sid) {
  return base::StringPrintf(L"%ls\\%ls", kAccountPicturesRootRegKey,
                            user_sid.c_str());
}

}  // namespace

HRESULT GetMachineRegDWORD(const base::string16& key_name,
                           const base::string16& name,
                           DWORD* value) {
  base::win::RegKey key;
  LONG sts = key.Open(HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_READ);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  sts = key.ReadValueDW(name.c_str(), value);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  return S_OK;
}

HRESULT GetMachineRegString(const base::string16& key_name,
                            const base::string16& name,
                            wchar_t* value,
                            ULONG* length) {
  DCHECK(value);
  DCHECK(length);
  DCHECK_GT(*length, 0u);

  base::win::RegKey key;
  LONG sts = key.Open(HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_READ);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  // read one less character that specified in |length| so that the returned
  // string can always be null terminated.  Note that string registry values
  // are not guaranteed to be null terminated.
  DWORD type;
  ULONG local_length = (*length - 1) * sizeof(decltype(value[0]));
  sts = key.ReadValue(name.c_str(), value, &local_length, &type);
  if (type != REG_SZ)
    return HRESULT_FROM_WIN32(ERROR_CANTREAD);

  // When using this overload of the ReadValue() method, the returned length
  // is in bytes.  The caller expects the length in characters.
  local_length /= sizeof(decltype(value[0]));

  if (sts != ERROR_SUCCESS) {
    if (sts == ERROR_MORE_DATA)
      *length = local_length;
    return HRESULT_FROM_WIN32(sts);
  }

  value[local_length] = 0;
  *length = local_length;
  return S_OK;
}

HRESULT GetAccountPictureRegString(const base::string16& user_sid,
                                   int image_size,
                                   wchar_t* value,
                                   ULONG* length) {
  return GetMachineRegString(GetAccountPictureRegPathForUSer(user_sid),
                             GetImageRegKeyForSpecificSize(image_size), value,
                             length);
}

// Sets a specific account picture registry key in HKEY_LOCAL_MACHINE
HRESULT SetAccountPictureRegString(const base::string16& user_sid,
                                   int image_size,
                                   const base::string16& value) {
  return SetMachineRegString(GetAccountPictureRegPathForUSer(user_sid),
                             GetImageRegKeyForSpecificSize(image_size), value);
}

HRESULT GetGlobalFlag(const base::string16& name, DWORD* value) {
  return GetMachineRegDWORD(kGcpRootKeyName, name, value);
}

HRESULT GetGlobalFlag(const base::string16& name,
                      wchar_t* value,
                      ULONG* length) {
  return GetMachineRegString(kGcpRootKeyName, name, value, length);
}

base::string16 GetGlobalFlagOrDefault(const base::string16& reg_key,
                                      const base::string16& default_value) {
  wchar_t reg_value_buffer[256];
  ULONG length = base::size(reg_value_buffer);
  HRESULT hr = GetGlobalFlag(reg_key, reg_value_buffer, &length);
  if (FAILED(hr))
    return default_value;

  return reg_value_buffer;
}

DWORD GetGlobalFlagOrDefault(const base::string16& reg_key,
                             const DWORD& default_value) {
  DWORD value;
  HRESULT hr = GetGlobalFlag(reg_key, &value);
  return SUCCEEDED(hr) ? value : default_value;
}

HRESULT SetGlobalFlagForTesting(const base::string16& name,
                                const base::string16& value) {
  return SetMachineRegString(kGcpRootKeyName, name, value);
}

HRESULT SetGlobalFlagForTesting(const base::string16& name, DWORD value) {
  return SetMachineRegDWORD(kGcpRootKeyName, name, value);
}

HRESULT GetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        DWORD* value) {
  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%s\\%s", kGcpUsersRootKeyName,
             sid.c_str());
  return GetMachineRegDWORD(key_name, name, value);
}

HRESULT GetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        wchar_t* value,
                        ULONG* length) {
  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%s\\%s", kGcpUsersRootKeyName,
             sid.c_str());
  return GetMachineRegString(key_name, name, value, length);
}

HRESULT SetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        DWORD value) {
  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%s\\%s", kGcpUsersRootKeyName,
             sid.c_str());
  return SetMachineRegDWORD(key_name, name, value);
}

HRESULT SetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        const base::string16& value) {
  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%s\\%s", kGcpUsersRootKeyName,
             sid.c_str());
  return SetMachineRegString(key_name, name, value);
}

HRESULT RemoveAllUserProperties(const base::string16& sid) {
  base::win::RegKey key;
  LONG sts = key.Open(HKEY_LOCAL_MACHINE, kGcpUsersRootKeyName, KEY_WRITE);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  sts = key.DeleteKey(sid.c_str());
  return sts != ERROR_SUCCESS ? HRESULT_FROM_WIN32(sts) : S_OK;
}

HRESULT GetUserTokenHandles(
    std::map<base::string16, UserTokenHandleInfo>* sid_to_handle_info) {
  DCHECK(sid_to_handle_info);
  sid_to_handle_info->clear();

  base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, kGcpUsersRootKeyName);
  for (; iter.Valid(); ++iter) {
    const wchar_t* sid = iter.Name();
    wchar_t gaia_id[256];
    ULONG length = base::size(gaia_id);
    HRESULT gaia_id_hr = GetUserProperty(sid, kUserId, gaia_id, &length);
    wchar_t token_handle[256];
    length = base::size(token_handle);
    HRESULT token_handle_hr =
        GetUserProperty(sid, kUserTokenHandle, token_handle, &length);
    sid_to_handle_info->emplace(
        sid,
        UserTokenHandleInfo{SUCCEEDED(gaia_id_hr) ? gaia_id : L"",
                            SUCCEEDED(token_handle_hr) ? token_handle : L""});
  }
  return S_OK;
}

HRESULT GetSidFromId(const base::string16& id, wchar_t* sid, ULONG length) {
  DCHECK(sid);


  bool result_found = false;
  base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, kGcpUsersRootKeyName);
  for (; iter.Valid(); ++iter) {
    const wchar_t* user_sid = iter.Name();
    wchar_t user_id[256];
    ULONG user_length = base::size(user_id);
    HRESULT hr = GetUserProperty(user_sid, kUserId, user_id, &user_length);
    if (SUCCEEDED(hr) && id == user_id) {
      // Make sure there are not 2 users with the same SID.
      if (result_found)
        return HRESULT_FROM_WIN32(ERROR_USER_EXISTS);

      wcsncpy_s(sid, length, user_sid, wcslen(user_sid));
      result_found = true;
    }
  }

  return result_found ? S_OK : HRESULT_FROM_WIN32(ERROR_NONE_MAPPED);
}

HRESULT GetIdFromSid(const wchar_t* sid, base::string16* id) {
  DCHECK(id);

  base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, kGcpUsersRootKeyName);
  for (; iter.Valid(); ++iter) {
    const wchar_t* user_sid = iter.Name();

    if (wcscmp(sid, user_sid) == 0) {
      wchar_t user_id[256];
      ULONG user_length = base::size(user_id);
      HRESULT hr = GetUserProperty(user_sid, kUserId, user_id, &user_length);
      if (SUCCEEDED(hr)) {
        *id = user_id;
        return S_OK;
      }
    }
  }
  return HRESULT_FROM_WIN32(ERROR_NONE_MAPPED);
}

HRESULT SetUserWinlogonUserListEntry(const base::string16& username,
                                     DWORD visible) {
  // Sets the value of the key that will hide the user from all credential
  // providers.
  base::win::RegKey key;
  LONG sts = key.Create(HKEY_LOCAL_MACHINE, kWinlogonUserListRegKey, KEY_WRITE);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  // The key is only set if we want to make a user invisible. The default
  // behavior is for the user to be visible so delete the key in this case.
  if (!visible) {
    sts = key.WriteValue(username.c_str(), visible);
  } else {
    sts = key.DeleteValue(username.c_str());
    if (sts == ERROR_FILE_NOT_FOUND)
      sts = ERROR_SUCCESS;
  }

  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  return S_OK;
}

HRESULT SetLogonUiUserTileEntry(const base::string16& sid, CLSID cp_guid) {
  return SetMachineRegString(kLogonUiUserTileRegKey, sid,
                             base::win::String16FromGUID(cp_guid));
}

HRESULT GetMachineGuid(base::string16* machine_guid) {
  // The machine guid is a unique identifier assigned to a computer on every
  // install of Windows. This guid can be used to uniquely identify this device
  // to various management services. The same guid is used to identify the
  // device to Chrome Browser Cloud Management. It is fetched in this file:
  // chrome/browser/policy/browser_dm_token_storage_win.cc:InitClientId.
  DCHECK(machine_guid);
  wchar_t machine_guid_buffer[64];
  ULONG guid_length = base::size(machine_guid_buffer);
  HRESULT hr = GetMachineRegString(kMicrosoftCryptographyRegKey,
                                   kMicrosoftCryptographyMachineGuidRegKey,
                                   machine_guid_buffer, &guid_length);

  if (SUCCEEDED(hr))
    *machine_guid = machine_guid_buffer;

  return hr;
}

HRESULT SetMachineGuidForTesting(const base::string16& machine_guid) {
  // Set a debug guid for the machine so that unit tests that override the
  // registry can run properly.
  return SetMachineRegString(kMicrosoftCryptographyRegKey,
                             kMicrosoftCryptographyMachineGuidRegKey,
                             machine_guid);
}

}  // namespace credential_provider
