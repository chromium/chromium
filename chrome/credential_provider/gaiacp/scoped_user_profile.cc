// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/scoped_user_profile.h"

#include <Windows.h>

#include <aclapi.h>
#include <atlcomcli.h>
#include <atlconv.h>
#include <dpapi.h>
#include <objidl.h>
#include <security.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <userenv.h>

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {

namespace {

// Retry count when attempting to determine if the user's OS profile has
// been created.  In slow envrionments, like VMs used for testing, it may
// take some time to create the OS profile so checks are done periodically.
// Ideally the OS would send out a notification when a profile is created and
// retrying would not be needed, but this notification does not exist.
const int kWaitForProfileCreationRetryCount = 30;

constexpr int kProfilePictureSizes[] = {32, 40, 48, 96, 192, 240, 448};

std::string GetEncryptedRefreshToken(
    base::win::ScopedHandle::Handle logon_handle,
    const base::Value& properties) {
  std::string refresh_token = GetDictStringUTF8(properties, kKeyRefreshToken);
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

HRESULT GetUserAccountPicturePath(const base::string16& sid,
                                  base::FilePath* base_path) {
  DCHECK(base_path);
  base_path->clear();
  LPWSTR path;
  HRESULT hr = ::SHGetKnownFolderPath(FOLDERID_PublicUserTiles, 0, NULL, &path);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SHGetKnownFolderPath=" << putHR(hr);
    return hr;
  }
  *base_path = base::FilePath(path).Append(sid);
  ::CoTaskMemFree(path);
  return S_OK;
}

base::FilePath GetUserSizedAccountPictureFilePath(
    const base::FilePath& account_picture_path,
    int size,
    const base::string16& picture_extension) {
  return account_picture_path.Append(base::StringPrintf(
      L"GoogleAccountPicture_%i%ls", size, picture_extension.c_str()));
}

using ImageProcessor =
    base::OnceCallback<HRESULT(const base::FilePath& picture_path,
                               const std::vector<char>& picture_buffer)>;

HRESULT SaveProcessedProfilePictureToDisk(
    const base::FilePath& picture_path,
    const std::vector<char>& picture_buffer,
    ImageProcessor processor_function) {
  DCHECK(processor_function);

  // Make the file visible in case it is hidden or else WriteFile will fail
  // to overwrite the existing file.
  DWORD file_attributes = ::GetFileAttributes(picture_path.value().c_str());
  if (file_attributes != INVALID_FILE_ATTRIBUTES) {
    if (!::SetFileAttributes(picture_path.value().c_str(),
                             file_attributes & ~FILE_ATTRIBUTE_HIDDEN)) {
      LOGFN(ERROR) << "SetFileAttributes(remove hidden) err="
                   << ::GetLastError();
    }
  }

  HRESULT hr = std::move(processor_function).Run(picture_path, picture_buffer);
  if (SUCCEEDED(hr)) {
    // Make the picture file hidden just like the system would normally.
    file_attributes = ::GetFileAttributes(picture_path.value().c_str());
    if (file_attributes != INVALID_FILE_ATTRIBUTES) {
      if (!::SetFileAttributes(picture_path.value().c_str(),
                               file_attributes | FILE_ATTRIBUTE_HIDDEN)) {
        LOGFN(ERROR) << "SetFileAttributes(add hidden) err="
                     << ::GetLastError();
      }
    }
  }

  return hr;
}

HRESULT CreateDirectoryWithRestrictedAccess(const base::FilePath& path) {
  if (base::PathExists(path))
    return S_OK;

  SECURITY_DESCRIPTOR sd;
  if (!::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
    LOGFN(ERROR) << "Failed to initialize sd hr="
                 << HRESULT_FROM_WIN32(::GetLastError());
    return E_FAIL;
  }

  PSID everyone_sid = nullptr;
  PSID creator_owner_sid = nullptr;
  PSID administrators_sid = nullptr;
  SID_IDENTIFIER_AUTHORITY everyone_sid_id = SECURITY_WORLD_SID_AUTHORITY;
  SID_IDENTIFIER_AUTHORITY creator_owner_sid_id =
      SECURITY_CREATOR_SID_AUTHORITY;
  SID_IDENTIFIER_AUTHORITY administrators_sid_id = SECURITY_NT_AUTHORITY;
  BYTE real_owner_sid[SECURITY_MAX_SID_SIZE];
  DWORD size_owner_sid = base::size(real_owner_sid);

  HRESULT hr = S_OK;

  // Get SIDs for Administrators, everyone, creator owner and local system.
  if (!::AllocateAndInitializeSid(&everyone_sid_id, 1, SECURITY_WORLD_RID, 0, 0,
                                  0, 0, 0, 0, 0, &everyone_sid) ||
      !::AllocateAndInitializeSid(&creator_owner_sid_id, 1,
                                  SECURITY_CREATOR_OWNER_RID, 0, 0, 0, 0, 0, 0,
                                  0, &creator_owner_sid) ||
      !::AllocateAndInitializeSid(
          &administrators_sid_id, 2, SECURITY_BUILTIN_DOMAIN_RID,
          DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administrators_sid) ||
      !::CreateWellKnownSid(WinLocalSystemSid, nullptr, real_owner_sid,
                            &size_owner_sid)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "Failed to get well known sids hr=" << putHR(hr);
  } else {
    std::vector<EXPLICIT_ACCESS> ea;

    // Only apply read access to the contents of the folder and not the folder
    // itself. If read access is given to the folder, then users will be able
    // to rename the folder (even though they are not allowed to delete it).
    ea.push_back({GENERIC_READ | STANDARD_RIGHTS_READ,
                  SET_ACCESS,
                  SUB_CONTAINERS_AND_OBJECTS_INHERIT | INHERIT_ONLY_ACE,
                  {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_SID,
                   TRUSTEE_IS_WELL_KNOWN_GROUP,
                   reinterpret_cast<wchar_t*>(everyone_sid)}});

    // Allow full access for administrators and creator owners.
    ea.push_back(
        {GENERIC_ALL | STANDARD_RIGHTS_ALL,
         SET_ACCESS,
         SUB_CONTAINERS_AND_OBJECTS_INHERIT,
         {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_SID, TRUSTEE_IS_GROUP,
          reinterpret_cast<wchar_t*>(administrators_sid)}});
    ea.push_back(
        {GENERIC_ALL | STANDARD_RIGHTS_ALL,
         SET_ACCESS,
         SUB_CONTAINERS_AND_OBJECTS_INHERIT,
         {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_SID, TRUSTEE_IS_USER,
          reinterpret_cast<wchar_t*>(creator_owner_sid)}});

    PACL acl = nullptr;
    DWORD err = ::SetEntriesInAcl(base::size(ea), ea.data(), nullptr, &acl);
    if (ERROR_SUCCESS != errno) {
      hr = HRESULT_FROM_WIN32(err);
      LOGFN(ERROR) << "Failed set sids in acl hr=" << putHR(hr);
    } else {
      // Add the ACL to the security descriptor.
      if (!::SetSecurityDescriptorDacl(&sd, TRUE, acl, FALSE)) {
        hr = HRESULT_FROM_WIN32(::GetLastError());
        LOGFN(ERROR) << "Failed to set dacl=" << path << " hr=" << putHR(hr);
      } else {
        // Make SYSTEM be the owner of this folder and all its children.
        if (!::SetSecurityDescriptorOwner(&sd, real_owner_sid, FALSE)) {
          hr = HRESULT_FROM_WIN32(::GetLastError());
          LOGFN(ERROR) << "Can't set owner sid hr=" << putHR(hr);
        } else {
          // Don't inherit ACE from parents.
          if (!::SetSecurityDescriptorControl(&sd, SE_DACL_PROTECTED,
                                              SE_DACL_PROTECTED)) {
            hr = HRESULT_FROM_WIN32(::GetLastError());
            LOGFN(ERROR) << "Failed to remove inheritance on descriptor hr="
                         << putHR(hr);
          } else {
            // Finally create the directory with the correct permissions.
            SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), &sd, FALSE};
            if (!::CreateDirectory(path.value().c_str(), &sa)) {
              hr = HRESULT_FROM_WIN32(::GetLastError());
              LOGFN(ERROR) << "Failed to create profile picture directory="
                           << path << " hr=" << putHR(hr);
            }
          }
        }
      }

      if (acl)
        ::LocalFree(acl);
    }

    if (everyone_sid)
      ::FreeSid(everyone_sid);
    if (creator_owner_sid)
      ::FreeSid(creator_owner_sid);
    if (administrators_sid)
      ::FreeSid(administrators_sid);
  }

  return hr;
}

