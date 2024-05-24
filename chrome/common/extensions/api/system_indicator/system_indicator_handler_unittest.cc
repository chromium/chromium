// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/system_indicator/system_indicator_handler.h"

#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

// The manifest permissions for system_indicator have an allowlist. This is the
// test key.
constexpr char kKey[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC9fDu8apG3Dz72XTT3Ym1SfGt06tdowTlYQ+"
    "3lGlCbVpfnMOmewgRgYxzUtUPso9aQERZcmI2+7UtbWjtk6/usl9Hr7a1JBQwfaUoUygEe56aj"
    "UeZhe/ErkH5CXT84U0pokfPr5vMvc7RVPduU+UBiF0DnGb/hSpzz/1UhJ5H9AwIDAQAB";

}  // anonymous namespace

using SystemIndicatorHandlerTest = ManifestTest;

TEST_F(SystemIndicatorHandlerTest, BasicTests) {
  // Simple icon path.
  {
    constexpr char kManifest[] =
        R"({
             "name": "System Indicator",
             "manifest_version": 2,
             "version": "0.1",
             "key": "%s",
             "system_indicator": { "default_icon": "icon.png" }
           })";
    scoped_refptr<const Extension> extension = LoadAndExpectSuccess(
        ManifestData(base::test::ParseJson(base::StringPrintf(kManifest, kKey))
                         .TakeDict()));
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
             "key": "%s",
             "system_indicator": {
               "default_icon": {
                 "24": "icon24.png",
                 "48": "icon48.png",
                 "79": "icon79.png"
               }
             }
           })";
    scoped_refptr<const Extension> extension = LoadAndExpectSuccess(
        ManifestData(base::test::ParseJson(base::StringPrintf(kManifest, kKey))
                         .TakeDict()));
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
             "key": "%s",
             "system_indicator": {}
           })";
    scoped_refptr<const Extension> extension = LoadAndExpectSuccess(
        ManifestData(base::test::ParseJson(base::StringPrintf(kManifest, kKey))
                         .TakeDict()));
    ASSERT_TRUE(extension);
    const ExtensionIconSet* icon =
        SystemIndicatorHandler::GetSystemIndicatorIcon(*extension);
    ASSERT_TRUE(icon);
    EXPECT_TRUE(icon->empty());
  }
}

}  // namespace extensions
