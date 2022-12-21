// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/status_icons/status_icon_mac.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/base/resource/resource_bundle.h"

class SkBitmap;

using StatusIconMacTest = CocoaTest;

TEST_F(StatusIconMacTest, Create) {
  // Create an icon, set the tool tip, then shut it down (checks for leaks).
  std::unique_ptr<StatusIcon> icon = std::make_unique<StatusIconMac>();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image = rb.GetImageSkiaNamed(IDR_STATUS_TRAY_ICON);
  icon->SetImage(*image);
  icon->SetToolTip(u"tool tip");
}

TEST_F(StatusIconMacTest, CreateMenu) {
  // Create a menu and verify by getting the title of the first menu item.
  const char16_t menu_title[] = u"Menu Title";
  std::unique_ptr<StatusIconMenuModel> model =
      std::make_unique<StatusIconMenuModel>(nullptr);
  model->AddItem(0, menu_title);

  std::unique_ptr<StatusIconMac> icon = std::make_unique<StatusIconMac>();
  icon->UpdatePlatformContextMenu(model.get());
  EXPECT_EQ(1, icon->item().menu.numberOfItems);

  NSMenuItem* menuItem = [icon->item().menu itemAtIndex:0];
  EXPECT_NSEQ(base::SysUTF16ToNSString(menu_title), menuItem.title);
}

TEST_F(StatusIconMacTest, MenuToolTip) {
  // Create a menu and set a tool tip. Verify the tool tip is inserted as the
  // first menu item.
  const char16_t menu_title[] = u"Menu Title";
  const char16_t tool_tip[] = u"Tool tip";
  std::unique_ptr<StatusIconMenuModel> model =
      std::make_unique<StatusIconMenuModel>(nullptr);
  model->AddItem(0, menu_title);

  std::unique_ptr<StatusIconMac> icon = std::make_unique<StatusIconMac>();
  icon->UpdatePlatformContextMenu(model.get());
  icon->SetToolTip(tool_tip);
  EXPECT_EQ(2, icon->item().menu.numberOfItems);

  NSMenuItem* tool_tip_item = [icon->item().menu itemAtIndex:0];
  EXPECT_NSEQ(base::SysUTF16ToNSString(tool_tip), tool_tip_item.title);
  NSMenuItem* menu_item = [icon->item().menu itemAtIndex:1];
  EXPECT_NSEQ(base::SysUTF16ToNSString(menu_title), menu_item.title);
}
