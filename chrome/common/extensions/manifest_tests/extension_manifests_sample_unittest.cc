// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"

namespace extensions {

TEST_F(ChromeManifestTest, PlatformsKey) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("platforms_key.json");
  EXPECT_EQ(0u, extension->install_warnings().size());
}

TEST_F(ChromeManifestTest, UnrecognizedKeyWarning) {
  scoped_refptr<Extension> extension =
      LoadAndExpectWarning("unrecognized_key.json",
                           "Unrecognized manifest key 'unrecognized_key_1'.");
}

// Tests that known, ignored keys do not emit an unrecognized key warning.
TEST_F(ChromeManifestTest, IgnoredUnrecognizedKeyNoWarning) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("ignored_unrecognized_key.json");
  EXPECT_EQ(0u, extension->install_warnings().size());
}

// Tests that using the deprecated "plugins" key causes an install warning.
TEST_F(ChromeManifestTest, DeprecatedPluginsKey) {
  LoadAndExpectWarning("deprecated_plugins_key.json",
                       "Unrecognized manifest key 'plugins'.");
}

}  // namespace extensions
