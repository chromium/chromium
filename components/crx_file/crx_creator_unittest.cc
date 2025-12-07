// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/crx_creator.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "components/crx_file/crx_verifier.h"
#include "crypto/keypair.h"
#include "crypto/test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath TestFile(const std::string& file) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("crx_file")
      .AppendASCII(file);
}

// Gzip compression of UTF-8 encoded string `sample_verified_contents`.
constexpr uint8_t kTestCompressedVerifiedContents[] = {
    0x1f, 0x8b, 0x8,  0x0,  0xbb, 0xf7, 0x1a, 0x60, 0x2,  0xff, 0x2b, 0x4e,
    0xcc, 0x2d, 0xc8, 0x49, 0x8d, 0x2f, 0x4b, 0x2d, 0xca, 0x4c, 0xcb, 0x4c,
    0x4d, 0x89, 0x4f, 0xce, 0xcf, 0x2b, 0x49, 0xcd, 0x2b, 0x29, 0x6,  0x0,
    0x8a, 0x10, 0xc9, 0x1,  0x18, 0x0,  0x0,  0x0,  0x0};

}  // namespace

namespace crx_file {

using CrxCreatorTest = ::testing::Test;

TEST_F(CrxCreatorTest, Create) {
  auto private_key = crypto::test::FixedRsa4096PrivateKeyForTesting();
  auto expected_public_key =
      base::Base64Encode(private_key.ToSubjectPublicKeyInfo());

  // Create a CRX File.
  base::FilePath temp_file;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_file));
  EXPECT_EQ(CreatorResult::OK,
            Create(temp_file, TestFile("sample.zip"), private_key));

  // Test that the created file can be verified.
  std::string public_key_in_crx;
  EXPECT_EQ(
      VerifierResult::OK_FULL,
      Verify(temp_file, VerifierFormat::CRX3, /*required_key_hashes=*/{},
             /*required_file_hash=*/{}, &public_key_in_crx, /*crx_id=*/nullptr,
             /*compressed_verified_contents=*/nullptr));
  EXPECT_EQ(expected_public_key, public_key_in_crx);

  // Delete the file.
  EXPECT_TRUE(base::DeleteFile(temp_file));
}

TEST_F(CrxCreatorTest, VerifyCrxWithVerifiedContents) {
  auto private_key = crypto::test::FixedRsa4096PrivateKeyForTesting();
  const std::string expected_public_key =
      base::Base64Encode(private_key.ToSubjectPublicKeyInfo());

  // Create a CRX File.
  base::FilePath temp_file;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_file));

  EXPECT_EQ(CreatorResult::OK,
            CreateCrxWithVerifiedContentsInHeader(
                temp_file, TestFile("sample.zip"), private_key,
                std::string(std::begin(kTestCompressedVerifiedContents),
                            std::end(kTestCompressedVerifiedContents))));

  // Test that the created file can be verified.
  std::string public_key_in_crx;
  std::vector<uint8_t> compressed_verified_contents;
  EXPECT_EQ(VerifierResult::OK_FULL,
            Verify(temp_file, VerifierFormat::CRX3, /*required_key_hashes=*/{},
                   /*required_file_hash=*/{}, &public_key_in_crx,
                   /*crx_id=*/nullptr, &compressed_verified_contents));
  EXPECT_EQ(expected_public_key, public_key_in_crx);
  EXPECT_THAT(compressed_verified_contents,
              testing::ElementsAreArray(kTestCompressedVerifiedContents));

  // Delete the file.
  EXPECT_TRUE(base::DeleteFile(temp_file));
}

}  // namespace crx_file
