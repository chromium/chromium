// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/api/extension_action/action_info_test_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_constants.h"
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
              base::Value::Dict()
                  .Set("name", "No default properties")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("browser_action",
                       base::Value::Dict().Set("default_title", "Title")))
          .Build();

  ASSERT_TRUE(extension.get());
  const ActionInfo* browser_action_info =
      GetActionInfoOfType(*extension, ActionInfo::Type::kBrowser);
  ASSERT_TRUE(browser_action_info);
  EXPECT_TRUE(browser_action_info->default_icon.empty());
}

TEST_F(BrowserActionManifestTest,
       BrowserActionManifestIcons_StringDefaultIcon) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", "String default icon")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("browser_action",
                       base::Value::Dict().Set("default_icon", "icon.png")))
          .Build();

  ASSERT_TRUE(extension.get());
  const ActionInfo* browser_action_info =
      GetActionInfoOfType(*extension, ActionInfo::Type::kBrowser);
  ASSERT_TRUE(browser_action_info);
  ASSERT_FALSE(browser_action_info->default_icon.empty());

  const ExtensionIconSet& icons = browser_action_info->default_icon;

  EXPECT_EQ(1u, icons.map().size());
  EXPECT_EQ("icon.png", icons.Get(extension_misc::EXTENSION_ICON_GIGANTOR,
                                  ExtensionIconSet::Match::kExactly));
}

TEST_F(BrowserActionManifestTest,
       BrowserActionManifestIcons_DictDefaultIcon) {
  // Arbitrary sizes should be allowed (useful for various scale factors).
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", "Dictionary default icon")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("browser_action",
                       base::Value::Dict().Set("default_icon",
                                               base::Value::Dict()
                                                   .Set("19", "icon19.png")
                                                   .Set("24", "icon24.png")
                                                   .Set("38", "icon38.png"))))
          .Build();

  ASSERT_TRUE(extension.get());
  const ActionInfo* browser_action_info =
      GetActionInfoOfType(*extension, ActionInfo::Type::kBrowser);
  ASSERT_TRUE(browser_action_info);
  ASSERT_FALSE(browser_action_info->default_icon.empty());

  const ExtensionIconSet& icons = browser_action_info->default_icon;

  // 24px icon should be included.
  EXPECT_EQ(3u, icons.map().size());
  EXPECT_EQ("icon19.png", icons.Get(19, ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icon24.png", icons.Get(24, ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("icon38.png", icons.Get(38, ExtensionIconSet::Match::kExactly));
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
