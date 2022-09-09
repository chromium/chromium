// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class IconsManifestTest : public ChromeManifestTest {
};

TEST_F(IconsManifestTest, NormalizeIconPaths) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("normalize_icon_paths.json"));
  const ExtensionIconSet& icons = IconsInfo::GetIcons(extension.get());

  EXPECT_EQ("16.png", icons.Get(extension_misc::EXTENSION_ICON_BITTY,
                                ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("48.png", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                                ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(IconsManifestTest, IconSizes) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("init_icon_size.json"));
  const ExtensionIconSet& icons = IconsInfo::GetIcons(extension.get());

  EXPECT_EQ("16.png", icons.Get(extension_misc::EXTENSION_ICON_BITTY,
                                ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("24.png", icons.Get(extension_misc::EXTENSION_ICON_SMALLISH,
                                ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("32.png", icons.Get(extension_misc::EXTENSION_ICON_SMALL,
                                ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("48.png", icons.Get(extension_misc::EXTENSION_ICON_MEDIUM,
                                ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("128.png", icons.Get(extension_misc::EXTENSION_ICON_LARGE,
                                 ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("256.png", icons.Get(extension_misc::EXTENSION_ICON_EXTRA_LARGE,
                                 ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("512.png", icons.Get(extension_misc::EXTENSION_ICON_GIGANTOR,
                                 ExtensionIconSet::MATCH_EXACTLY));

  // Any old size will be accepted.
  EXPECT_EQ("300.png", IconsInfo::GetIcons(extension.get())
                           .Get(300, ExtensionIconSet::MATCH_EXACTLY));
}

}  // namespace extensions
