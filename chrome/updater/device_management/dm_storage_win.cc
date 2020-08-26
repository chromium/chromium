// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_storage.h"

#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/updater/win/constants.h"

namespace updater {

namespace {

// Registry for device ID.
constexpr base::char16 kRegKeyCryptographyKey[] =
    L"SOFTWARE\\Microsoft\\Cryptography\\";
constexpr base::char16 kRegValueMachineGuid[] = L"MachineGuid";

// Registry for enrollment token.
constexpr base::char16 kRegKeyCompanyCloudManagement[] =
    COMPANY_POLICIES_KEY L"CloudManagement\\";
constexpr base::char16 kRegValueEnrollmentToken[] = L"EnrollmentToken\\";

// Registry for DM token.
constexpr base::char16 kRegKeyCompanyEnrollment[] =
    COMPANY_KEY L"Enrollment\\";
constexpr base::char16 kRegValueDmToken[] = L"dmtoken";

class TokenService : public TokenServiceInterface {
 public:
  TokenService() = default;
  ~TokenService() override = default;

  // Overrides for TokenServiceInterface.
  std::string GetDeviceID() const override;
  bool StoreEnrollmentToken(const std::string& enrollment_token) override;
  std::string GetEnrollmentToken() const override;
  bool StoreDmToken(const std::string& dm_token) override;
  std::string GetDmToken() const override;
};

std::string TokenService::GetDeviceID() const {
  base::string16 device_id;
  base::win::RegKey key;
  key.Open(HKEY_LOCAL_MACHINE, kRegKeyCryptographyKey,
           KEY_READ | KEY_WOW64_64KEY);
  if (key.ReadValue(kRegValueMachineGuid, &device_id) != ERROR_SUCCESS)
    return std::string();

  return base::SysWideToUTF8(device_id);
}

bool TokenService::StoreEnrollmentToken(const std::string& token) {
  base::win::RegKey key;
  key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyCloudManagement, KEY_WRITE);
  return key.WriteValue(kRegValueEnrollmentToken,
                        base::SysUTF8ToWide(token).c_str()) == ERROR_SUCCESS;
}

std::string TokenService::GetEnrollmentToken() const {
  base::string16 token;
  base::win::RegKey key;
  key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyCloudManagement, KEY_READ);
  if (key.ReadValue(kRegValueEnrollmentToken, &token) != ERROR_SUCCESS)
    return std::string();

  return base::SysWideToUTF8(token);
}

bool TokenService::StoreDmToken(const std::string& token) {
  base::win::RegKey key;
  key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyEnrollment, KEY_WRITE);
  return key.WriteValue(kRegValueDmToken, base::SysUTF8ToWide(token).c_str()) ==
         ERROR_SUCCESS;
}

std::string TokenService::GetDmToken() const {
  base::string16 token;
  base::win::RegKey key;
  key.Open(HKEY_LOCAL_MACHINE, kRegKeyCompanyEnrollment, KEY_READ);
  if (key.ReadValue(kRegValueDmToken, &token) != ERROR_SUCCESS)
    return std::string();

  return base::SysWideToUTF8(token);
}

}  // namespace

DMStorage::DMStorage(const base::FilePath& policy_cache_root)
    : DMStorage(policy_cache_root, std::make_unique<TokenService>()) {}

}  // namespace updater
