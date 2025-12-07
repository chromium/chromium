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
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "components/web_package/web_bundle_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

constexpr std::array<uint8_t, 32> kEd25519PublicKey = {
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51,
    0x66, 0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02,
    0x72, 0xFE, 0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

constexpr std::array<uint8_t, 64> kEd25519PrivateKey = {
    0x1F, 0x27, 0x3F, 0x93, 0xE9, 0x59, 0x4E, 0xC7, 0x88, 0x82, 0xC7, 0x49,
    0xF8, 0x79, 0x3D, 0x8C, 0xDB, 0xE4, 0x60, 0x1C, 0x21, 0xF1, 0xD9, 0xF9,
    0xBC, 0x3A, 0xB5, 0xC7, 0x7F, 0x2D, 0x95, 0xE1,
    // public key (part of the private key)
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51, 0x66,
    0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02, 0x72, 0xFE,
    0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

constexpr std::array<uint8_t, 33> kEcdsaP256PublicKey = {
    0x03, 0x0A, 0x22, 0xFC, 0x5C, 0x0B, 0x1E, 0x14, 0x85, 0x90, 0xE1,
    0xF9, 0x87, 0xCC, 0x4E, 0x0D, 0x49, 0x2E, 0xF8, 0xE5, 0x1E, 0x23,
    0xF9, 0xB3, 0x63, 0x75, 0xE1, 0x52, 0xB2, 0x4A, 0xEC, 0xA5, 0xE6};

constexpr std::array<uint8_t, 32> kEcdsaP256PrivateKey = {
    0x24, 0xAB, 0xA9, 0x6A, 0x44, 0x4B, 0xEB, 0xE9, 0x3C, 0xD2, 0x88,
    0x47, 0x22, 0x63, 0x02, 0xB8, 0xE4, 0xA0, 0x16, 0x1A, 0x0E, 0x95,
    0xAA, 0x36, 0x95, 0x26, 0x83, 0x49, 0xEE, 0xCD, 0x27, 0x1A};

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

TEST(WebBundleSignerTest, SignedWebBundleByteByByteComparisonV2BlockEd25519) {
  std::vector<uint8_t> unsigned_bundle = CreateUnsignedBundle();
  std::vector<uint8_t> signed_bundle = test::WebBundleSigner::SignBundle(
      unsigned_bundle,
      test::Ed25519KeyPair(kEd25519PublicKey, kEd25519PrivateKey));

  std::vector<uint8_t> expected_signed_bundle =
      GetStringAsBytes(GetTestFileContents(base::FilePath(
          FILE_PATH_LITERAL("simple_b2_signed_v2_ed25519.swbn"))));
  EXPECT_EQ(signed_bundle.size(), expected_signed_bundle.size());
  EXPECT_EQ(signed_bundle, expected_signed_bundle);
}

TEST(WebBundleSignerTest, SignedWebBundleByteByByteComparisonV2BlockEcdsaP256) {
  std::vector<uint8_t> unsigned_bundle = CreateUnsignedBundle();
  std::vector<uint8_t> signed_bundle = test::WebBundleSigner::SignBundle(
      unsigned_bundle,
      test::EcdsaP256KeyPair(kEcdsaP256PublicKey, kEcdsaP256PrivateKey));

  std::vector<uint8_t> expected_signed_bundle =
      GetStringAsBytes(GetTestFileContents(base::FilePath(
          FILE_PATH_LITERAL("simple_b2_signed_v2_ecdsa_p256.swbn"))));
  EXPECT_EQ(signed_bundle.size(), expected_signed_bundle.size());
  EXPECT_EQ(signed_bundle, expected_signed_bundle);
}

TEST(WebBundleSignerTest, SignedWebBundleByteByByteComparisonV2BlockBothKeys) {
  std::vector<uint8_t> unsigned_bundle = CreateUnsignedBundle();
  std::vector<uint8_t> signed_bundle = test::WebBundleSigner::SignBundle(
      unsigned_bundle,
      {test::EcdsaP256KeyPair(kEcdsaP256PublicKey, kEcdsaP256PrivateKey),
       test::Ed25519KeyPair(kEd25519PublicKey, kEd25519PrivateKey)});

  std::vector<uint8_t> expected_signed_bundle =
      GetStringAsBytes(GetTestFileContents(
          base::FilePath(FILE_PATH_LITERAL("simple_b2_signed_v2.swbn"))));
  EXPECT_EQ(signed_bundle.size(), expected_signed_bundle.size());
  EXPECT_EQ(signed_bundle, expected_signed_bundle);
}

}  // namespace
}  // namespace web_package
