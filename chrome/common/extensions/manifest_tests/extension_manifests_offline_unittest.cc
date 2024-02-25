// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

using ExtensionManifestOfflineEnabledTest = ChromeManifestTest;

TEST_F(ExtensionManifestOfflineEnabledTest, OfflineEnabled) {
  LoadAndExpectError("offline_enabled_invalid.json",
                     errors::kInvalidOfflineEnabled);
  scoped_refptr<Extension> extension_0(
      LoadAndExpectSuccess("offline_enabled_extension.json"));
  EXPECT_TRUE(OfflineEnabledInfo::IsOfflineEnabled(extension_0.get()));
  scoped_refptr<Extension> extension_1(
      LoadAndExpectSuccess("offline_enabled_packaged_app.json"));
  EXPECT_TRUE(OfflineEnabledInfo::IsOfflineEnabled(extension_1.get()));
  scoped_refptr<Extension> extension_2(
      LoadAndExpectSuccess("offline_disabled_packaged_app.json"));
  EXPECT_FALSE(OfflineEnabledInfo::IsOfflineEnabled(extension_2.get()));
  scoped_refptr<Extension> extension_3(
      LoadAndExpectSuccess("offline_default_packaged_app.json"));
  EXPECT_FALSE(OfflineEnabledInfo::IsOfflineEnabled(extension_3.get()));
  scoped_refptr<Extension> extension_4(
      LoadAndExpectSuccess("offline_enabled_hosted_app.json"));
  EXPECT_TRUE(OfflineEnabledInfo::IsOfflineEnabled(extension_4.get()));
  scoped_refptr<Extension> extension_5(
      LoadAndExpectSuccess("offline_default_platform_app.json"));
  EXPECT_TRUE(OfflineEnabledInfo::IsOfflineEnabled(extension_5.get()));
  scoped_refptr<Extension> extension_6(
      LoadAndExpectSuccess("offline_default_platform_app_with_webview.json"));
  EXPECT_FALSE(OfflineEnabledInfo::IsOfflineEnabled(extension_6.get()));
}

}  // namespace extensions
