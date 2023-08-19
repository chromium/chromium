// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/device_management/dm_storage.h"

#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/strings/string_util.h"
#include "chrome/updater/updater_branding.h"

namespace updater {
namespace {

constexpr char kEnrollmentTokenFilePath[] =
    "/opt/" COMPANY_SHORTNAME_STRING "/" PRODUCT_FULLNAME_STRING
    "/CloudManagementEnrollmentToken";
constexpr char kDmTokenFilePath[] =
    "/opt/" COMPANY_SHORTNAME_STRING "/" PRODUCT_FULLNAME_STRING
    "/CloudManagement";

std::string GetMachineId() {
  std::string machine_id;
  if (!base::ReadFileToString(base::FilePath("/etc/machine-id"), &machine_id)) {
    return std::string();
  }
  return machine_id;
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

  std::string GetEnrollmentToken() const override { return enrollment_token_; }

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

  std::string GetDmToken() const override { return dm_token_; }

 private:
  // Cached values in memory.
  const std::string device_id_ = GetMachineId();
  const base::FilePath enrollment_token_path_;
  const base::FilePath dm_token_path_;
  std::string enrollment_token_;
  std::string dm_token_;
};

DMStorage::DMStorage(const base::FilePath& policy_cache_root,
                     const base::FilePath& enrollment_token_path,
                     const base::FilePath& dm_token_path)
    : DMStorage(policy_cache_root,
                std::make_unique<TokenService>(enrollment_token_path,
                                               dm_token_path)) {}

scoped_refptr<DMStorage> GetDefaultDMStorage() {
  return base::MakeRefCounted<DMStorage>(
      base::FilePath("/opt")
          .AppendASCII(COMPANY_SHORTNAME_STRING)
          .AppendASCII(PRODUCT_FULLNAME_STRING)
          .AppendASCII("DeviceManagement"));
}

}  // namespace updater
