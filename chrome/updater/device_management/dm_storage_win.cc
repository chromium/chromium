// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_storage.h"

#include <string>

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
      SetRegistryKey(HKEY_LOCAL_MACHINE, kRegKeyCompanyCloudManagement,
                     kRegValueEnrollmentToken, base::SysUTF8ToWide(token));

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
  std::wstring token;
  if (base::win::RegKey key;
      key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyCloudManagement,
               Wow6432(KEY_READ)) == ERROR_SUCCESS &&
      key.ReadValue(kRegValueEnrollmentToken, &token) == ERROR_SUCCESS) {
    return base::SysWideToUTF8(token);
  }

  if (base::win::RegKey key;
      key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyLegacyCloudManagement,
               Wow6432(KEY_READ)) == ERROR_SUCCESS &&
      key.ReadValue(kRegValueCloudManagementEnrollmentToken, &token) ==
          ERROR_SUCCESS) {
    return base::SysWideToUTF8(token);
  }
  return {};
}

bool TokenService::StoreDmToken(const std::string& token) {
  const bool result =
      SetRegistryKey(HKEY_LOCAL_MACHINE, kRegKeyCompanyEnrollment,
                     kRegValueDmToken, base::SysUTF8ToWide(token));
  VLOG(1) << "Update DM token to: [" << token << "], bool result=" << result;
  return result;
}

bool TokenService::DeleteDmToken() {
  base::win::RegKey key;
  auto result = key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyEnrollment,
                         Wow6432(KEY_SET_VALUE));

  // The registry key which stores the DMToken value was not found, so deletion
  // is not necessary.
  if (result == ERROR_FILE_NOT_FOUND) {
    return true;
  }

  if (result == ERROR_SUCCESS) {
    result = key.DeleteValue(kRegValueDmToken);
  } else {
    VLOG(1) << "Failed to delete DM token.";
    return false;
  }

  // Delete the key if no other values are present.
  base::win::RegKey(HKEY_LOCAL_MACHINE, L"", Wow6432(KEY_WRITE))
      .DeleteEmptyKey(kRegKeyCompanyEnrollment);
  VLOG(1) << "DM token is deleted.";
  return true;
}

std::string TokenService::GetDmToken() const {
  std::wstring token;
  base::win::RegKey key;
  if (key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyEnrollment,
               Wow6432(KEY_READ)) != ERROR_SUCCESS ||
      key.ReadValue(kRegValueDmToken, &token) != ERROR_SUCCESS) {
    return std::string();
  }

  return base::SysWideToUTF8(token);
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
