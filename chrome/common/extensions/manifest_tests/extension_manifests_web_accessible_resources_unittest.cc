// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::WebAccessibleResourcesInfo;

class WebAccessibleResourcesManifestTest : public ChromeManifestTest {
};

TEST_F(WebAccessibleResourcesManifestTest, WebAccessibleResources) {
  // No web_accessible_resources.
  scoped_refptr<Extension> none(
      LoadAndExpectSuccess("web_accessible_resources/v2/none.json"));
  EXPECT_FALSE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(none.get()));
  EXPECT_FALSE(
      WebAccessibleResourcesInfo::IsResourceWebAccessible(none.get(), "test"));

  // web_accessible_resources: ["test"].
  scoped_refptr<Extension> single(
      LoadAndExpectSuccess("web_accessible_resources/v2/single.json"));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(single.get()));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(single.get(),
                                                                  "test"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(single.get(),
                                                                   "other"));

  // web_accessible_resources: ["*"].
  scoped_refptr<Extension> wildcard(
      LoadAndExpectSuccess("web_accessible_resources/v2/wildcard.json"));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(wildcard.get()));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      wildcard.get(), "anything"));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      wildcard.get(), "path/anything"));

  // web_accessible_resources: ["path/*.ext"].
  scoped_refptr<Extension> pattern(
      LoadAndExpectSuccess("web_accessible_resources/v2/pattern.json"));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(pattern.get()));
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      pattern.get(), "path/anything.ext"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      pattern.get(), "anything.ext"));
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      pattern.get(), "path/anything.badext"));
}
