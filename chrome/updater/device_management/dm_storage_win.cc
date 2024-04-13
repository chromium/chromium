// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_storage.h"

#include <string>
#include <vector>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {
namespace {

// Registry for device ID.
constexpr wchar_t kRegKeyCryptographyKey[] =
    L"SOFTWARE\\Microsoft\\Cryptography\\";
constexpr wchar_t kRegValueMachineGuid[] = L"MachineGuid";

bool ReadTokenBinary(const base::win::RegKey& key,
                     const wchar_t* name,
                     std::string& token) {
  VLOG(2) << __func__;
  DWORD size = 0;
  DWORD type = 0;
  LONG error = key.ReadValue(name, nullptr, &size, &type);
  if (error != ERROR_SUCCESS) {
    VLOG(2) << "ReadValue failed: " << error;
    return false;
  }
  if (size > DMStorage::kMaxDmTokenLength) {
    VLOG(2) << "Value is too large: " << size;
    return false;
  }
  std::vector<char> value(size);
  error = key.ReadValue(name, &value.front(), &size, &type);
  if (error != ERROR_SUCCESS) {
    VLOG(2) << "ReadValue failed: " << error;
    return false;
  }
  token.assign(value.begin(), value.end());
  return true;
}

bool WriteTokenBinary(base::win::RegKey& key,
                      const wchar_t* name,
                      const std::string& token) {
  VLOG(2) << __func__;
  if (token.size() > DMStorage::kMaxDmTokenLength) {
    VLOG(2) << "Value is too large: " << token.size();
    return false;
  }
  const LONG error =
      key.WriteValue(name, token.data(), token.size(), REG_BINARY);
  if (error != ERROR_SUCCESS) {
    VLOG(2) << "WriteValue failed: " << error;
    return false;
  }
  return true;
}

// Set `name` in `root`\`key` as a binary `value`.
bool SetRegistryKeyBinary(HKEY root,
                          const std::wstring& key,
                          const std::wstring& name,
                          const std::string& value) {
  base::win::RegKey rkey;
  LONG error = rkey.Create(root, key.c_str(), Wow6432(KEY_WRITE));
  if (error != ERROR_SUCCESS) {
    VLOG(1) << "Failed to open (" << root << ") " << key << ": " << error;
    return false;
  }
  error = rkey.WriteValue(name.c_str(), value.data(), value.size(), REG_BINARY);
  if (error != ERROR_SUCCESS) {
    VLOG(1) << "Failed to write (" << root << ") " << key << " @ " << name
            << " (binary): " << error;
    return false;
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
  std::wstring device_id;
  base::win::RegKey key;
  if (key.Open(HKEY_LOCAL_MACHINE, kRegKeyCryptographyKey,
               KEY_READ | KEY_WOW64_64KEY) != ERROR_SUCCESS ||
      key.ReadValue(kRegValueMachineGuid, &device_id) != ERROR_SUCCESS) {
    return std::string();
  }

  return base::SysWideToUTF8(device_id);
}

bool TokenService::IsEnrollmentMandatory() const {
  DWORD is_mandatory = 0;
  base::win::RegKey key;
  return (key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyCloudManagement,
                   Wow6432(KEY_READ)) == ERROR_SUCCESS &&
          key.ReadValueDW(kRegValueEnrollmentMandatory, &is_mandatory) ==
              ERROR_SUCCESS)
             ? is_mandatory
             : false;
}

bool TokenService::StoreEnrollmentToken(const std::string& token) {
  const bool result =
      SetRegistryKeyBinary(HKEY_LOCAL_MACHINE, kRegKeyCompanyCloudManagement,
                           kRegValueEnrollmentToken, token);
  VLOG(1) << "Update enrollment token to: [" << token
          << "], bool result=" << result;
  return result;
}

bool TokenService::DeleteEnrollmentToken() {
  VLOG(1) << __func__;
  return DeleteRegValue(HKEY_LOCAL_MACHINE, kRegKeyCompanyCloudManagement,
                        kRegValueEnrollmentToken) &&
         DeleteRegValue(HKEY_LOCAL_MACHINE, kRegKeyCompanyLegacyCloudManagement,
                        kRegValueCloudManagementEnrollmentToken);
}

std::string TokenService::GetEnrollmentToken() const {
  std::string token;
  if (base::win::RegKey key;
      key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyCloudManagement,
               Wow6432(KEY_READ)) == ERROR_SUCCESS &&
      ReadTokenBinary(key, kRegValueEnrollmentToken, token)) {
    return token;
  }
  if (base::win::RegKey key;
      key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyLegacyCloudManagement,
               Wow6432(KEY_READ)) == ERROR_SUCCESS &&
      ReadTokenBinary(key, kRegValueCloudManagementEnrollmentToken, token)) {
    return token;
  }
  return {};
}

bool TokenService::StoreDmToken(const std::string& token) {
  if (!SetRegistryKeyBinary(HKEY_LOCAL_MACHINE, kRegKeyCompanyEnrollment,
                            kRegValueDmToken, token)) {
    VLOG(1) << "Failed to write DM token.";
    return false;
  }
  base::win::RegKey legacy_key;
  if (legacy_key.Create(HKEY_LOCAL_MACHINE, kRegKeyCompanyLegacyEnrollment,
                        KEY_WOW64_64KEY | KEY_WRITE) != ERROR_SUCCESS ||
      !WriteTokenBinary(legacy_key, kRegValueDmToken, token)) {
    VLOG(1) << "Failed to write DM token at the legacy place.";
    return false;
  }
  VLOG(1) << "Updated DM token to: [" << token << "]";
  return true;
}

bool TokenService::DeleteDmToken() {
  if (!DeleteRegValue(HKEY_LOCAL_MACHINE, kRegKeyCompanyEnrollment,
                      kRegValueDmToken)) {
    VLOG(1) << "Failed to delete DM token.";
    return false;
  }

  base::win::RegKey legacy_dm_key(HKEY_LOCAL_MACHINE,
                                  kRegKeyCompanyLegacyEnrollment,
                                  KEY_WOW64_64KEY | KEY_READ | KEY_WRITE);
  if (legacy_dm_key.Valid()) {
    LONG result = legacy_dm_key.DeleteValue(kRegValueDmToken);
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
      VLOG(1) << "Failed to delete DM token from the legacy place.";
      return false;
    }
  }

  VLOG(1) << __func__ << ": success.";
  return true;
}

std::string TokenService::GetDmToken() const {
  std::string token;
  if (base::win::RegKey key;
      key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyEnrollment,
               Wow6432(KEY_READ)) == ERROR_SUCCESS &&
      ReadTokenBinary(key, kRegValueDmToken, token)) {
    return token;
  }
  if (base::win::RegKey key;
      key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyLegacyEnrollment,
               KEY_WOW64_64KEY | KEY_READ) == ERROR_SUCCESS &&
      ReadTokenBinary(key, kRegValueDmToken, token)) {
    return token;
  }
  return {};
}

}  // namespace

DMStorage::DMStorage(const base::FilePath& policy_cache_root)
    : DMStorage(policy_cache_root, std::make_unique<TokenService>()) {}

scoped_refptr<DMStorage> GetDefaultDMStorage() {
  base::FilePath program_filesx86_dir;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILESX86,
                              &program_filesx86_dir)) {
    return nullptr;
  }

  return base::MakeRefCounted<DMStorage>(
      program_filesx86_dir.AppendASCII(COMPANY_SHORTNAME_STRING)
          .AppendASCII("Policies"));
}

}  // namespace updater
