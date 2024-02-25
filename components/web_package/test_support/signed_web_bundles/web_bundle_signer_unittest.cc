// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "components/web_package/web_bundle_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

constexpr uint8_t kTestPublicKey[] = {
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51,
    0x66, 0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02,
    0x72, 0xFE, 0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D,
};

constexpr uint8_t kTestPrivateKey[] = {
    0x1F, 0x27, 0x3F, 0x93, 0xE9, 0x59, 0x4E, 0xC7, 0x88, 0x82, 0xC7, 0x49,
    0xF8, 0x79, 0x3D, 0x8C, 0xDB, 0xE4, 0x60, 0x1C, 0x21, 0xF1, 0xD9, 0xF9,
    0xBC, 0x3A, 0xB5, 0xC7, 0x7F, 0x2D, 0x95, 0xE1,
    // public key (part of the private key)
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51, 0x66,
    0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02, 0x72, 0xFE,
    0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

std::string GetTestFileContents(const base::FilePath& path) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.Append(
      base::FilePath(FILE_PATH_LITERAL("components/test/data/web_package")));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(test_data_dir.Append(path), &contents));
  return contents;
}

std::vector<uint8_t> GetStringAsBytes(base::StringPiece contents) {
  auto bytes = base::as_bytes(base::make_span(contents));
  return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

TEST(WebBundleSignerTest, SignedWebBundleByteByByteComparison) {
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
  std::vector<uint8_t> signed_bundle = WebBundleSigner::SignBundle(
      unsigned_bundle,
      {WebBundleSigner::KeyPair(kTestPublicKey, kTestPrivateKey)});

  std::vector<uint8_t> expected_bundle = GetStringAsBytes(GetTestFileContents(
      base::FilePath(FILE_PATH_LITERAL("simple_b2_signed.swbn"))));
  EXPECT_EQ(signed_bundle, expected_bundle);
}

}  // namespace
}  // namespace web_package
