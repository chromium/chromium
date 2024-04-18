// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_TEST_SIGNED_WEB_BUNDLE_BUILDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_TEST_SIGNED_WEB_BUNDLE_BUILDER_H_

#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/version.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "url/gurl.h"

class SkBitmap;

namespace web_app {
namespace test {
  std::string EncodeAsPng(const SkBitmap& bitmap);
}

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
inline constexpr std::string_view kTestEd25519WebBundleId =
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

class TestSignedWebBundleBuilder {
 public:
  explicit TestSignedWebBundleBuilder(
      web_package::WebBundleSigner::Ed25519KeyPair key_pair =
          web_package::WebBundleSigner::Ed25519KeyPair::CreateRandom(),
      web_package::WebBundleSigner::ErrorsForTesting errors_for_testing = {
          /*integrity_block_errors=*/{},
          /*signatures_errors=*/{}});

  static constexpr std::string_view kTestManifestUrl =
      "/.well-known/manifest.webmanifest";
  static constexpr std::string_view kTestIconUrl = "/256x256-green.png";
  static constexpr std::string_view kTestHtmlUrl = "/index.html";

  // TODO(crbug.com/40264793): Use a struct instead when designated initializers
  // are supported.
  class BuildOptions {
   public:
    BuildOptions();
    BuildOptions(const BuildOptions&);
    BuildOptions(BuildOptions&&);
    ~BuildOptions();

    BuildOptions& SetKeyPair(
        web_package::WebBundleSigner::Ed25519KeyPair key_pair) {
      key_pair_ = std::move(key_pair);
      return *this;
    }

    BuildOptions& SetVersion(base::Version version) {
      version_ = std::move(version);
      return *this;
    }

    BuildOptions& SetPrimaryUrl(GURL primary_url) {
      primary_url_ = std::move(primary_url);
      return *this;
    }

    BuildOptions& SetAppName(const std::string& app_name) {
      app_name_ = app_name;
      return *this;
    }

    BuildOptions& SetBaseUrl(GURL base_url) {
      base_url_ = std::move(base_url);
      return *this;
    }

    BuildOptions& SetIndexHTMLContent(std::string_view index_html_content) {
      index_html_content_ = index_html_content;
      return *this;
    }

    BuildOptions& SetErrorsForTesting(
        web_package::WebBundleSigner::ErrorsForTesting errors_for_testing) {
      errors_for_testing_ = errors_for_testing;
      return *this;
    }

    web_package::WebBundleSigner::Ed25519KeyPair key_pair_;
    base::Version version_;
    std::string app_name_;
    std::optional<GURL> primary_url_;
    std::optional<GURL> base_url_;
    std::optional<std::string_view> index_html_content_;
    web_package::WebBundleSigner::ErrorsForTesting errors_for_testing_;
  };

  // Adds a manifest type payload to the bundle.
  void AddManifest(std::string_view manifest_string);

  // Adds a image/PNG type payload to the bundle.
  void AddPngImage(std::string_view url, std::string_view image_string);

  // Adds a text/html type payload to the bundle.
  void AddHtml(std::string_view url, std::string_view html_content);

  // Adds an application/javascript type payload to the bundle.
  void AddJavaScript(std::string_view url, std::string_view script_content);

  // For each file, deduces the mime type from the extension and adds a payload
  // with this type to the bundle. Only files with the extension ".webmanifest"
  // are added with the manifest mime type. For files in sub directories, the
  // url mirrors the relative path.
  void AddFilesFromFolder(const base::FilePath& folder);

  // Adds the primary url to the bundle. DO NOT use this for IWAs - primary URLs
  // are not supported in IWAs.
  void AddPrimaryUrl(GURL url);

  TestSignedWebBundle Build();
  static TestSignedWebBundle BuildDefault(
      BuildOptions build_options = BuildOptions());

 private:
  web_package::WebBundleSigner::Ed25519KeyPair key_pair_;
  web_package::WebBundleSigner::ErrorsForTesting errors_for_testing_;
  web_package::WebBundleBuilder builder_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_TEST_SIGNED_WEB_BUNDLE_BUILDER_H_