HRESULT UpdateProfilePicturesForWindows8AndNewer(
    const base::string16& sid,
    const base::string16& picture_url,
    bool force_update) {
  DCHECK(!sid.empty());
  DCHECK(!picture_url.empty());
  DCHECK(base::win::GetVersion() >= base::win::Version::WIN8);

  // Try to download profile pictures of all required sizes for windows.
  // Needed profile picture sizes are in |kProfilePictureSizes|.
  // The way Windows8+ stores profile pictures is the following:
  // In |reg_utils.cc:kAccountPicturesRootRegKey| there is a registry key
  // for each resolution of profile picture needed. The keys are names
  // "Image[x]" where [x] is the resolution of the picture.
  // Each key points to a profile picture of the correct resolution on disk.
  // Generally the profile pictures are stored under:
  // FOLDERID_PublicUserTiles\\{user sid}

  base::string16 picture_url_path = base::UTF8ToUTF16(GURL(picture_url).path());
  if (picture_url_path.size() <= 1) {
    LOGFN(ERROR) << "Invalid picture url=" << picture_url;
    return E_FAIL;
  }

  base::FilePath account_picture_path;
  HRESULT hr = GetUserAccountPicturePath(sid, &account_picture_path);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "Failed to get account picture known folder=" << putHR(hr);
    return E_FAIL;
  }

  if (!base::PathExists(account_picture_path)) {
    HRESULT hr = CreateDirectoryWithRestrictedAccess(account_picture_path);
    if (FAILED(hr)) {
      LOGFN(ERROR) << "Failed to create profile picture directory="
                   << account_picture_path << " hr=" << putHR(hr);
      return hr;
    }
  }

  base::string16 base_picture_extension = kDefaultProfilePictureFileExtension;

  size_t last_period = picture_url_path.find_last_of('.');
  if (last_period != std::string::npos)
    base_picture_extension = picture_url_path.substr(last_period);

  for (auto image_size : kProfilePictureSizes) {
    base::FilePath target_picture_path = GetUserSizedAccountPictureFilePath(
        account_picture_path, image_size, base_picture_extension);
    bool needs_to_save_original =
        force_update || !base::PathExists(target_picture_path);

    // Skip if the file already exists and an update is not forced.
    if (!needs_to_save_original) {
      // Update the reg string for the image if it is not up to date.
      wchar_t old_picture_path[MAX_PATH];
      ULONG path_size = base::size(old_picture_path);
      HRESULT hr = GetAccountPictureRegString(sid, image_size, old_picture_path,
                                              &path_size);
      if (FAILED(hr) || target_picture_path.value() != old_picture_path) {
        HRESULT hr = SetAccountPictureRegString(sid, image_size,
                                                target_picture_path.value());
        if (FAILED(hr))
          LOGFN(ERROR) << "SetAccountPictureRegString(pic) hr=" << putHR(hr);
      }
      continue;
    }

    std::string current_picture_url =
        base::UTF16ToUTF8(picture_url) + base::StringPrintf("=s%i", image_size);

    auto fetcher = WinHttpUrlFetcher::Create(GURL(current_picture_url));
    if (!fetcher) {
      LOGFN(ERROR) << "Failed to create fetcher for=" << current_picture_url;
      continue;
    }

    std::vector<char> response;
    HRESULT hr = fetcher->Fetch(&response);
    if (FAILED(hr)) {
      LOGFN(INFO) << "fetcher.Fetch hr=" << putHR(hr);
      continue;
    }

    if (needs_to_save_original) {
      SaveProcessedProfilePictureToDisk(
          target_picture_path, response,
          base::BindOnce(
              [](const base::string16& sid, int image_size,
                 const base::FilePath& picture_path,
                 const std::vector<char>& picture_buffer) {
                HRESULT hr = S_OK;
                if (base::WriteFile(picture_path, picture_buffer.data(),
                                    picture_buffer.size()) !=
                    static_cast<int>(picture_buffer.size())) {
                  LOGFN(ERROR) << "Failed to write profile picture to file="
                               << picture_path;
                  hr = HRESULT_FROM_WIN32(::GetLastError());
                } else {
                  // Finally update the registry to point to this profile
                  // picture.
                  HRESULT reg_hr = SetAccountPictureRegString(
                      sid, image_size, picture_path.value());
                  if (FAILED(reg_hr))
                    LOGFN(ERROR) << "SetAccountPictureRegString(pic) hr="
                                 << putHR(reg_hr);
                }
                return hr;
              },
              sid, image_size));
    }
  }

  return S_OK;
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
    const base::string16& domain,
    const base::string16& username,
    const base::string16& password) {
  if (!GetCreatorFunctionStorage()->is_null())
    return GetCreatorFunctionStorage()->Run(sid, domain, username, password);

  std::unique_ptr<ScopedUserProfile> scoped(
      new ScopedUserProfile(sid, domain, username, password));
  return scoped->IsValid() ? std::move(scoped) : nullptr;
}

