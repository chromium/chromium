// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_storage.h"

#import <Foundation/Foundation.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/mac_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace updater {
namespace {

bool LoadEnrollmentTokenFromPolicy(std::string* enrollment_token) {
  const CFStringRef kEnrollmentTokenKey = CFSTR("EnrollmentToken");
  const CFStringRef kBrowserBundleId =
      CFSTR(MAC_BROWSER_BUNDLE_IDENTIFIER_STRING);

  base::ScopedCFTypeRef<CFPropertyListRef> token_value(
      CFPreferencesCopyAppValue(kEnrollmentTokenKey, kBrowserBundleId));
  if (!token_value || CFGetTypeID(token_value) != CFStringGetTypeID() ||
      !CFPreferencesAppValueIsForced(kEnrollmentTokenKey, kBrowserBundleId)) {
    return false;
  }

  CFStringRef value_string = base::mac::CFCast<CFStringRef>(token_value);
  if (!value_string)
    return false;

  *enrollment_token = base::SysCFStringRefToUTF8(value_string);
  return true;
}

// Enrollment token path:
//   /Library/Google/Chrome/CloudManagementEnrollmentToken.
base::FilePath GetEnrollmentTokenFilePath() {
  base::FilePath lib_path;
  if (!base::mac::GetLocalDirectory(NSLibraryDirectory, &lib_path)) {
    VLOG(1) << "Failed to get local library path.";
    return base::FilePath();
  }

  return lib_path.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(BROWSER_NAME_STRING)
      .AppendASCII("CloudManagementEnrollmentToken");
}

// DM token path:
//   /Library/Application Support/Google/CloudManagement.
base::FilePath GetDmTokenFilePath() {
  base::FilePath app_path;
  if (!base::mac::GetLocalDirectory(NSApplicationSupportDirectory, &app_path)) {
    VLOG(1) << "Failed to get Application support path.";
    return base::FilePath();
  }

  return app_path.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII("CloudManagement");
}

bool LoadTokenFromFile(const base::FilePath& token_file_path,
                       std::string* token) {
  std::string token_value;
  if (token_file_path.empty() ||
      !base::ReadFileToString(token_file_path, &token_value)) {
    return false;
  }

  *token = std::string(base::TrimWhitespaceASCII(token_value, base::TRIM_ALL));
  return true;
}

class TokenService : public TokenServiceInterface {
 public:
  TokenService();
  ~TokenService() override = default;

  // Overrides for TokenServiceInterface.
  std::string GetDeviceID() const override { return device_id_; }
  bool IsEnrollmentMandatory() const override { return false; }
  bool StoreEnrollmentToken(const std::string& enrollment_token) override;
  std::string GetEnrollmentToken() const override { return enrollment_token_; }
  bool StoreDmToken(const std::string& dm_token) override;
  bool DeleteDmToken() override;
  std::string GetDmToken() const override { return dm_token_; }

 private:
  // Cached values in memory.
  std::string device_id_;
  std::string enrollment_token_;
  std::string dm_token_;
};

TokenService::TokenService() {
  device_id_ = base::mac::GetPlatformSerialNumber();
  std::string enrollment_token;
  if (LoadEnrollmentTokenFromPolicy(&enrollment_token) ||
      LoadTokenFromFile(GetEnrollmentTokenFilePath(), &enrollment_token)) {
    enrollment_token_ = enrollment_token;
  }

  std::string dm_token;
  if (LoadTokenFromFile(GetDmTokenFilePath(), &dm_token)) {
    dm_token_ = dm_token;
  }
}

bool TokenService::StoreEnrollmentToken(const std::string& enrollment_token) {
  const base::FilePath enrollment_token_path = GetEnrollmentTokenFilePath();
  if (enrollment_token_path.empty() ||
      !base::CreateDirectory(enrollment_token_path.DirName()) ||
      !base::ImportantFileWriter::WriteFileAtomically(enrollment_token_path,
                                                      enrollment_token)) {
    return false;
  }

  enrollment_token_ = enrollment_token;
  return true;
}

bool TokenService::StoreDmToken(const std::string& token) {
  const base::FilePath dm_token_path = GetDmTokenFilePath();
  if (dm_token_path.empty() ||
      !base::CreateDirectory(dm_token_path.DirName()) ||
      !base::ImportantFileWriter::WriteFileAtomically(dm_token_path, token)) {
    return false;
  }
  dm_token_ = token;
  return true;
}

bool TokenService::DeleteDmToken() {
  const base::FilePath dm_token_path = GetDmTokenFilePath();
  if (dm_token_path.empty() || !base::DeleteFile(dm_token_path)) {
    return false;
  }
  dm_token_.clear();
  return true;
}

}  // namespace

DMStorage::DMStorage(const base::FilePath& policy_cache_root)
    : DMStorage(policy_cache_root, std::make_unique<TokenService>()) {}

scoped_refptr<DMStorage> GetDefaultDMStorage() {
  absl::optional<base::FilePath> keystone_path =
      GetKeystoneFolderPath(UpdaterScope::kSystem);
  return keystone_path ? base::MakeRefCounted<DMStorage>(
                             keystone_path->AppendASCII("DeviceManagement"))
                       : nullptr;
}

}  // namespace updater
