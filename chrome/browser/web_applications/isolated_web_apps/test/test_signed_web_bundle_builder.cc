// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "net/base/mime_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

namespace web_app {

namespace {
constexpr std::string_view kTestManifest = R"({
      "name": "$1",
      "version": "$2",
      "id": "/",
      "scope": "/",
      "start_url": "/index.html",
      "display": "standalone",
      "icons": [
        {
          "src": "/256x256-green.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ]
    })";

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

// Returns the value of `web_bundle_id` if specified, or generates a fallback ID
// from `key_pair`'s public key.
web_package::SignedWebBundleId GetWebBundleIdWithFallback(
    const std::optional<web_package::SignedWebBundleId>& web_bundle_id,
    const web_package::test::KeyPair& key_pair) {
  if (web_bundle_id) {
    return *web_bundle_id;
  }
  return absl::visit(
      [](const auto& key_pair) {
        return web_package::SignedWebBundleId::CreateForPublicKey(
            key_pair.public_key);
      },
      key_pair);
}
}  // namespace

namespace test {

std::string EncodeAsPng(const SkBitmap& bitmap) {
  SkDynamicMemoryWStream stream;
  CHECK(SkPngEncoder::Encode(&stream, bitmap.pixmap(), {}));
  sk_sp<SkData> icon_skdata = stream.detachAsData();
  return std::string(static_cast<const char*>(icon_skdata->data()),
                     icon_skdata->size());
}

web_package::test::Ed25519KeyPair GetDefaultEd25519KeyPair() {
  return web_package::test::Ed25519KeyPair(kEd25519PublicKey,
                                           kEd25519PrivateKey);
}

web_package::SignedWebBundleId GetDefaultEd25519WebBundleId() {
  return web_package::SignedWebBundleId::CreateForPublicKey(
      GetDefaultEd25519KeyPair().public_key);
}

web_package::test::EcdsaP256KeyPair GetDefaultEcdsaP256KeyPair() {
  return web_package::test::EcdsaP256KeyPair(kEcdsaP256PublicKey,
                                             kEcdsaP256PrivateKey);
}

web_package::SignedWebBundleId GetDefaultEcdsaP256WebBundleId() {
  return web_package::SignedWebBundleId::CreateForPublicKey(
      GetDefaultEcdsaP256KeyPair().public_key);
}

}  // namespace test

TestSignedWebBundle::TestSignedWebBundle(
    std::vector<uint8_t> data,
    const web_package::SignedWebBundleId& id)
    : data(std::move(data)), id(id) {}

TestSignedWebBundle::TestSignedWebBundle(const TestSignedWebBundle&) = default;

TestSignedWebBundle::TestSignedWebBundle(TestSignedWebBundle&&) = default;

TestSignedWebBundle::~TestSignedWebBundle() = default;

TestSignedWebBundleBuilder::TestSignedWebBundleBuilder(
    web_package::test::KeyPair key_pair,
    web_package::test::WebBundleSigner::ErrorsForTesting errors_for_testing)
    : key_pairs_({std::move(key_pair)}),
      errors_for_testing_(errors_for_testing) {}

TestSignedWebBundleBuilder::TestSignedWebBundleBuilder(
    web_package::test::KeyPairs key_pairs,
    const web_package::SignedWebBundleId& web_bundle_id,
    web_package::test::WebBundleSigner::ErrorsForTesting errors_for_testing)
    : key_pairs_(std::move(key_pairs)),
      web_bundle_id_(web_bundle_id),
      errors_for_testing_(std::move(errors_for_testing)) {
  CHECK_GE(key_pairs_.size(), 1U)
      << "At least 1 key has to be specified for signing.";
}

TestSignedWebBundleBuilder::~TestSignedWebBundleBuilder() = default;

TestSignedWebBundleBuilder::BuildOptions::BuildOptions()
    : version_(base::Version("1.0.0")),
      app_name_("Simple Isolated App"),
      errors_for_testing_(
          {/*integrity_block_errors=*/{}, /*signatures_errors=*/{}}) {}

TestSignedWebBundleBuilder::BuildOptions::BuildOptions(const BuildOptions&) =
    default;
TestSignedWebBundleBuilder::BuildOptions::BuildOptions(BuildOptions&&) =
    default;
TestSignedWebBundleBuilder::BuildOptions::~BuildOptions() = default;

void TestSignedWebBundleBuilder::AddManifest(std::string_view manifest_string) {
  builder_.AddExchange(
      kTestManifestUrl,
      {{":status", "200"}, {"content-type", "application/manifest+json"}},
      manifest_string);
}

void TestSignedWebBundleBuilder::AddPngImage(std::string_view url,
                                             std::string_view image_string) {
  builder_.AddExchange(url, {{":status", "200"}, {"content-type", "image/png"}},
                       image_string);
}

void TestSignedWebBundleBuilder::AddHtml(std::string_view url,
                                         std::string_view html_content) {
  builder_.AddExchange(url, {{":status", "200"}, {"content-type", "text/html"}},
                       html_content);
}

void TestSignedWebBundleBuilder::AddJavaScript(
    std::string_view url,
    std::string_view script_content) {
  builder_.AddExchange(
      url, {{":status", "200"}, {"content-type", "application/javascript"}},
      script_content);
}

void TestSignedWebBundleBuilder::AddFilesFromFolder(
    const base::FilePath& folder) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(base::DirectoryExists(folder))
      << "Directory '" << folder.LossyDisplayName() << "' not found.";
  base::FilePath file_path;
  base::FileEnumerator file_enumerator(folder, /*recursive=*/true,
                                       base::FileEnumerator::FILES);

