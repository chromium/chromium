// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gtest/include/gtest/gtest.h"

class DevToolsPageManifestTest : public ChromeManifestTest {
};

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

TEST_F(DevToolsPageManifestTest, DevToolsPageAbsoluteUrl) {
  auto manifest = base::Value::Dict()
                      .Set("name", "test")
                      .Set("version", "1")
                      .Set("manifest_version", 3)
                      .Set("devtools_page", "chrome-extension://someid/path");
  std::string error;
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  scoped_refptr<extensions::Extension> extension =
      extensions::Extension::Create(
          dir.GetPath(), extensions::mojom::ManifestLocation::kInternal,
          manifest, extensions::Extension::NO_FLAGS, "someid", &error);
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(error.empty());

  // Specifying an absolute URL for a different extension's resource should
  // fail, similar to other remote sources.
  extension = extensions::Extension::Create(
      dir.GetPath(), extensions::mojom::ManifestLocation::kInternal, manifest,
      extensions::Extension::NO_FLAGS, "otherid", &error);
  ASSERT_FALSE(extension);
  ASSERT_FALSE(error.empty());
  EXPECT_EQ(
      base::UTF16ToUTF8(extensions::manifest_errors::kInvalidDevToolsPage),
      error);
}