ScopedUserProfile::ScopedUserProfile(const base::string16& sid,
                                     const base::string16& domain,
                                     const base::string16& username,
                                     const base::string16& password) {
  LOGFN(INFO);
  // Load the user's profile so that their regsitry hive is available.
  base::win::ScopedHandle::Handle handle;

  if (!::LogonUserW(username.c_str(), domain.c_str(), password.c_str(),
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

HRESULT ScopedUserProfile::ExtractAssociationInformation(
    const base::Value& properties,
    base::string16* sid,
    base::string16* id,
    base::string16* email,
    base::string16* token_handle,
    base::string16* last_online_login_millis) {
  DCHECK(sid);
  DCHECK(id);
  DCHECK(email);
  DCHECK(token_handle);
  DCHECK(last_online_login_millis);

  *sid = GetDictString(properties, kKeySID);
  if (sid->empty()) {
    LOGFN(ERROR) << "SID is empty";
    return E_INVALIDARG;
  }

  *id = GetDictString(properties, kKeyId);
  if (id->empty()) {
    LOGFN(ERROR) << "Id is empty";
    return E_INVALIDARG;
  }

  *email = GetDictString(properties, kKeyEmail);
  if (email->empty()) {
    LOGFN(ERROR) << "Email is empty";
    return E_INVALIDARG;
  }

  *token_handle = GetDictString(properties, kKeyTokenHandle);
  if (token_handle->empty()) {
    LOGFN(ERROR) << "Token handle is empty";
    return E_INVALIDARG;
  }

  *last_online_login_millis =
      GetDictString(properties, kKeyLastSuccessfulOnlineLoginMillis);
  if (last_online_login_millis->empty()) {
    // This may return empty when there exists no successful login attempt.
    // Need not fail the call and instead fallback to returning S_OK.
    LOGFN(INFO) << "LastSuccessfulOnlineLoginMillis is empty";
    *last_online_login_millis = L"0";
  }

  return S_OK;
}

HRESULT ScopedUserProfile::RegisterAssociation(
    const base::string16& sid,
    const base::string16& id,
    const base::string16& email,
    const base::string16& token_handle,
    const base::string16& last_online_login_millis) {
  // Save token handle.  This handle will be used later to determine if the
  // the user has changed their password since the account was created.
  HRESULT hr = SetUserProperty(sid, kUserTokenHandle, token_handle);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(th) hr=" << putHR(hr);
    return hr;
  }

  hr = SetUserProperty(sid, kUserId, id);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(id) hr=" << putHR(hr);
    return hr;
  }

  hr = SetUserProperty(sid, kUserEmail, email);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(email) hr=" << putHR(hr);
    return hr;
  }

  hr = SetUserProperty(sid,
                       base::UTF8ToUTF16(kKeyLastSuccessfulOnlineLoginMillis),
                       last_online_login_millis);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "SetUserProperty(last_online_login_millis) hr="
                 << putHR(hr);
    return hr;
  }

  return S_OK;
}

