// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/reg_utils.h"

#include <Windows.h>

#include "base/stl_util.h"
#include "base/win/registry.h"
#include "chrome/credential_provider/gaiacp/gcp_strings.h"

namespace credential_provider {

namespace {

// Root registry key for GCP configuration and state.
#if defined(GOOGLE_CHROME_BUILD)
#define CREDENTIAL_PROVIDER_REGISTRY_KEY L"Software\\Google\\GCP"
#else
#define CREDENTIAL_PROVIDER_REGISTRY_KEY L"Software\\Chromium\\GCP"
#endif  // defined(GOOGLE_CHROME_BUILD)

const wchar_t kGcpRootKeyName[] = CREDENTIAL_PROVIDER_REGISTRY_KEY;

HRESULT GetRegDWORD(const base::string16& key_name,
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

HRESULT GetRegString(const base::string16& key_name,
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
  ULONG local_length = *length - 1;
  sts = key.ReadValue(name.c_str(), value, &local_length, &type);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);
  if (type != REG_SZ)
    return HRESULT_FROM_WIN32(ERROR_CANTREAD);

  value[local_length] = 0;
  *length = local_length;
  return S_OK;
}

HRESULT SetRegDWORD(const base::string16& key_name,
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

HRESULT SetRegString(const base::string16& key_name,
                     const base::string16& name,
                     const base::string16& value) {
  base::win::RegKey key;
  LONG sts = key.Create(HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_WRITE);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  if (value.empty()) {
    sts = key.DeleteValue(name.c_str());
  } else {
    sts = key.WriteValue(name.c_str(), value.c_str());
  }
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  return S_OK;
}

}  // namespace

HRESULT GetGlobalFlag(const base::string16& name, DWORD* value) {
  return GetRegDWORD(kGcpRootKeyName, name, value);
}

HRESULT GetGlobalFlag(const base::string16& name,
                      wchar_t* value,
                      ULONG* length) {
  return GetRegString(kGcpRootKeyName, name, value, length);
}

HRESULT GetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        DWORD* value) {
  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%s\\Users\\%s", kGcpRootKeyName,
             sid.c_str());
  return GetRegDWORD(key_name, name, value);
}

HRESULT GetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        wchar_t* value,
                        ULONG* length) {
  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%s\\Users\\%s", kGcpRootKeyName,
             sid.c_str());
  return GetRegString(key_name, name, value, length);
}

HRESULT SetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        DWORD value) {
  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%s\\Users\\%s", kGcpRootKeyName,
             sid.c_str());
  return SetRegDWORD(key_name, name, value);
}

HRESULT SetUserProperty(const base::string16& sid,
                        const base::string16& name,
                        const base::string16& value) {
  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%s\\Users\\%s", kGcpRootKeyName,
             sid.c_str());
  return SetRegString(key_name, name, value);
}

HRESULT RemoveAllUserProperties(const base::string16& sid) {
  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%s\\Users", kGcpRootKeyName);

  base::win::RegKey key;
  LONG sts = key.Open(HKEY_LOCAL_MACHINE, key_name, KEY_WRITE);
  if (sts != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(sts);

  sts = key.DeleteKey(sid.c_str());
  return sts != ERROR_SUCCESS ? HRESULT_FROM_WIN32(sts) : S_OK;
}

HRESULT GetUserTokenHandles(std::map<base::string16, base::string16>* handles) {
  DCHECK(handles);

  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%s\\Users", kGcpRootKeyName);

  base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, key_name);
  for (; iter.Valid(); ++iter) {
    const wchar_t* sid = iter.Name();
    wchar_t token_handle[256];
    ULONG length = base::size(token_handle);
    HRESULT hr = GetUserProperty(sid, kUserTokenHandle, token_handle, &length);
    if (SUCCEEDED(hr))
      handles->emplace(sid, token_handle);
  }
  return S_OK;
}

HRESULT GetSidFromId(const base::string16& id, wchar_t* sid, ULONG length) {
  DCHECK(sid);

  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%ls\\Users", kGcpRootKeyName);

  base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, key_name);
  for (; iter.Valid(); ++iter) {
    const wchar_t* user_sid = iter.Name();
    wchar_t user_id[256];
    ULONG user_length = base::size(user_id);
    HRESULT hr =
        GetUserProperty(user_sid, kUserTokenHandle, user_id, &user_length);
    if (SUCCEEDED(hr) && id == user_id) {
      wcsncpy_s(sid, length, user_sid, wcslen(user_sid));
      return S_OK;
    }
  }
  return HRESULT_FROM_WIN32(ERROR_NONE_MAPPED);
}

const wchar_t* GetUsersRootKeyForTesting() {
  return CREDENTIAL_PROVIDER_REGISTRY_KEY L"\\Users";
}

}  // namespace credential_provider
