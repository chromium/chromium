// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"

#include <string>

#include "base/base64url.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "chrome/updater/updater_branding.h"

namespace device_management_storage {
namespace {

constexpr char kEnrollmentTokenFilePath[] =
    "/opt/" COMPANY_SHORTNAME_STRING "/" PRODUCT_FULLNAME_STRING
    "/CloudManagementEnrollmentToken";
constexpr char kDmTokenFilePath[] =
    "/opt/" COMPANY_SHORTNAME_STRING "/" PRODUCT_FULLNAME_STRING
    "/CloudManagement";
constexpr int kExpectedMachineIdSize = 32;

// Determines the CBE DeviceID derived from /etc/machine-id as implemented by
// |BrowserDMTokenStorageLinux::InitClientId|.
std::string DetermineDeviceID() {
  std::string machine_id;
  if (!base::ReadFileToString(base::FilePath("/etc/machine-id"), &machine_id)) {
    return std::string();
  }

  std::string_view trimmed_machine_id =
      base::TrimWhitespaceASCII(machine_id, base::TRIM_TRAILING);
  if (trimmed_machine_id.size() != kExpectedMachineIdSize) {
    LOG(ERROR) << "Error: /etc/machine-id contains "
               << trimmed_machine_id.size() << " characters ("
               << kExpectedMachineIdSize << " were expected).";
    return std::string();
  }

  std::string device_id;
  base::Base64UrlEncode(base::SHA1HashString(trimmed_machine_id),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &device_id);
  return device_id;
}

// Reads a token from the given file. Returns the empty string if the file could
// not be read.
std::string LoadTokenFromFile(const base::FilePath& token_file_path) {
  std::string token_value;
  if (!base::ReadFileToString(token_file_path, &token_value)) {
    return std::string();
  }

  return std::string(base::TrimWhitespaceASCII(token_value, base::TRIM_ALL));
}
}  // namespace

class TokenService : public TokenServiceInterface {
 public:
  TokenService(const base::FilePath& enrollment_token_path,
               const base::FilePath& dm_token_path)
      : enrollment_token_path_(enrollment_token_path.empty()
                                   ? base::FilePath(kEnrollmentTokenFilePath)
                                   : enrollment_token_path),
        dm_token_path_(dm_token_path.empty() ? base::FilePath(kDmTokenFilePath)
                                             : dm_token_path),
        enrollment_token_(LoadTokenFromFile(enrollment_token_path_)),
        dm_token_(LoadTokenFromFile(dm_token_path_)) {}
  ~TokenService() override = default;

  // Overrides for TokenServiceInterface.
  std::string GetDeviceID() const override { return device_id_; }

  bool IsEnrollmentMandatory() const override { return false; }

  bool StoreEnrollmentToken(const std::string& enrollment_token) override {
    if (!WriteContentToGlobalReadableFile(enrollment_token_path_,
                                          enrollment_token)) {
      return false;
    }

    enrollment_token_ = enrollment_token;
    return true;
  }

  bool DeleteEnrollmentToken() override {
    if (!base::DeleteFile(enrollment_token_path_)) {
      return false;
    }
    enrollment_token_.clear();
    return true;
  }

  std::string GetEnrollmentToken() const override {
    enrollment_token_ = LoadTokenFromFile(enrollment_token_path_);
    return enrollment_token_;
  }

  bool StoreDmToken(const std::string& dm_token) override {
    if (!WriteContentToGlobalReadableFile(dm_token_path_, dm_token)) {
      return false;
    }
    dm_token_ = dm_token;
    return true;
  }

  bool DeleteDmToken() override {
    if (!base::DeleteFile(dm_token_path_)) {
      return false;
    }
    dm_token_.clear();
    return true;
  }

  std::string GetDmToken() const override {
    dm_token_ = LoadTokenFromFile(dm_token_path_);
    return dm_token_;
  }

 private:
  // Cached values in memory.
  const std::string device_id_ = DetermineDeviceID();
  const base::FilePath enrollment_token_path_;
  const base::FilePath dm_token_path_;
  mutable std::string enrollment_token_;
  mutable std::string dm_token_;
};

scoped_refptr<DMStorage> CreateDMStorage(
    const base::FilePath& policy_cache_root,
    const base::FilePath& enrollment_token_path,
    const base::FilePath& dm_token_path) {
  return CreateDMStorage(
      policy_cache_root,
      std::make_unique<TokenService>(enrollment_token_path, dm_token_path));
}

scoped_refptr<DMStorage> GetDefaultDMStorage() {
  return CreateDMStorage(base::FilePath("/opt")
                             .AppendASCII(COMPANY_SHORTNAME_STRING)
                             .AppendASCII(PRODUCT_FULLNAME_STRING)
                             .AppendASCII("DeviceManagement"));
}

}  // namespace device_management_storage
