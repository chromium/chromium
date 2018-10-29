// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/scoped_user_profile.h"

#include <Windows.h>
#include <dpapi.h>
#include <security.h>
#include <userenv.h>
#include <atlconv.h>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/credential_provider/gaiacp/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {

namespace {

// Registry key under HKCU to write account info into.
#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kRegAccountsPath[] = L"Software\\Google\\Accounts";
#else
const wchar_t kRegAccountsPath[] = L"Software\\Chromium\\Accounts";
#endif  // defined(GOOGLE_CHROME_BUILD)

std::string GetEncryptedRefreshToken(
    base::win::ScopedHandle::Handle logon_handle,
    const base::DictionaryValue& properties) {
  std::string refresh_token = GetDictStringUTF8(&properties, kKeyRefreshToken);
  if (refresh_token.empty()) {
    LOGFN(ERROR) << "Refresh token is empty";
    return std::string();
  }

  if (!::ImpersonateLoggedOnUser(logon_handle)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "ImpersonateLoggedOnUser hr=" << putHR(hr);
    return std::string();
  }

  // Don't include null character in ciphertext.
  DATA_BLOB plaintext;
  plaintext.pbData =
      reinterpret_cast<BYTE*>(const_cast<char*>(refresh_token.c_str()));
  plaintext.cbData = static_cast<DWORD>(refresh_token.length());

  DATA_BLOB ciphertext;
  BOOL success =
      ::CryptProtectData(&plaintext, L"Gaia refresh token", nullptr, nullptr,
                         nullptr, CRYPTPROTECT_UI_FORBIDDEN, &ciphertext);
  HRESULT hr = success ? S_OK : HRESULT_FROM_WIN32(::GetLastError());
  ::RevertToSelf();
  if (!success) {
    LOGFN(ERROR) << "CryptProtectData hr=" << putHR(hr);
    return std::string();
  }

  // NOTE: return value is binary data, not null-terminate string.
  std::string encrypted_data(reinterpret_cast<char*>(ciphertext.pbData),
                             ciphertext.cbData);
  ::LocalFree(ciphertext.pbData);
  return encrypted_data;
}

}  // namespace

// static
ScopedUserProfile::CreatorCallback*
ScopedUserProfile::GetCreatorFunctionStorage() {
  static CreatorCallback creator_for_testing;
  return &creator_for_testing;
}

// static
std::unique_ptr<ScopedUserProfile> ScopedUserProfile::Create(
    const base::string16& sid,
    const base::string16& username,
    const base::string16& password) {
  if (!GetCreatorFunctionStorage()->is_null())
    return GetCreatorFunctionStorage()->Run(sid, username, password);

  std::unique_ptr<ScopedUserProfile> scoped(
      new ScopedUserProfile(sid, username, password));
  return scoped->IsValid() ? std::move(scoped) : nullptr;
}

ScopedUserProfile::ScopedUserProfile(const base::string16& sid,
                                     const base::string16& username,
                                     const base::string16& password) {
  LOGFN(INFO);
  // Load the user's profile so that their regsitry hive is available.
  base::win::ScopedHandle::Handle handle;
  if (!::LogonUserW(username.c_str(), L".", password.c_str(),
                    LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT,
                    &handle)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "LogonUserW hr=" << putHR(hr);
    return;
  }
  token_.Set(handle);

  if (!WaitForProfileCreation(sid))
    token_.Close();
}

ScopedUserProfile::~ScopedUserProfile() {}

bool ScopedUserProfile::IsValid() {
  return token_.IsValid();
}

