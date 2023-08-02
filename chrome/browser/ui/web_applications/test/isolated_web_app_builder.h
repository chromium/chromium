// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_BUILDER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_BUILDER_H_

#include <vector>

#include "base/strings/string_piece.h"
#include "base/version.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"

namespace web_app {

inline constexpr uint8_t kTestPublicKey[] = {
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51,
    0x66, 0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02,
    0x72, 0xFE, 0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

inline constexpr uint8_t kTestPrivateKey[] = {
    0x1F, 0x27, 0x3F, 0x93, 0xE9, 0x59, 0x4E, 0xC7, 0x88, 0x82, 0xC7, 0x49,
    0xF8, 0x79, 0x3D, 0x8C, 0xDB, 0xE4, 0x60, 0x1C, 0x21, 0xF1, 0xD9, 0xF9,
    0xBC, 0x3A, 0xB5, 0xC7, 0x7F, 0x2D, 0x95, 0xE1,
    // public key (part of the private key)
    0xE4, 0xD5, 0x16, 0xC9, 0x85, 0x9A, 0xF8, 0x63, 0x56, 0xA3, 0x51, 0x66,
    0x7D, 0xBD, 0x00, 0x43, 0x61, 0x10, 0x1A, 0x92, 0xD4, 0x02, 0x72, 0xFE,
    0x2B, 0xCE, 0x81, 0xBB, 0x3B, 0x71, 0x3F, 0x2D};

// Derived from `kTestPublicKey`.
inline constexpr base::StringPiece kTestEd25519WebBundleId =
    "4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic";

struct TestSignedWebBundle {
  TestSignedWebBundle(std::vector<uint8_t> data,
                      const web_package::SignedWebBundleId& id);

  TestSignedWebBundle(const TestSignedWebBundle&);
  TestSignedWebBundle(TestSignedWebBundle&&);

  ~TestSignedWebBundle();

  std::vector<uint8_t> data;
  web_package::SignedWebBundleId id;
};

struct TestSignedWebBundleBuilderOptions {
  base::Version version = base::Version("1.0.0");
  web_package::WebBundleSigner::ErrorsForTesting errors_for_testing = {};
};

class TestSignedWebBundleBuilder {
 public:
  explicit TestSignedWebBundleBuilder(
      web_package::WebBundleSigner::KeyPair key_pair =
          web_package::WebBundleSigner::KeyPair::CreateRandom());

  // Adds a manifest type payload to the bundle.
  void AddManifest(base::StringPiece manifest_string);

  // Adds a image/PNG type payload to the bundle.
  void AddPngImage(base::StringPiece url, base::StringPiece image_string);

  TestSignedWebBundle Build(
      TestSignedWebBundleBuilderOptions build_options = {});
  static TestSignedWebBundle BuildDefault(
      TestSignedWebBundleBuilderOptions build_options = {});

 private:
  web_package::WebBundleSigner::KeyPair key_pair_;
  web_package::WebBundleBuilder builder_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_BUILDER_H_
