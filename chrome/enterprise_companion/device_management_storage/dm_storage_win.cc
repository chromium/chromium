// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"

#include <string>
#include <vector>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace device_management_storage {
namespace {

// Registry for device ID.
constexpr wchar_t kRegKeyCryptographyKey[] =
    L"SOFTWARE\\Microsoft\\Cryptography\\";
constexpr wchar_t kRegValueMachineGuid[] = L"MachineGuid";

bool ReadRegistryString(const std::wstring& key_path,
                        const std::wstring& name,
                        REGSAM reg_view,
                        std::string& value) {
  std::wstring data;
  const LONG result = base::win::RegKey(HKEY_LOCAL_MACHINE, key_path.c_str(),
                                        reg_view | KEY_READ)
                          .ReadValue(name.c_str(), &data);
  if (result != ERROR_SUCCESS) {
    VLOG(1) << __func__ << ": failed to read registry: " << key_path << "@"
            << name;
    return false;
  }
  value = base::SysWideToUTF8(data);
  return true;
}

bool DeleteRegistryValue(const std::wstring& key_path,
                         const std::wstring& name,
                         REGSAM reg_view) {
  base::win::RegKey key;
  if (const LONG result = key.Open(HKEY_LOCAL_MACHINE, key_path.c_str(),
                                   reg_view | KEY_SET_VALUE);
      result != ERROR_SUCCESS) {
    return result == ERROR_FILE_NOT_FOUND;
  }
  if (const LONG result = key.DeleteValue(name.c_str());
      result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
    VLOG(1) << "Failed to delete registry: " << key_path << "@" << name;
    return false;
  }
  return true;
}

// Reads a token from the registry value of string type .
bool ReadTokenString(const std::wstring& key_path,
                     const std::wstring& name,
                     REGSAM reg_view,
                     std::string& token) {
  if (!ReadRegistryString(key_path, name, reg_view, token)) {
    return false;
  }
  if (token.size() > DMStorage::kMaxDmTokenLength) {
    token.clear();
    VLOG(1) << __func__ << ": token [" << token << "] is too long.";
    return false;
  }
  return true;
}

// Reads a token from the registry value of type REG_BINARY.
bool ReadTokenBinary(const std::wstring& key_path,
                     const std::wstring& name,
                     REGSAM reg_view,
                     std::string& token) {
  base::win::RegKey key;
  if (key.Open(HKEY_LOCAL_MACHINE, key_path.c_str(), reg_view | KEY_READ) !=
      ERROR_SUCCESS) {
    return false;
  }
  DWORD size = 0;
  DWORD type = 0;
  LONG error = key.ReadValue(name.c_str(), nullptr, &size, &type);
  if (error != ERROR_SUCCESS) {
    VLOG(2) << "ReadValue failed: " << error;
    return false;
  }
  if (size == 0) {
    VLOG(2) << "The token is empty.";
    return false;
  }
  if (size > DMStorage::kMaxDmTokenLength) {
    VLOG(2) << "Value is too large: " << size;
    return false;
  }
  if (type != REG_BINARY) {
    VLOG(2) << "Ignored token value with incompatible type.";
    return false;
  }
  std::vector<char> value(size);
  error = key.ReadValue(name.c_str(), &value.front(), &size, &type);
  if (error != ERROR_SUCCESS) {
    VLOG(2) << "ReadValue failed: " << error;
    return false;
  }
  token.assign(value.begin(), value.end());
  return true;
}

// Writes a token as a binary value to the registry.
bool WriteTokenBinary(const std::wstring& key_path,
                      const std::wstring& name,
                      REGSAM reg_view,
                      const std::string& token) {
  if (token.size() > DMStorage::kMaxDmTokenLength) {
    VLOG(2) << "Value is too large: " << token.size();
    return false;
  }

  base::win::RegKey key;
  LONG error =
      key.Create(HKEY_LOCAL_MACHINE, key_path.c_str(), reg_view | KEY_WRITE);
  if (error != ERROR_SUCCESS) {
    VLOG(1) << "Failed to open " << key_path << ": " << error;
    return false;
  }

  error = key.WriteValue(name.c_str(), token.data(), token.size(), REG_BINARY);
  if (error != ERROR_SUCCESS) {
    VLOG(1) << "Failed to write " << key_path << " @ " << name
            << " (binary): " << error;
  }
  return error == ERROR_SUCCESS;
}

class TokenService : public TokenServiceInterface {
 public:
  TokenService() = default;
  ~TokenService() override = default;