HRESULT ScopedUserProfile::SaveAccountInfo(
    const base::DictionaryValue& properties) {
  LOGFN(INFO);

  base::string16 sid = GetDictString(&properties, kKeySID);
  if (sid.empty()) {
    LOGFN(ERROR) << "SID is empty";
    return E_INVALIDARG;
  }

  base::string16 id = GetDictString(&properties, kKeyId);
  if (id.empty()) {
    LOGFN(ERROR) << "Id is empty";
    return E_INVALIDARG;
  }

  base::string16 email = GetDictString(&properties, kKeyEmail);
  if (email.empty()) {
    LOGFN(ERROR) << "Email is empty";
    return E_INVALIDARG;
  }

  base::string16 token_handle = GetDictString(&properties, kKeyTokenHandle);
  if (token_handle.empty()) {
    LOGFN(ERROR) << "Token handle is empty";
    return E_INVALIDARG;
  }

  // Save token handle.  This handle will be used later to determine if the
  // the user has changed their password since the account was created.
  HRESULT hr = SetUserProperty(sid.c_str(), kUserTokenHandle,
                               token_handle.c_str());
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(th) hr=" << putHR(hr);
    return hr;
  }

  hr = SetUserProperty(sid.c_str(), kUserId, id.c_str());
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(id) hr=" << putHR(hr);
    return hr;
  }

  hr = SetUserProperty(sid.c_str(), kUserEmail, email.c_str());
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(email) hr=" << putHR(hr);
    return hr;
  }

  // Write account information to the user's hive.
  // NOTE: regular users cannot access the registry entry of other users,
  // but administrators and SYSTEM can.
  {
    wchar_t key_name[128];
    swprintf_s(key_name, base::size(key_name), L"%s\\%s\\%s", sid.c_str(),
               kRegAccountsPath, id.c_str());
    LOGFN(INFO) << "HKU\\" << key_name;

    base::win::RegKey key;
    LONG sts = key.Create(HKEY_USERS, key_name, KEY_READ | KEY_WRITE);
    if (sts != ERROR_SUCCESS) {
      HRESULT hr = HRESULT_FROM_WIN32(sts);
      LOGFN(ERROR) << "key.Create(" << id << ") hr=" << putHR(hr);
      return hr;
    }

    sts = key.WriteValue(base::ASCIIToUTF16(kKeyEmail).c_str(), email.c_str());
    if (sts != ERROR_SUCCESS) {
      HRESULT hr = HRESULT_FROM_WIN32(sts);
      LOGFN(ERROR) << "key.WriteValue(" << sid << ", email) hr=" << putHR(hr);
      return hr;
    }

    // NOTE: |encrypted_data| is binary data, not null-terminate string.
    std::string encrypted_data =
        GetEncryptedRefreshToken(token_.Get(), properties);
    if (encrypted_data.empty()) {
      LOGFN(ERROR) << "GetEncryptedRefreshToken returned empty string";
      return E_UNEXPECTED;
    }

    sts = key.WriteValue(
        base::ASCIIToUTF16(kKeyRefreshToken).c_str(), encrypted_data.c_str(),
        static_cast<ULONG>(encrypted_data.length()), REG_BINARY);
    if (sts != ERROR_SUCCESS) {
      HRESULT hr = HRESULT_FROM_WIN32(sts);
      LOGFN(ERROR) << "key.WriteValue(" << sid << ", RT) hr=" << putHR(hr);
      return hr;
    }
  }

  return S_OK;
}

ScopedUserProfile::ScopedUserProfile() {}

bool ScopedUserProfile::WaitForProfileCreation(const base::string16& sid) {
  LOGFN(INFO);
  wchar_t profile_dir[MAX_PATH];
  bool created = false;

  for (int i = 0; i < 10; ++i) {
    Sleep(1000);
    DWORD length = base::size(profile_dir);
    if (::GetUserProfileDirectoryW(token_.Get(), profile_dir, &length)) {
      LOGFN(INFO) << "GetUserProfileDirectoryW " << i << " " << profile_dir;
      created = true;
      break;
    } else {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(INFO) << "GetUserProfileDirectoryW hr=" << putHR(hr);
    }
  }

  if (!created)
    LOGFN(INFO) << "Profile not created yet???";

  created = false;

  // Write account information to the user's hive.
  // NOTE: regular users cannot access the registry entry of other users,
  // but administrators and SYSTEM can.
  base::win::RegKey key;
  wchar_t key_name[128];
  swprintf_s(key_name, base::size(key_name), L"%s\\%s", sid.c_str(),
             kRegAccountsPath);
  LOGFN(INFO) << "HKU\\" << key_name;

  for (int i = 0; i < 10; ++i) {
    Sleep(1000);
    LONG sts = key.Create(HKEY_USERS, key_name, KEY_READ | KEY_WRITE);
    if (sts == ERROR_SUCCESS) {
      LOGFN(INFO) << "Registry hive created " << i;
      created = true;
      break;
    }
  }

  if (!created)
    LOGFN(ERROR) << "Profile not created really???";

  return created;
}

}  // namespace credential_provider
