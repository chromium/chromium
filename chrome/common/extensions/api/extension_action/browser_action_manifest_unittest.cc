// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/test/values_test_util.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/api/extension_action/action_info_test_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

namespace {

class BrowserActionManifestTest : public ChromeManifestTest {
};

TEST_F(BrowserActionManifestTest,
       BrowserActionManifestIcons_NoDefaultIcons) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "No default properties")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set(
                      "browser_action",
                      DictionaryBuilder().Set("default_title", "Title").Build())
                  .Build())
          .Build();

  ASSERT_TRUE(extension.get());
  const ActionInfo* browser_action_info =
      GetActionInfoOfType(*extension, ActionInfo::TYPE_BROWSER);
  ASSERT_TRUE(browser_action_info);
  EXPECT_TRUE(browser_action_info->default_icon.empty());
}

TEST_F(BrowserActionManifestTest,
       BrowserActionManifestIcons_StringDefaultIcon) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "String default icon")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("browser_action", DictionaryBuilder()
                                             .Set("default_icon", "icon.png")
                                             .Build())
                  .Build())
          .Build();

  ASSERT_TRUE(extension.get());
  const ActionInfo* browser_action_info =
      GetActionInfoOfType(*extension, ActionInfo::TYPE_BROWSER);
  ASSERT_TRUE(browser_action_info);
  ASSERT_FALSE(browser_action_info->default_icon.empty());

  const ExtensionIconSet& icons = browser_action_info->default_icon;

  EXPECT_EQ(1u, icons.map().size());
  EXPECT_EQ("icon.png", icons.Get(extension_misc::EXTENSION_ICON_GIGANTOR,
                                  ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(BrowserActionManifestTest,
       BrowserActionManifestIcons_DictDefaultIcon) {
  // Arbitrary sizes should be allowed (useful for various scale factors).
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "Dictionary default icon")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("browser_action",
                       DictionaryBuilder()
                           .Set("default_icon", DictionaryBuilder()
                                                    .Set("19", "icon19.png")
                                                    .Set("24", "icon24.png")
                                                    .Set("38", "icon38.png")
                                                    .Build())
                           .Build())
                  .Build())
          .Build();

  ASSERT_TRUE(extension.get());
  const ActionInfo* browser_action_info =
      GetActionInfoOfType(*extension, ActionInfo::TYPE_BROWSER);
  ASSERT_TRUE(browser_action_info);
  ASSERT_FALSE(browser_action_info->default_icon.empty());

  const ExtensionIconSet& icons = browser_action_info->default_icon;

  // 24px icon should be included.
  EXPECT_EQ(3u, icons.map().size());
  EXPECT_EQ("icon19.png", icons.Get(19, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon24.png", icons.Get(24, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("icon38.png", icons.Get(38, ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(BrowserActionManifestTest,
       BrowserActionManifestIcons_InvalidDefaultIcon) {
  base::Value manifest_value = base::test::ParseJson(R"(
      {
        "name": "Invalid default icon",
        "version": "1.0.0",
        "manifest_version": 2,
        "browser_action": {
          "default_icon": {
            "19": "",  // Invalid value.
            "24": "icon24.png",
            "38": "icon38.png"
          }
        }
      })");
  ASSERT_TRUE(manifest_value.is_dict());
  std::u16string error =
      ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidIconPath, "19");
  LoadAndExpectError(ManifestData(std::move(manifest_value).TakeDict()),
                     errors::kInvalidIconPath);
}

}  // namespace
}  // namespace extensions