  // Overrides for TokenServiceInterface.
  std::string GetDeviceID() const override;
  bool IsEnrollmentMandatory() const override;
  bool StoreEnrollmentToken(const std::string& enrollment_token) override;
  bool DeleteEnrollmentToken() override;
  std::string GetEnrollmentToken() const override;
  bool StoreDmToken(const std::string& dm_token) override;
  bool DeleteDmToken() override;
  std::string GetDmToken() const override;
};

std::string TokenService::GetDeviceID() const {
  std::string device_id;
  if (!ReadRegistryString(kRegKeyCryptographyKey, kRegValueMachineGuid,
                          KEY_READ | KEY_WOW64_64KEY, device_id)) {
    return {};
  }
  return device_id;
}

bool TokenService::IsEnrollmentMandatory() const {
  DWORD is_mandatory = 0;
  base::win::RegKey key;
  return (key.Open(HKEY_LOCAL_MACHINE, updater::kRegKeyCompanyCloudManagement,
                   updater::Wow6432(KEY_READ)) == ERROR_SUCCESS &&
          key.ReadValueDW(updater::kRegValueEnrollmentMandatory,
                          &is_mandatory) == ERROR_SUCCESS)
             ? is_mandatory
             : false;
}

bool TokenService::StoreEnrollmentToken(const std::string& token) {
  const bool result = updater::SetRegistryKey(
      HKEY_LOCAL_MACHINE, updater::kRegKeyCompanyCloudManagement,
      updater::kRegValueEnrollmentToken, base::SysUTF8ToWide(token));
  VLOG(1) << "Update enrollment token to: [" << token
          << "], bool result=" << result;
  return result;
}

bool TokenService::DeleteEnrollmentToken() {
  VLOG(1) << __func__;
  return DeleteRegistryValue(updater::kRegKeyCompanyCloudManagement,
                             updater::kRegValueEnrollmentToken,
                             KEY_WOW64_32KEY) &&
         DeleteRegistryValue(updater::kRegKeyCompanyLegacyCloudManagement,
                             updater::kRegValueCloudManagementEnrollmentToken,
                             KEY_WOW64_32KEY);
}

std::string TokenService::GetEnrollmentToken() const {
  std::string token;
  if (ReadTokenString(updater::kRegKeyCompanyCloudManagement,
                      updater::kRegValueEnrollmentToken, KEY_WOW64_32KEY,
                      token)) {
    return token;
  }
  for (const std::wstring& key_path :
       {std::wstring(updater::kRegKeyCompanyLegacyCloudManagement),
        updater::GetAppClientsKey(UPDATER_APPID),
        updater::GetAppClientsKey(updater::kLegacyGoogleUpdateAppID)}) {
    if (ReadTokenString(key_path,
                        updater::kRegValueCloudManagementEnrollmentToken,
                        KEY_WOW64_32KEY, token)) {
      return token;
    }
  }
  return {};
}

bool TokenService::StoreDmToken(const std::string& token) {
  VLOG(1) << __func__ << ": [" << token << "]";
  return WriteTokenBinary(updater::kRegKeyCompanyEnrollment,
                          updater::kRegValueDmToken, KEY_WOW64_32KEY, token) &&
         WriteTokenBinary(updater::kRegKeyCompanyLegacyEnrollment,
                          updater::kRegValueDmToken, KEY_WOW64_64KEY, token);
}

bool TokenService::DeleteDmToken() {
  VLOG(1) << __func__;
  return DeleteRegistryValue(updater::kRegKeyCompanyEnrollment,
                             updater::kRegValueDmToken, KEY_WOW64_32KEY) &&
         DeleteRegistryValue(updater::kRegKeyCompanyLegacyEnrollment,
                             updater::kRegValueDmToken, KEY_WOW64_64KEY);
}

std::string TokenService::GetDmToken() const {
  std::string token;
  if (ReadTokenBinary(updater::kRegKeyCompanyEnrollment,
                      updater::kRegValueDmToken, KEY_WOW64_32KEY, token) ||
      ReadTokenBinary(updater::kRegKeyCompanyLegacyEnrollment,
                      updater::kRegValueDmToken, KEY_WOW64_64KEY, token)) {
    return token;
  }
  VLOG(1) << __func__ << ": token not found.";
  return {};
}

}  // namespace

scoped_refptr<DMStorage> CreateDMStorage(
    const base::FilePath& policy_cache_root) {
  return CreateDMStorage(policy_cache_root, std::make_unique<TokenService>());
}

scoped_refptr<DMStorage> GetDefaultDMStorage() {
  base::FilePath program_filesx86_dir;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILESX86,
                              &program_filesx86_dir)) {
    return nullptr;
  }

  return CreateDMStorage(
      program_filesx86_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
          .AppendASCII("Policies"));
}

}  // namespace device_management_storage
