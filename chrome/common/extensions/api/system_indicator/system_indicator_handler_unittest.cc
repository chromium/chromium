// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/system_indicator/system_indicator_handler.h"

#include "base/test/values_test_util.h"
#include "components/version_info/channel.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

using SystemIndicatorHandlerTest = ManifestTest;

TEST_F(SystemIndicatorHandlerTest, BasicTests) {
  ScopedCurrentChannel current_channel(version_info::Channel::DEV);

  // Simple icon path.
  {
    constexpr char kManifest[] =
        R"({
             "name": "System Indicator",
             "manifest_version": 2,
             "version": "0.1",
             "system_indicator": { "default_icon": "icon.png" }
           })";
    scoped_refptr<const Extension> extension = LoadAndExpectSuccess(
        ManifestData(base::test::ParseJson(kManifest), "icon"));
    ASSERT_TRUE(extension);
    const ExtensionIconSet* icon =
        SystemIndicatorHandler::GetSystemIndicatorIcon(*extension);
    ASSERT_TRUE(icon);
    // Make a copy of the map since [] is more readable than find() for
    // comparing values.
    ExtensionIconSet::IconMap icon_map = icon->map();
    EXPECT_THAT(icon_map,
                testing::ElementsAre(std::make_pair(
                    extension_misc::EXTENSION_ICON_GIGANTOR, "icon.png")));
  }

  // Icon dictionary.
  {
    constexpr char kManifest[] =
        R"({
             "name": "System Indicator",
             "manifest_version": 2,
             "version": "0.1",
             "system_indicator": {
               "default_icon": {
                 "24": "icon24.png",
                 "48": "icon48.png",
                 "79": "icon79.png"
               }
             }
           })";
    scoped_refptr<const Extension> extension = LoadAndExpectSuccess(
        ManifestData(base::test::ParseJson(kManifest), "icon"));
    ASSERT_TRUE(extension);
    const ExtensionIconSet* icon =
        SystemIndicatorHandler::GetSystemIndicatorIcon(*extension);
    ASSERT_TRUE(icon);
    // Make a copy of the map since [] is more readable than find() for
    // comparing values.
    ExtensionIconSet::IconMap icon_map = icon->map();
    EXPECT_THAT(icon_map,
                testing::ElementsAre(std::make_pair(24, "icon24.png"),
                                     std::make_pair(48, "icon48.png"),
                                     std::make_pair(79, "icon79.png")));
  }

  // Empty dictionary.
  {
    constexpr char kManifest[] =
        R"({
             "name": "System Indicator",
             "manifest_version": 2,
             "version": "0.1",
             "system_indicator": {}
           })";
    scoped_refptr<const Extension> extension = LoadAndExpectSuccess(
        ManifestData(base::test::ParseJson(kManifest), "icon"));
    ASSERT_TRUE(extension);
    const ExtensionIconSet* icon =
        SystemIndicatorHandler::GetSystemIndicatorIcon(*extension);
    ASSERT_TRUE(icon);
    EXPECT_TRUE(icon->empty());
  }
}

}  // namespace extensions
