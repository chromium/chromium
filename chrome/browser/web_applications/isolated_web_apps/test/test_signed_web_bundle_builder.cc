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
}  // namespace

namespace test {

std::string EncodeAsPng(const SkBitmap& bitmap) {
  SkDynamicMemoryWStream stream;
  CHECK(SkPngEncoder::Encode(&stream, bitmap.pixmap(), {}));
  sk_sp<SkData> icon_skdata = stream.detachAsData();
  return std::string(static_cast<const char*>(icon_skdata->data()),
                     icon_skdata->size());
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
    web_package::WebBundleSigner::Ed25519KeyPair key_pair,
    web_package::WebBundleSigner::ErrorsForTesting errors_for_testing)
    : key_pair_(key_pair), errors_for_testing_(errors_for_testing) {}

TestSignedWebBundleBuilder::BuildOptions::BuildOptions()
    : key_pair_(web_package::WebBundleSigner::Ed25519KeyPair(kTestPublicKey,
                                                             kTestPrivateKey)),
      version_(base::Version("1.0.0")),
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
  return TestSignedWebBundle(
      web_package::WebBundleSigner::SignBundle(
          builder_.CreateBundle(), {key_pair_}, errors_for_testing_),
      web_package::SignedWebBundleId::CreateForEd25519PublicKey(
          key_pair_.public_key));
}

TestSignedWebBundle TestSignedWebBundleBuilder::BuildDefault(
    BuildOptions build_options) {
  TestSignedWebBundleBuilder builder = TestSignedWebBundleBuilder(
      build_options.key_pair_, build_options.errors_for_testing_);

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
