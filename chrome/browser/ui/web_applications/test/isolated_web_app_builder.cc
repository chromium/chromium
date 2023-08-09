// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"

#include <memory>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/version.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

namespace web_app {

namespace {
constexpr base::StringPiece kTestManifest = R"({
      "name": "Simple Isolated App",
      "version": "$1",
      "id": "/",
      "scope": "/",
      "start_url": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "256x256-green.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ]
    })";

std::string GetTestIconInString() {
  SkBitmap icon_bitmap = CreateSquareIcon(256, SK_ColorGREEN);
  SkDynamicMemoryWStream stream;
  bool success = SkPngEncoder::Encode(&stream, icon_bitmap.pixmap(), {});
  CHECK(success);
  sk_sp<SkData> icon_skdata = stream.detachAsData();
  return std::string(static_cast<const char*>(icon_skdata->data()),
                     icon_skdata->size());
}
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

void TestSignedWebBundleBuilder::AddPrimaryUrl(base::StringPiece url) {
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
    TestSignedWebBundleBuilderOptions build_options) {
  TestSignedWebBundleBuilder builder = TestSignedWebBundleBuilder(
      web_package::WebBundleSigner::KeyPair(kTestPublicKey, kTestPrivateKey),
      build_options.errors_for_testing);

  if (build_options.primary_url != "") {
    builder.AddPrimaryUrl(build_options.primary_url);
  }

  const bool has_valid_base_url = build_options.base_url.is_valid();

  builder.AddManifest(base::ReplaceStringPlaceholders(
      kTestManifest, {build_options.version.GetString()},
      /*offsets=*/nullptr));

  builder.AddPngImage(has_valid_base_url
                          ? build_options.base_url.Resolve(kTestIconUrl).spec()
                          : kTestIconUrl,
                      GetTestIconInString());

  if (!build_options.html_content.empty()) {
    builder.AddHtml(has_valid_base_url
                        ? build_options.base_url.Resolve(kTestHtmlUrl).spec()
                        : kTestHtmlUrl,
                    build_options.html_content);
  }

  return builder.Build();
}

}  // namespace web_app
