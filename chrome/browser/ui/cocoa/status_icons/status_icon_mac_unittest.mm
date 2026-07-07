// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/status_icons/status_icon_mac.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/ui/cocoa/status_icons/status_icons_features.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/menus/cocoa/menu_controller.h"

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
  ASSERT_EQ(1, icon->item().menu.numberOfItems);

  NSMenuItem* menuItem = [icon->item().menu itemAtIndex:0];
  EXPECT_NSEQ(base::SysUTF16ToNSString(menu_title), menuItem.title);
}

TEST_F(StatusIconMacTest, MenuToolTip) {
  // Create a menu and set a tool tip. Verify the tool tip is not inserted as
  // the first menu item.
  const char16_t menu_title[] = u"Menu Title";
  const char16_t tool_tip[] = u"Tool tip";
  std::unique_ptr<StatusIconMenuModel> model =
      std::make_unique<StatusIconMenuModel>(nullptr);
  model->AddItem(0, menu_title);

  std::unique_ptr<StatusIconMac> icon = std::make_unique<StatusIconMac>();
  icon->UpdatePlatformContextMenu(model.get());
  icon->SetToolTip(tool_tip);
  ASSERT_EQ(1, icon->item().menu.numberOfItems);

  NSMenuItem* menu_item = [icon->item().menu itemAtIndex:0];
  EXPECT_NSEQ(base::SysUTF16ToNSString(menu_title), menu_item.title);
}

TEST_F(StatusIconMacTest, SecondaryClickMenuNoToolTip) {
  // Create a status item with a secondary click menu and set a tool tip. Verify
  // the tool tip is not inserted as the first menu item.
  const char16_t menu_title[] = u"Menu Title";
  const char16_t tool_tip[] = u"Tool tip";
  std::unique_ptr<StatusIconMenuModel> model =
      std::make_unique<StatusIconMenuModel>(nullptr);
  model->AddItem(0, menu_title);

  std::unique_ptr<StatusIconMac> icon = std::make_unique<StatusIconMac>();
  icon->SetToolTip(tool_tip);
  icon->SetOpenMenuWithSecondaryClick(true);
  icon->SetContextMenu(std::move(model));
  ASSERT_EQ(0, icon->menu_.menu.numberOfItems);

  EXPECT_NSEQ(base::SysUTF16ToNSString(tool_tip), icon->item().button.toolTip);
}

// Regression test for crbug.com/494614152.
TEST_F(StatusIconMacTest, Os26_StatusIconHiddenInFullscreen) {
  if (!@available(macOS 26, *)) {
    GTEST_SKIP() << "This test only applies to macOS 26 (Tahoe).";
  }
  if (@available(macOS 27, *)) {
    GTEST_SKIP() << "This test only applies to macOS 26 (Tahoe).";
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kHideStatusIconMacInFullscreen);

  std::unique_ptr<StatusIconMac> icon = std::make_unique<StatusIconMac>();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image = rb.GetImageSkiaNamed(IDR_STATUS_TRAY_ICON);
  icon->SetImage(*image);
  icon->SetVisible(true);
  EXPECT_TRUE(icon->item().visible);

  // The icon is hidden when notified that the window will enter fullscreen.
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSWindowWillEnterFullScreenNotification
                    object:nil];
  EXPECT_FALSE(icon->item().visible);

  // The icon is shown when the window exits fullscreen.
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSWindowDidExitFullScreenNotification
                    object:nil];
  EXPECT_TRUE(icon->item().visible);

  // If only notified when a window has entered fullscreen, the icon doesn't
  // change its visibility. This is because
  // NSWindowWillEnterFullScreenNotification should always precede
  // NSWindowDidEnterFullScreenNotification.
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSWindowDidEnterFullScreenNotification
                    object:nil];
  EXPECT_TRUE(icon->item().visible);

  // Notification that the window is leaving fullscreen is not enough to display
  // the icon again. This is to prevent the non-interactive window from
  // appearing, and will be handled by NSWindowDidExitFullScreenNotification.
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSWindowWillEnterFullScreenNotification
                    object:nil];
  EXPECT_FALSE(icon->item().visible);
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSWindowWillExitFullScreenNotification
                    object:nil];
  EXPECT_FALSE(icon->item().visible);
}
