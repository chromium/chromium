// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/win_platform_delegate.h"

#include <array>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "components/device_signals/test/win/scoped_executable_files.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

namespace {

// Using regular strings instead of file path literals as they will be used
// to construct all sorts of file paths, and also non-file-paths.
constexpr char kEnvironmentVariableName[] = "TestEnvironmentVariablePath";
constexpr char kTestFileName[] = "test_file";

constexpr char kExpectedSignedBase64PublicKey[] =
    "Rsw3wqh8gUxnMU8j2jGvvBMZqpe6OhIxn/WeEVg+pYQ=";
constexpr char kExpectedMultiSignedPrimaryBase64PublicKey[] =
    "ir/0opX6HPqsQlv4dFWqSx+nilORf7Q9474b2lGYZ94=";
constexpr char kExpectedMultiSignedSecondaryBase64PublicKey[] =
    "tzTDLyjSfGIMobYniu5f0JwZ5uSo0nmBV7T566A3vcQ=";

constexpr base::FilePath::CharType kInexistantFileName[] =
    FILE_PATH_LITERAL("does_not_exit");

}  // namespace

class WinPlatformDelegateTest : public testing::Test {
 protected:
  WinPlatformDelegateTest() : env_(base::Environment::Create()) {
    EXPECT_TRUE(scoped_dir_.CreateUniqueTempDir());
    absolute_file_path_ = scoped_dir_.GetPath().Append(
        base::FilePath::FromUTF8Unsafe(kTestFileName));
    EXPECT_TRUE(
        base::WriteFile(absolute_file_path_,
                        base::Uuid::GenerateRandomV4().AsLowercaseString()));

    env_->SetVar(kEnvironmentVariableName,
                 scoped_dir_.GetPath().AsUTF8Unsafe());
  }

  ~WinPlatformDelegateTest() override {
    env_->UnSetVar(kEnvironmentVariableName);
  }

  base::ScopedTempDir scoped_dir_;
  base::FilePath absolute_file_path_;
  std::unique_ptr<base::Environment> env_;
  test::ScopedExecutableFiles scoped_executable_files_;
  WinPlatformDelegate platform_delegate_;
};

TEST_F(WinPlatformDelegateTest, ResolveFilePath_Success) {
  std::string directory_name = scoped_dir_.GetPath().BaseName().AsUTF8Unsafe();

  std::array<std::string, 2> test_cases = {
      base::StrCat({"%", kEnvironmentVariableName, "%\\", kTestFileName}),
      base::StrCat({"%", kEnvironmentVariableName, "%\\..\\", directory_name,
                    "\\", kTestFileName})};

  for (const auto& test_case : test_cases) {
    base::FilePath resolved_fp;
    const base::FilePath test_case_fp =
        base::FilePath::FromUTF8Unsafe(test_case);
    EXPECT_TRUE(platform_delegate_.ResolveFilePath(test_case_fp, &resolved_fp));
    EXPECT_TRUE(base::ContentsEqual(absolute_file_path_, resolved_fp));
  }
}

TEST_F(WinPlatformDelegateTest, ResolveFilePath_Fail) {
  base::FilePath resolved_fp;
  EXPECT_FALSE(platform_delegate_.ResolveFilePath(
      scoped_dir_.GetPath().Append(kInexistantFileName), &resolved_fp));
  EXPECT_EQ(resolved_fp, base::FilePath());
}

TEST_F(WinPlatformDelegateTest, GetSigningCertificatesPublicKeys_InvalidPath) {
  auto public_keys =
      platform_delegate_.GetSigningCertificatesPublicKeys(base::FilePath());
  ASSERT_TRUE(public_keys);
  EXPECT_EQ(public_keys->hashes.size(), 0U);
  EXPECT_FALSE(public_keys->is_os_verified);
  EXPECT_FALSE(public_keys->subject_name);
}

TEST_F(WinPlatformDelegateTest, GetSigningCertificatesPublicKeys_Signed) {
  base::FilePath signed_exe_path = scoped_executable_files_.GetSignedExePath();
  ASSERT_TRUE(base::PathExists(signed_exe_path));

  auto public_keys =
      platform_delegate_.GetSigningCertificatesPublicKeys(signed_exe_path);
  ASSERT_TRUE(public_keys);
  EXPECT_EQ(public_keys->hashes.size(), 1U);
  // The binary is properly signed, but with a self-signed cert that the OS
  // does not trust.
  EXPECT_FALSE(public_keys->is_os_verified);
  EXPECT_TRUE(public_keys->subject_name);
  EXPECT_EQ(public_keys->subject_name.value(), "Joe's-Software-Emporium");

  const std::string base64_encoded_public_key =
      base::Base64Encode(public_keys.value().hashes[0]);
  EXPECT_EQ(base64_encoded_public_key, kExpectedSignedBase64PublicKey);
}

TEST_F(WinPlatformDelegateTest, GetSigningCertificatesPublicKeys_MultiSigned) {
  base::FilePath multi_signed_exe_path =
      scoped_executable_files_.GetMultiSignedExePath();
  ASSERT_TRUE(base::PathExists(multi_signed_exe_path));

  auto public_keys = platform_delegate_.GetSigningCertificatesPublicKeys(
      multi_signed_exe_path);
  ASSERT_TRUE(public_keys);
  EXPECT_EQ(public_keys->hashes.size(), 2U);
  // The binary is properly signed, but with a self-signed cert that the OS
  // does not trust.
  EXPECT_FALSE(public_keys->is_os_verified);
  EXPECT_TRUE(public_keys->subject_name);
  EXPECT_EQ(public_keys->subject_name.value(), "SebL's-Software-Emporium");

  std::string base64_encoded_public_key =
      base::Base64Encode(public_keys.value().hashes[0]);
  EXPECT_EQ(base64_encoded_public_key,
            kExpectedMultiSignedPrimaryBase64PublicKey);
  base64_encoded_public_key = base::Base64Encode(public_keys.value().hashes[1]);
  EXPECT_EQ(base64_encoded_public_key,
            kExpectedMultiSignedSecondaryBase64PublicKey);
}

TEST_F(WinPlatformDelegateTest, GetSigningCertificatePublicKeysHash_Empty) {
  base::FilePath empty_exe_path = scoped_executable_files_.GetEmptyExePath();
  ASSERT_TRUE(base::PathExists(empty_exe_path));

  auto public_keys =
      platform_delegate_.GetSigningCertificatesPublicKeys(empty_exe_path);
  ASSERT_TRUE(public_keys);
  EXPECT_EQ(public_keys->hashes.size(), 0U);
  EXPECT_FALSE(public_keys->is_os_verified);
  EXPECT_FALSE(public_keys->subject_name);
}

TEST_F(WinPlatformDelegateTest, GetProductMetadata_Success) {
  base::FilePath metadata_exe_path =
      scoped_executable_files_.GetMetadataExePath();
  ASSERT_TRUE(base::PathExists(metadata_exe_path));

  auto metadata = platform_delegate_.GetProductMetadata(metadata_exe_path);

  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->name, scoped_executable_files_.GetMetadataProductName());
  EXPECT_EQ(metadata->version,
            scoped_executable_files_.GetMetadataProductVersion());
}

TEST_F(WinPlatformDelegateTest, GetProductMetadata_Empty) {
  base::FilePath empty_exe_path = scoped_executable_files_.GetEmptyExePath();
  ASSERT_TRUE(base::PathExists(empty_exe_path));

  EXPECT_FALSE(platform_delegate_.GetProductMetadata(empty_exe_path));
}

}  // namespace device_signals
