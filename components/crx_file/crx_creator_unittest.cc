// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/crx_creator.h"
#include "base/base64.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "components/crx_file/crx_verifier.h"
#include "crypto/rsa_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath TestFile(const std::string& file) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("crx_file")
      .AppendASCII(file);
}

}  // namespace

namespace crx_file {

using CrxCreatorTest = testing::Test;

TEST_F(CrxCreatorTest, Create) {
  // Set up a signing key.
  auto signing_key = crypto::RSAPrivateKey::Create(4096);
  std::vector<uint8_t> public_key;
  signing_key->ExportPublicKey(&public_key);
  std::string expected_public_key;
  base::Base64Encode(std::string(public_key.begin(), public_key.end()),
                     &expected_public_key);

  // Create a CRX File.
  base::FilePath temp_file;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_file));
  EXPECT_EQ(CreatorResult::OK,
            Create(temp_file, TestFile("sample.zip"), signing_key.get()));

  // Test that the created file can be verified.
  const std::vector<std::vector<uint8_t>> keys;
  const std::vector<uint8_t> hash;
  std::string public_key_in_crx;
  EXPECT_EQ(
      VerifierResult::OK_FULL,
      Verify(temp_file, VerifierFormat::CRX3, keys, hash, &public_key_in_crx,
             nullptr, /*compressed_verified_contents=*/nullptr));
  EXPECT_EQ(expected_public_key, public_key_in_crx);

  // Delete the file.
  EXPECT_TRUE(base::DeleteFile(temp_file));
}

}  // namespace crx_file
