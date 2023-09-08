// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"

#include <memory>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/version.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

namespace {
constexpr base::StringPiece kTestManifest = R"({
      "name": "$1",
      "version": "$2",
      "id": "/",
      "scope": "/",
      "start_url": "/index.html",
      "display": "standalone",
      "icons": [
        {
          "src": "256x256-green.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ]
    })";
}  // namespace

TestSignedWebBundle::TestSignedWebBundle(
    std::vector<uint8_t> data,
    const web_package::SignedWebBundleId& id)
    : data(std::move(data)), id(id) {}

TestSignedWebBundle::TestSignedWebBundle(const TestSignedWebBundle&) = default;

TestSignedWebBundle::TestSignedWebBundle(TestSignedWebBundle&&) = default;

TestSignedWebBundle::~TestSignedWebBundle() = default;

TestSignedWebBundleBuilder::TestSignedWebBundleBuilder(
    web_package::WebBundleSigner::KeyPair key_pair,
    web_package::WebBundleSigner::ErrorsForTesting errors_for_testing)
    : key_pair_(key_pair), errors_for_testing_(errors_for_testing) {}

TestSignedWebBundleBuilder::BuildOptions::BuildOptions()
    : key_pair_(web_package::WebBundleSigner::KeyPair(kTestPublicKey,
                                                      kTestPrivateKey)),
      version_(base::Version("1.0.0")),
      app_name_("Simple Isolated App"),
      errors_for_testing_({}) {}

TestSignedWebBundleBuilder::BuildOptions::BuildOptions(const BuildOptions&) =
    default;
TestSignedWebBundleBuilder::BuildOptions::BuildOptions(BuildOptions&&) =
    default;
TestSignedWebBundleBuilder::BuildOptions::~BuildOptions() = default;

void TestSignedWebBundleBuilder::AddManifest(
    base::StringPiece manifest_string) {
  builder_.AddExchange(
      kTestManifestUrl,
      {{":status", "200"}, {"content-type", "application/manifest+json"}},
      manifest_string);
}

void TestSignedWebBundleBuilder::AddPngImage(base::StringPiece url,
                                             base::StringPiece image_string) {
  builder_.AddExchange(url, {{":status", "200"}, {"content-type", "image/png"}},
                       image_string);
}

void TestSignedWebBundleBuilder::AddHtml(base::StringPiece url,
                                         base::StringPiece html_content) {
  builder_.AddExchange(url, {{":status", "200"}, {"content-type", "text/html"}},
                       html_content);
}

void TestSignedWebBundleBuilder::AddJavaScript(
    base::StringPiece url,
    base::StringPiece script_content) {
  builder_.AddExchange(
      url, {{":status", "200"}, {"content-type", "application/javascript"}},
      script_content);
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
      test::BitmapAsPng(CreateSquareIcon(256, SK_ColorGREEN)));

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
