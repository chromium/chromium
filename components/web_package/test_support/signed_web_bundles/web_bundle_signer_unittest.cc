// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"

#include <string_view>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/web_bundle_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

constexpr uint8_t kEd25519PublicKey[] = {
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51,
    0x66, 0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02,
    0x72, 0xFE, 0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D,
};

constexpr uint8_t kEd25519PrivateKey[] = {
    0x1F, 0x27, 0x3F, 0x93, 0xE9, 0x59, 0x4E, 0xC7, 0x88, 0x82, 0xC7, 0x49,
    0xF8, 0x79, 0x3D, 0x8C, 0xDB, 0xE4, 0x60, 0x1C, 0x21, 0xF1, 0xD9, 0xF9,
    0xBC, 0x3A, 0xB5, 0xC7, 0x7F, 0x2D, 0x95, 0xE1,
    // public key (part of the private key)
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51, 0x66,
    0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02, 0x72, 0xFE,
    0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

constexpr std::array<uint8_t, 33> kEcdsaP256PublicKey = {
    0x02, 0x72, 0xcd, 0x38, 0x5f, 0x32, 0xb5, 0x2e, 0x52, 0x6a, 0xe7,
    0xff, 0x36, 0x02, 0x18, 0x04, 0x41, 0x26, 0x87, 0x8e, 0x70, 0x51,
    0x33, 0x58, 0xfa, 0xcb, 0x20, 0x5c, 0x8e, 0xa3, 0x22, 0x0b, 0x53};

constexpr std::array<uint8_t, 32> kEcdsaP256PrivateKey = {
    0xdb, 0xdc, 0xc9, 0x3f, 0x4e, 0x2c, 0xe8, 0x18, 0x91, 0xd3, 0x68,
    0xc8, 0x74, 0x22, 0x36, 0x7d, 0xee, 0x97, 0x0c, 0x6e, 0x92, 0xc8,
    0x7c, 0xd4, 0x2e, 0x01, 0x01, 0x80, 0x62, 0x83, 0xad, 0xf5};

std::string GetTestFileContents(const base::FilePath& path) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.Append(
      base::FilePath(FILE_PATH_LITERAL("components/test/data/web_package")));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(test_data_dir.Append(path), &contents));
  return contents;
}

std::vector<uint8_t> GetStringAsBytes(std::string_view contents) {
  return base::ToVector(base::as_byte_span(contents));
}

std::vector<uint8_t> CreateUnsignedBundle() {
  WebBundleBuilder builder;
  builder.AddExchange(
      "https://test.example.org/",
      {{":status", "200"}, {"content-type", "text/html; charset=UTF-8"}},
      "<a href='index.html'>click for web bundles</a>");
  builder.AddExchange(
      "https://test.example.org/index.html",
      {{":status", "200"}, {"content-type", "text/html; charset=UTF-8"}},
      "<p>Hello Web Bundles!</p>");
  std::vector<uint8_t> unsigned_bundle = builder.CreateBundle();

  std::vector<uint8_t> expected_unsigned_bundle = GetStringAsBytes(
      GetTestFileContents(base::FilePath(FILE_PATH_LITERAL("simple_b2.wbn"))));
  EXPECT_EQ(expected_unsigned_bundle, unsigned_bundle);
  return unsigned_bundle;
}

TEST(WebBundleSignerTest, SignedWebBundleByteByByteComparisonEd25519) {
  std::vector<uint8_t> unsigned_bundle = CreateUnsignedBundle();
  std::vector<uint8_t> signed_bundle = WebBundleSigner::SignBundle(
      unsigned_bundle,
      {WebBundleSigner::Ed25519KeyPair(kEd25519PublicKey, kEd25519PrivateKey)});

  std::vector<uint8_t> expected_bundle = GetStringAsBytes(GetTestFileContents(
      base::FilePath(FILE_PATH_LITERAL("simple_b2_signed.swbn"))));
  EXPECT_EQ(signed_bundle, expected_bundle);
}

TEST(WebBundleSignerTest, SignedWebBundleByteByByteComparisonEcdsaP256SHA256) {
  std::vector<uint8_t> unsigned_bundle = CreateUnsignedBundle();
  std::vector<uint8_t> signed_bundle = WebBundleSigner::SignBundle(
      unsigned_bundle, {WebBundleSigner::EcdsaP256KeyPair(
                           kEcdsaP256PublicKey, kEcdsaP256PrivateKey)});

  std::vector<uint8_t> expected_signed_bundle =
      GetStringAsBytes(GetTestFileContents(base::FilePath(
          FILE_PATH_LITERAL("simple_b2_signed_ecdsa_p256_sha256.swbn"))));
  EXPECT_EQ(signed_bundle.size(), expected_signed_bundle.size());
  EXPECT_EQ(signed_bundle, expected_signed_bundle);
}

}  // namespace
}  // namespace web_package
