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

// Succeed when all keys are defined.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV3AllKeysDefined) {
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      "web_accessible_resources/v3/all_keys_defined.json"));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension.get()));
}

// Error if objects has no keys.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV3EmptyObject) {
  LoadAndExpectError("web_accessible_resources/v3/empty_object.json",
                     "Invalid value for 'web_accessible_resources[0]'.");
}

// Error if entry only contains |resources|.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV3MissingAllButResources) {
  LoadAndExpectError(
      "web_accessible_resources/v3/missing_all_but_resources.json",
      "Invalid value for 'web_accessible_resources[0]'.");
}

// Succeed if only specifying |extension_ids| and |resources|.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV3ExtensionIdsOnly) {
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      "web_accessible_resources/v3/extension_ids_only.json"));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension.get()));
}

// Succeed if only specifying |matches| and |resources|.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV3MatchesOnly) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("web_accessible_resources/v3/matches_only.json"));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension.get()));
}

// Succeed if there are multiple objects.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV3MultipleObjects) {
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      "web_accessible_resources/v3/multiple_objects.json"));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension.get()));
}

// Error if incorrect keyed object type is present.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV3MatchesObjectInsteadOfArray) {
  LoadAndExpectError(
      "web_accessible_resources/v3/matches_object_instead_of_array.json",
      "Invalid value for 'web_accessible_resources[0]'.");
}

// Error if incorrect keyed object type is present.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV3MatchesOrIdHasNonString) {
  LoadAndExpectError(
      "web_accessible_resources/v3/matches_or_id_has_non_string.json",
      "Invalid value for 'web_accessible_resources[0]'.");
}

// Succeed if unexpected key exists in entry.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV3UnexpectedKey) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("web_accessible_resources/v3/unexpected_key.json"));
  EXPECT_TRUE(
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension.get()));
}

// Succeed if only the use_dynamic_url key is set, but not others.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV3UseDynamicUrlIsSetButNothingElse) {
  LoadAndExpectSuccess(
      "web_accessible_resources/v3/"
      "use_dynamic_url_is_set_but_nothing_else.json");
}

// Error if web_accessible_resources key is of incorrect type.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV3InvalidTopLevelType) {
  LoadAndExpectError("web_accessible_resources/v3/invalid_top_level_type.json",
                     "Invalid value for 'web_accessible_resources'.");
}

// Error if extension id is invalid.
TEST_F(WebAccessibleResourcesManifestTest,
       WebAccessibleResourcesV3ExtensionIdInvalid) {
  LoadAndExpectError("web_accessible_resources/v3/extension_id_invalid.json",
                     "Invalid value for 'web_accessible_resources[0]'.");
}
