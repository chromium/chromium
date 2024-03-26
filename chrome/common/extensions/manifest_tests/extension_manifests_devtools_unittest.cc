// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using DevToolsPageManifestTest = ChromeManifestTest;

TEST_F(DevToolsPageManifestTest, DevToolsExtensions) {
  LoadAndExpectError("devtools_extension_url_invalid_type.json",
                     extensions::manifest_errors::kInvalidDevToolsPage);

  LoadAndExpectError("devtools_extension_invalid_page_url.json",
                     extensions::manifest_errors::kInvalidDevToolsPage);

  LoadAndExpectError("devtools_extension_page_url_https.json",
                     extensions::manifest_errors::kInvalidDevToolsPage);

  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess("devtools_extension.json");
  EXPECT_EQ(extension->url().spec() + "devtools.html",
            extensions::chrome_manifest_urls::GetDevToolsPage(extension.get())
                .spec());
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .HasEffectiveAccessToAllHosts());
}

namespace extensions {

namespace {

class ManifestDevToolsPageHandlerTest : public ManifestTest {
 public:
  ManifestDevToolsPageHandlerTest() = default;

 protected:
  base::Value::Dict CreateManifest(const std::string& devtools_page) {
    return base::Value::Dict()
        .Set("name", "DevTools")
        .Set("version", "1")
        .Set("manifest_version", 3)
        .Set("devtools_page", devtools_page);
  }

  void LoadAndExpectSuccess(const std::string& id,
                            const std::string& devtools_page,
                            const std::string& parsed_devtools_page) {
    base::ScopedTempDir dir;
    std::string error;
    base::Value::Dict manifest = CreateManifest(devtools_page);
    ASSERT_TRUE(dir.CreateUniqueTempDir());
    scoped_refptr<Extension> extension =
        Extension::Create(dir.GetPath(), mojom::ManifestLocation::kInternal,
                          manifest, Extension::NO_FLAGS, id, &error);
    ASSERT_TRUE(extension.get());
    EXPECT_TRUE(error.empty());
    ASSERT_EQ(parsed_devtools_page,
              extensions::chrome_manifest_urls::GetDevToolsPage(extension.get())
                  .spec());
  }

  void LoadAndExpectSuccess(const std::string& id,
                            const std::string& devtools_page) {
    LoadAndExpectSuccess(id, devtools_page, devtools_page);
  }

  void LoadAndExpectError(const std::string& id,
                          const std::string& devtools_page,
                          const std::u16string& expected_error) {
    base::ScopedTempDir dir;
    std::string error;
    base::Value::Dict manifest = CreateManifest(devtools_page);
    ASSERT_TRUE(dir.CreateUniqueTempDir());
    scoped_refptr<Extension> extension =
        Extension::Create(dir.GetPath(), mojom::ManifestLocation::kInternal,
                          manifest, Extension::NO_FLAGS, id, &error);
    ASSERT_FALSE(extension);
    ASSERT_EQ(base::UTF16ToUTF8(expected_error), error);
  }

  void LoadAndExpectError(const std::string& devtools_page,
                          const std::u16string& expected_error) {
    LoadAndExpectError(std::string(), devtools_page, expected_error);
  }

  void ValidateAndExpectWarning(const std::string& id,
                                const std::string& devtools_page,
                                const std::string& expected_warning) {
    base::ScopedTempDir dir;
    std::string error;
    std::vector<InstallWarning> warnings;
    base::Value::Dict manifest = CreateManifest(devtools_page);
    ASSERT_TRUE(dir.CreateUniqueTempDir());
    scoped_refptr<Extension> extension =
        Extension::Create(dir.GetPath(), mojom::ManifestLocation::kInternal,
                          manifest, Extension::NO_FLAGS, id, &error);
    ASSERT_TRUE(extension);
    EXPECT_TRUE(
        ManifestHandler::ValidateExtension(extension.get(), &error, &warnings));
    EXPECT_TRUE(error.empty());
    ASSERT_EQ(1u, warnings.size());
    EXPECT_EQ(InstallWarning(expected_warning), warnings[0]);
  }

  void ValidateAndExpectWarning(const std::string& devtools_page,
                                const std::string& expected_warning) {
    ValidateAndExpectWarning(std::string(), devtools_page, expected_warning);
  }
};

}  // namespace

TEST_F(ManifestDevToolsPageHandlerTest, DevToolsPageImported) {
  // Specifying an absolute URL for an imported resource should fail, similar
  // to other remote sources.
  LoadAndExpectError("someid",
                     "chrome-extension://someid/_modules/"
                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/devtools.html",
                     std::u16string(manifest_errors::kInvalidDevToolsPage));

  // Specifying an absolute URL for a resouce with '_modules' in it but not
  // actually an imported resource should be fine
  LoadAndExpectSuccess("someid",
                       "chrome-extension://someid/_modules1/devtools.html");
}

TEST_F(ManifestDevToolsPageHandlerTest, DevTools) {
  LoadAndExpectSuccess("someid", "chrome-extension://someid/path");

  ValidateAndExpectWarning(
      "does-not-exist.html",
      ErrorUtils::FormatErrorMessage(manifest_errors::kFileNotFound,
                                     "does-not-exist.html"));
  ValidateAndExpectWarning(
      "/does-not-exist.html",
      ErrorUtils::FormatErrorMessage(manifest_errors::kFileNotFound,
                                     "does-not-exist.html"));
  ValidateAndExpectWarning(
      "aaaa", "_modules/aaaa/bbbb/index.html",
      ErrorUtils::FormatErrorMessage(manifest_errors::kFileNotFound,
                                     "_modules/aaaa/bbbb/index.html"));
  ValidateAndExpectWarning(
      "aaaa", "chrome-extension://aaaa/does-not-exist.html",
      ErrorUtils::FormatErrorMessage(manifest_errors::kFileNotFound,
                                     "does-not-exist.html"));

  // Specifying an absolute URL for a different extension's resource should
  // fail, similar to other remote sources.
  LoadAndExpectError("aaaa", "chrome-extension://bbbb/index.html",
                     std::u16string(manifest_errors::kInvalidDevToolsPage));
  LoadAndExpectError("wss://bad.example.com",
                     std::u16string(manifest_errors::kInvalidDevToolsPage));
  LoadAndExpectError("https://bad.example.com/dont_ever_do_this.html",
                     std::u16string(manifest_errors::kInvalidDevToolsPage));
}

}  // namespace extensions
