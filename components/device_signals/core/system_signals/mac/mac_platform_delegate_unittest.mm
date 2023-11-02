// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/mac/mac_platform_delegate.h"

#import <Foundation/Foundation.h>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#import "base/mac/foundation_util.h"
#include "components/device_signals/test/test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

class MacPlatformDelegateTest : public testing::Test {
 protected:
  MacPlatformDelegateTest()
      : home_dir_(base::GetHomeDir()),
        bundle_path_(test::GetTestBundlePath()),
        binary_path_(test::GetTestBundleBinaryPath()) {}

  const base::FilePath home_dir_;
  const base::FilePath bundle_path_;
  const base::FilePath binary_path_;
  MacPlatformDelegate platform_delegate_;
};

TEST_F(MacPlatformDelegateTest, ResolvingBundlePath) {
  // Should point to the bundle's main executable.
  base::FilePath resolved_file_path;
  ASSERT_TRUE(
      platform_delegate_.ResolveFilePath(bundle_path_, &resolved_file_path));
  EXPECT_EQ(resolved_file_path, binary_path_);
}

TEST_F(MacPlatformDelegateTest, ResolveFilePath_Absolute) {
  base::FilePath resolved_file_path;
  EXPECT_TRUE(
      platform_delegate_.ResolveFilePath(binary_path_, &resolved_file_path));
  EXPECT_EQ(resolved_file_path, binary_path_);
}

TEST_F(MacPlatformDelegateTest, ResolveFilePath_PathTraversal) {
  auto bundle_base_name = bundle_path_.BaseName();
  auto file_path_with_traversal = bundle_path_.DirName();
  auto bundle_dir_base_name = file_path_with_traversal.BaseName();
  file_path_with_traversal = file_path_with_traversal.Append("..")
                                 .Append(bundle_dir_base_name)
                                 .Append(bundle_base_name);

  base::FilePath resolved_file_path;
  EXPECT_TRUE(platform_delegate_.ResolveFilePath(file_path_with_traversal,
                                                 &resolved_file_path));

  // Final resolved path should point to the binary directly.
  EXPECT_EQ(resolved_file_path, binary_path_);
}

TEST_F(MacPlatformDelegateTest, ResolveFilePath_BundleNotFound) {
  base::FilePath file_path =
      base::FilePath::FromUTF8Unsafe("/some/file/path/no/bundle");
  base::FilePath resolved_file_path;
  EXPECT_FALSE(
      platform_delegate_.ResolveFilePath(file_path, &resolved_file_path));
}

TEST_F(MacPlatformDelegateTest, GetProductMetadata_Success) {
  auto product_metadata = platform_delegate_.GetProductMetadata(bundle_path_);

  ASSERT_TRUE(product_metadata);
  ASSERT_TRUE(product_metadata->name);
  EXPECT_EQ(product_metadata->name.value(), test::GetTestBundleProductName());
  ASSERT_TRUE(product_metadata->version);
  EXPECT_EQ(product_metadata->version.value(),
            test::GetTestBundleProductVersion());

  // Metadata values should be the same between a bundle and its binary.
  auto binary_product_metadata =
      platform_delegate_.GetProductMetadata(binary_path_);
  EXPECT_EQ(binary_product_metadata, product_metadata);
}

TEST_F(MacPlatformDelegateTest, GetProductMetadata_NoMetadata) {
  auto product_metadata =
      platform_delegate_.GetProductMetadata(test::GetUnsignedBundlePath());
  ASSERT_TRUE(product_metadata);
  EXPECT_FALSE(product_metadata->name);
  EXPECT_FALSE(product_metadata->version);
}

TEST_F(MacPlatformDelegateTest, GetProductMetadata_NoFile) {
  auto product_metadata =
      platform_delegate_.GetProductMetadata(test::GetUnusedPath());
  ASSERT_TRUE(product_metadata);
  EXPECT_FALSE(product_metadata->name);
  EXPECT_FALSE(product_metadata->version);
}

TEST_F(MacPlatformDelegateTest, GetSigningCertificatesPublicKeyHashes_Success) {
  auto spki_hashes = platform_delegate_.GetSigningCertificatesPublicKeyHashes(
      test::GetTestBundlePath());

  ASSERT_TRUE(spki_hashes);
  ASSERT_EQ(spki_hashes->size(), 1U);

  std::string base64_hash;
  base::Base64Encode(spki_hashes.value()[0], &base64_hash);
  EXPECT_EQ(base64_hash, "E7ahL43DGT2VrGvGpnlI9ONkEqdni9ddf4fCTN26uFc=");

  // Should work for the binary path too.
  auto binary_spki_hashes =
      platform_delegate_.GetSigningCertificatesPublicKeyHashes(
          test::GetTestBundleBinaryPath());
  EXPECT_EQ(binary_spki_hashes, spki_hashes);
}

TEST_F(MacPlatformDelegateTest,
       GetSigningCertificatesPublicKeyHashes_NoSignature) {
  auto spki_hashes = platform_delegate_.GetSigningCertificatesPublicKeyHashes(
      test::GetUnsignedBundlePath());
  ASSERT_TRUE(spki_hashes);
  EXPECT_TRUE(spki_hashes->empty());
}

TEST_F(MacPlatformDelegateTest, GetSigningCertificatesPublicKeyHashes_NoFile) {
  auto spki_hashes = platform_delegate_.GetSigningCertificatesPublicKeyHashes(
      test::GetUnusedPath());
  ASSERT_TRUE(spki_hashes);
  EXPECT_TRUE(spki_hashes->empty());
}

}  // namespace device_signals