HRESULT ScopedUserProfile::SaveAccountInfo(const base::Value& properties) {
  LOGFN(INFO);

  base::string16 sid;
  base::string16 id;
  base::string16 email;
  base::string16 token_handle;
  base::string16 last_online_login_millis;

  HRESULT hr = ExtractAssociationInformation(
      properties, &sid, &id, &email, &token_handle, &last_online_login_millis);
  if (FAILED(hr))
    return hr;

  hr = RegisterAssociation(sid, id, email, token_handle,
                           last_online_login_millis);
  if (FAILED(hr))
    return hr;

  // Write account information to the user's hive.
  // NOTE: regular users cannot access the registry entry of other users,
  // but administrators and SYSTEM can.
  {
    wchar_t key_name[128];
    swprintf_s(key_name, base::size(key_name), L"%s\\%s\\%s", sid.c_str(),
               kRegHkcuAccountsPath, id.c_str());
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

  // This code for setting profile pictures is specific for windows 8+.
  if (base::win::GetVersion() >= base::win::Version::WIN8) {
    base::string16 picture_url = GetDictString(properties, kKeyPicture);
    if (!picture_url.empty() && !sid.empty()) {
      wchar_t old_picture_url[512];
      ULONG url_size = base::size(old_picture_url);
      hr = GetUserProperty(sid, kUserPictureUrl, old_picture_url, &url_size);

      UpdateProfilePicturesForWindows8AndNewer(
          sid, picture_url, FAILED(hr) || old_picture_url != picture_url);
      hr = SetUserProperty(sid.c_str(), kUserPictureUrl, picture_url.c_str());
      if (FAILED(hr)) {
        LOGFN(ERROR) << "SetUserProperty(pic) hr=" << putHR(hr);
        return hr;
      }
    }
  }

  return S_OK;
}

ScopedUserProfile::ScopedUserProfile() {}

bool ScopedUserProfile::WaitForProfileCreation(const base::string16& sid) {
  LOGFN(INFO);
  wchar_t profile_dir[MAX_PATH];
  bool created = false;

  for (int i = 0; i < kWaitForProfileCreationRetryCount; ++i) {
    ::Sleep(1000);
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
             kRegHkcuAccountsPath);
  LOGFN(INFO) << "HKU\\" << key_name;

  for (int i = 0; i < kWaitForProfileCreationRetryCount; ++i) {
    ::Sleep(1000);
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