  while (!(file_path = file_enumerator.Next()).empty()) {
    std::string mime_type;
    if (file_path.MatchesExtension(FILE_PATH_LITERAL(".webmanifest"))) {
      mime_type = "application/manifest+json";
    } else if (!net::GetWellKnownMimeTypeFromExtension(
                   file_path.Extension().substr(1), &mime_type)) {
      LOG(ERROR) << "Unable to deduce mime type from extension: "
                 << file_path.Extension();
      continue;
    }

    std::string file_content;
    if (!ReadFileToString(file_path, &file_content)) {
      continue;
    }

    base::FilePath relative_path;
    folder.AppendRelativePath(file_path, &relative_path);
    builder_.AddExchange(relative_path.AsUTF8Unsafe(),
                         {{":status", "200"}, {"content-type", mime_type}},
                         file_content);
  }
}

void TestSignedWebBundleBuilder::AddPrimaryUrl(GURL url) {
  builder_.AddPrimaryURL(url);
}

TestSignedWebBundle TestSignedWebBundleBuilder::Build() {
  web_package::SignedWebBundleId web_bundle_id =
      GetWebBundleIdWithFallback(web_bundle_id_, key_pairs_[0]);

  return TestSignedWebBundle(
      web_package::test::WebBundleSigner::SignBundle(
          builder_.CreateBundle(), key_pairs_,
          /*ib_attributes=*/{{.web_bundle_id = web_bundle_id.id()}},
          errors_for_testing_),
      web_bundle_id);
}

TestSignedWebBundle TestSignedWebBundleBuilder::BuildDefault(
    BuildOptions build_options) {
  if (build_options.key_pairs_.empty()) {
    build_options.AddKeyPair(test::GetDefaultEd25519KeyPair());
  }
  CHECK(build_options.key_pairs_.size() == 1 || build_options.web_bundle_id_)
      << "`web_bundle_id` must always be set if there's more than 1 key "
         "involved.";

  web_package::SignedWebBundleId web_bundle_id = GetWebBundleIdWithFallback(
      build_options.web_bundle_id_, build_options.key_pairs_[0]);
  auto builder = TestSignedWebBundleBuilder(
      std::move(build_options.key_pairs_), std::move(web_bundle_id),
      std::move(build_options.errors_for_testing_));

  if (build_options.primary_url_.has_value()) {
    builder.AddPrimaryUrl(build_options.primary_url_.value());
  }

  builder.AddManifest(base::ReplaceStringPlaceholders(
      kTestManifest,
      {build_options.app_name_, build_options.version_.GetString()},
      /*offsets=*/nullptr));

  builder.AddPngImage(
      build_options.base_url_.has_value()
          ? build_options.base_url_.value().Resolve(kTestIconUrl).spec()
          : kTestIconUrl,
      test::EncodeAsPng(CreateSquareIcon(256, SK_ColorGREEN)));

  if (build_options.index_html_content_.has_value()) {
    builder.AddHtml(
        build_options.base_url_.has_value()
            ? build_options.base_url_.value().Resolve(kTestHtmlUrl).spec()
            : kTestHtmlUrl,
        build_options.index_html_content_.value());
  }

  return builder.Build();
}

}  // namespace web_app
