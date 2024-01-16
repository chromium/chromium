// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/webview_info.h"

using extensions::ErrorUtils;
using extensions::Extension;
using extensions::WebviewInfo;
namespace errors = extensions::manifest_errors;

using WebviewAccessibleResourcesManifestTest = ChromeManifestTest;

TEST_F(WebviewAccessibleResourcesManifestTest, WebviewAccessibleResources) {
  // Manifest version 2 with webview accessible resources specified.
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("webview_accessible_resources_1.json"));

  EXPECT_FALSE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                        "fail", "a.html"));
  EXPECT_FALSE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                        "fail", "b.html"));
  EXPECT_FALSE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                        "fail", "c.html"));
  EXPECT_FALSE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                        "fail", "d.html"));

  EXPECT_TRUE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                       "foo", "a.html"));
  EXPECT_TRUE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                       "foo", "b.html"));
  EXPECT_FALSE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                        "foo", "c.html"));
  EXPECT_FALSE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                        "foo", "d.html"));

  EXPECT_TRUE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                       "bar", "a.html"));
  EXPECT_FALSE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                        "bar", "b.html"));
  EXPECT_TRUE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                       "bar", "c.html"));
  EXPECT_FALSE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                        "bar", "d.html"));

  EXPECT_TRUE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                       "foobar", "a.html"));
  EXPECT_TRUE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                       "foobar", "b.html"));
  EXPECT_TRUE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                       "foobar", "c.html"));
  EXPECT_FALSE(WebviewInfo::IsResourceWebviewAccessible(extension.get(),
                                                        "foobar", "d.html"));

  EXPECT_FALSE(WebviewInfo::IsResourceWebviewAccessible(nullptr,
                                                        "foobar", "a.html"));
}

TEST_F(WebviewAccessibleResourcesManifestTest, InvalidManifest) {
  LoadAndExpectError("webview_accessible_resources_invalid1.json",
                      errors::kInvalidWebview);
  LoadAndExpectError("webview_accessible_resources_invalid2.json",
                      errors::kInvalidWebviewPartitionsList);
  LoadAndExpectError("webview_accessible_resources_invalid3.json",
                      errors::kInvalidWebviewPartitionsList);
  LoadAndExpectError(
      "webview_accessible_resources_invalid4.json",
      ErrorUtils::FormatErrorMessage(errors::kInvalidWebviewPartition,
                                     base::NumberToString(0)));
  LoadAndExpectError("webview_accessible_resources_invalid5.json",
                     errors::kInvalidWebviewPartitionName);
  LoadAndExpectError("webview_accessible_resources_invalid6.json",
                     errors::kInvalidWebviewAccessibleResourcesList);
  LoadAndExpectError("webview_accessible_resources_invalid7.json",
                     errors::kInvalidWebviewAccessibleResourcesList);
  LoadAndExpectError(
      "webview_accessible_resources_invalid8.json",
      ErrorUtils::FormatErrorMessage(errors::kInvalidWebviewAccessibleResource,
                                     base::NumberToString(0)));

  {
    // Specifying non-relative paths as accessible resources should fail. We
    // raise a warning rather than a hard-error because existing apps do this
    // and we don't want to break them for all existing users.
    // https://crbug.com/856948.
    scoped_refptr<const Extension> extension = LoadAndExpectWarning(
        "webview_accessible_resources_non_relative_path.json",
        ErrorUtils::FormatErrorMessage(
            errors::kInvalidWebviewAccessibleResource,
            base::NumberToString(0)));
    EXPECT_FALSE(WebviewInfo::IsResourceWebviewAccessible(
        extension.get(), "nonrelative", "a.html"));
  }
}
