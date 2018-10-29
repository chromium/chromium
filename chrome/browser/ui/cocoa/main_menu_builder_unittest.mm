// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/main_menu_builder.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

using chrome::internal::MenuItemBuilder;

TEST(MainMenuBuilderTest, Separator) {
  base::scoped_nsobject<NSMenuItem> item =
      MenuItemBuilder().is_separator().Build();
  EXPECT_TRUE([item isSeparatorItem]);
  EXPECT_EQ(0, [item tag]);
}

TEST(MainMenuBuilderTest, SeparatorWithTag) {
  base::scoped_nsobject<NSMenuItem> item =
      MenuItemBuilder().is_separator().tag(999).Build();
  EXPECT_TRUE([item isSeparatorItem]);
  EXPECT_EQ(999, [item tag]);
}

TEST(MainMenuBuilderTest, CommandId) {
  base::scoped_nsobject<NSMenuItem> item =
      MenuItemBuilder(IDS_NEW_TAB).command_id(IDC_NEW_TAB).Build();
  EXPECT_EQ(@selector(commandDispatch:), [item action]);
  EXPECT_FALSE([item target]);
  EXPECT_NSEQ(l10n_util::GetNSStringWithFixup(IDS_NEW_TAB), [item title]);
  EXPECT_EQ(IDC_NEW_TAB, [item tag]);
  EXPECT_NSEQ(@"t", [item keyEquivalent]);
  EXPECT_EQ(NSEventModifierFlagCommand, [item keyEquivalentModifierMask]);
}

TEST(MainMenuBuilderTest, CustomTargetAction) {
  base::scoped_nsobject<NSObject> target([[NSObject alloc] init]);

  base::scoped_nsobject<NSMenuItem> item = MenuItemBuilder(IDS_PREFERENCES)
                                               .target(target)
                                               .action(@selector(fooBar:))
                                               .Build();
  EXPECT_NSEQ(l10n_util::GetNSStringWithFixup(IDS_PREFERENCES), [item title]);

  EXPECT_EQ(target.get(), [item target]);
  EXPECT_EQ(@selector(fooBar:), [item action]);
  EXPECT_EQ(0, [item tag]);
}

TEST(MainMenuBuilderTest, Submenu) {
  base::scoped_nsobject<NSMenuItem> item =
      MenuItemBuilder(IDS_EDIT)
          .tag(123)
          .submenu({
            MenuItemBuilder(IDS_CUT).tag(456).action(@selector(first:)),
                MenuItemBuilder(IDS_COPY).tag(789).action(@selector(second:)),
          })
          .Build();

  EXPECT_EQ(123, [item tag]);
  EXPECT_NSEQ(l10n_util::GetNSStringWithFixup(IDS_EDIT), [item title]);
  // These are hooked up by AppKit's -setSubmenu:.
  EXPECT_EQ([item submenu], [item target]);
  EXPECT_EQ(@selector(submenuAction:), [item action]);

  NSMenu* submenu = [item submenu];
  EXPECT_TRUE(submenu);

  ASSERT_EQ(2u, [submenu numberOfItems]);

  NSMenuItem* subitem = [submenu itemAtIndex:0];
  EXPECT_EQ(456, [subitem tag]);
  EXPECT_EQ(@selector(first:), [subitem action]);
  EXPECT_NSEQ(l10n_util::GetNSStringWithFixup(IDS_CUT), [subitem title]);

  subitem = [submenu itemAtIndex:1];
  EXPECT_EQ(789, [subitem tag]);
  EXPECT_EQ(@selector(second:), [subitem action]);
  EXPECT_NSEQ(l10n_util::GetNSStringWithFixup(IDS_COPY), [subitem title]);
}

TEST(MainMenuBuilderTest, StringId) {
  base::scoped_nsobject<NSMenuItem> item =
      MenuItemBuilder(IDS_NEW_TAB_MAC).Build();
  EXPECT_NSEQ(l10n_util::GetNSStringWithFixup(IDS_NEW_TAB_MAC), [item title]);
}

TEST(MainMenuBuilderTest, StringIdWithArg) {
  base::string16 product_name(base::ASCIIToUTF16("MyAppIsTotallyAwesome"));
  base::scoped_nsobject<NSMenuItem> item =
      MenuItemBuilder(IDS_ABOUT_MAC).string_format_1(product_name).Build();

  EXPECT_NSEQ(l10n_util::GetNSStringF(IDS_ABOUT_MAC, product_name),
              [item title]);
}

TEST(MainMenuBuilderTest, Disabled) {
  base::scoped_nsobject<NSMenuItem> item =
      MenuItemBuilder(IDS_NEW_TAB_MAC).remove_if(true).Build();
  EXPECT_EQ(nil, item.get());

  item = MenuItemBuilder(IDS_NEW_TAB_MAC).remove_if(false).Build();
  EXPECT_NSEQ(l10n_util::GetNSStringWithFixup(IDS_NEW_TAB_MAC), [item title]);
}

}  // namespace
