// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_storage.h"

#import <Foundation/Foundation.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/mac_util.h"

namespace updater {
namespace {

const CFStringRef kEnrollmentTokenKey = CFSTR("EnrollmentToken");
const CFStringRef kBrowserBundleId =
    CFSTR(MAC_BROWSER_BUNDLE_IDENTIFIER_STRING);

bool LoadEnrollmentTokenFromPolicy(std::string* enrollment_token) {
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

void DeletePolicyEnrollmentToken() {
  CFPreferencesSetValue(kEnrollmentTokenKey, nil, kBrowserBundleId,
                        kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
  CFPreferencesSynchronize(kBrowserBundleId, kCFPreferencesAnyUser,
                           kCFPreferencesCurrentHost);
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
  TokenService(const base::FilePath& enrollment_token_path,
               const base::FilePath& dm_token_path);
  ~TokenService() override = default;

  // Overrides for TokenServiceInterface.
  std::string GetDeviceID() const override { return device_id_; }
  bool IsEnrollmentMandatory() const override { return false; }
  bool StoreEnrollmentToken(const std::string& enrollment_token) override;
  bool DeleteEnrollmentToken() override;
  std::string GetEnrollmentToken() const override { return enrollment_token_; }
  bool StoreDmToken(const std::string& dm_token) override;
  bool DeleteDmToken() override;
  std::string GetDmToken() const override { return dm_token_; }

 private:
  // Cached values in memory.
  const std::string device_id_ = base::mac::GetPlatformSerialNumber();
  const base::FilePath enrollment_token_path_;
  const base::FilePath dm_token_path_;
  std::string enrollment_token_;
  std::string dm_token_;
};

TokenService::TokenService(const base::FilePath& enrollment_token_path,
                           const base::FilePath& dm_token_path)
    : enrollment_token_path_(enrollment_token_path.empty()
                                 ? GetEnrollmentTokenFilePath()
                                 : enrollment_token_path),
      dm_token_path_(dm_token_path.empty() ? GetDmTokenFilePath()
                                           : dm_token_path) {
  std::string enrollment_token;
  if (LoadEnrollmentTokenFromPolicy(&enrollment_token) ||
      LoadTokenFromFile(enrollment_token_path_, &enrollment_token)) {
    enrollment_token_ = enrollment_token;
  }

  std::string dm_token;
  if (LoadTokenFromFile(dm_token_path_, &dm_token)) {
    dm_token_ = dm_token;
  }
}

bool TokenService::StoreEnrollmentToken(const std::string& enrollment_token) {
  if (enrollment_token_path_.empty() ||
      !CreateGlobalAccessibleDirectory(enrollment_token_path_.DirName()) ||
      !WriteContentToGlobalReadableFile(enrollment_token_path_,
                                        enrollment_token)) {
    VLOG(1) << "Failed to update enrollment token.";
    return false;
  }

  enrollment_token_ = enrollment_token;
  VLOG(1) << "Updated enrollment token to: " << enrollment_token;
  return true;
}

bool TokenService::DeleteEnrollmentToken() {
  enrollment_token_ = "";
  DeletePolicyEnrollmentToken();
  return base::DeleteFile(base::FilePath(enrollment_token_path_));
}

bool TokenService::StoreDmToken(const std::string& token) {
  if (dm_token_path_.empty() ||
      !CreateGlobalAccessibleDirectory(dm_token_path_.DirName()) ||
      !WriteContentToGlobalReadableFile(dm_token_path_, token)) {
    VLOG(1) << "Failed to update DM token.";
    return false;
  }
  dm_token_ = token;
  VLOG(1) << "Updated DM token to: " << token;
  return true;
}

bool TokenService::DeleteDmToken() {
  if (dm_token_path_.empty() || !base::DeleteFile(dm_token_path_)) {
    VLOG(1) << "Failed to delete DM token.";
    return false;
  }
  dm_token_.clear();
  VLOG(1) << "DM token deleted.";
  return true;
}

}  // namespace

DMStorage::DMStorage(const base::FilePath& policy_cache_root,
                     const base::FilePath& enrollment_token_path,
                     const base::FilePath& dm_token_path)
    : DMStorage(policy_cache_root,
                std::make_unique<TokenService>(enrollment_token_path,
                                               dm_token_path)) {}

scoped_refptr<DMStorage> GetDefaultDMStorage() {
  absl::optional<base::FilePath> keystone_path =
      GetKeystoneFolderPath(UpdaterScope::kSystem);
  return keystone_path ? base::MakeRefCounted<DMStorage>(
                             keystone_path->AppendASCII("DeviceManagement"))
                       : nullptr;
}

}  // namespace updater
