// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/status_icons/status_icon_mac.h"

#include <memory>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/ui/cocoa/status_icons/status_icons_features.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/menus/cocoa/menu_controller.h"

class SkBitmap;

class MockStatusIconObserver : public StatusIconObserver {
 public:
  MOCK_METHOD(void, OnStatusIconClicked, (), (override));
};

class StatusIconMacTest : public CocoaTest {
 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
};

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
  icon->SetContextMenu(std::move(model));
  ASSERT_EQ(1, icon->GetItemForTesting().menu.numberOfItems);

  NSMenuItem* menuItem = [icon->GetItemForTesting().menu itemAtIndex:0];
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
  icon->SetContextMenu(std::move(model));
  icon->SetToolTip(tool_tip);
  ASSERT_EQ(1, icon->GetItemForTesting().menu.numberOfItems);

  NSMenuItem* menu_item = [icon->GetItemForTesting().menu itemAtIndex:0];
  EXPECT_NSEQ(base::SysUTF16ToNSString(menu_title), menu_item.title);
}

TEST_F(StatusIconMacTest, SecondaryClickMenuNoToolTip) {
  // Create a status item with a secondary click menu and set a tool tip. Verify
  // `item.menu` stays populated (so VoiceOver and Ctrl+F8 can open it), the
  // tool tip is not inserted as a menu item, and both `toolTip` and
  // `accessibilityLabel` are populated.
  const char16_t menu_title[] = u"Menu Title";
  const char16_t tool_tip[] = u"Tool tip";
  std::unique_ptr<StatusIconMenuModel> model =
      std::make_unique<StatusIconMenuModel>(nullptr);
  model->AddItem(0, menu_title);

  std::unique_ptr<StatusIconMac> icon = std::make_unique<StatusIconMac>();
  icon->SetToolTip(tool_tip);
  icon->SetOpenMenuWithSecondaryClick(true);
  icon->SetContextMenu(std::move(model));
  ASSERT_EQ(1, icon->GetItemForTesting().menu.numberOfItems);

  NSMenuItem* menu_item = [icon->GetItemForTesting().menu itemAtIndex:0];
  EXPECT_NSEQ(base::SysUTF16ToNSString(menu_title), menu_item.title);
  EXPECT_NSEQ(base::SysUTF16ToNSString(tool_tip),
              icon->GetItemForTesting().button.toolTip);
  EXPECT_NSEQ(base::SysUTF16ToNSString(tool_tip),
              icon->GetItemForTesting().button.accessibilityLabel);
}

TEST_F(StatusIconMacTest, SecondaryClickMouseRouting) {
  std::unique_ptr<StatusIconMenuModel> model =
      std::make_unique<StatusIconMenuModel>(nullptr);
  model->AddItem(0, u"Menu Title");

  std::unique_ptr<StatusIconMac> icon = std::make_unique<StatusIconMac>();
  MockStatusIconObserver observer;
  icon->AddObserver(&observer);
  icon->SetOpenMenuWithSecondaryClick(true);
  icon->SetContextMenu(std::move(model));

  size_t open_menu_count = 0;
  icon->SetOpenMenuForTesting(
      base::BindLambdaForTesting([&] { ++open_menu_count; }));

  NSStatusBarButton* button = icon->GetItemForTesting().button;

  // Plain left-click dispatches a primary click without opening `item.menu`.
  NSEvent* left_mouse_down =
      [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
                         location:NSZeroPoint
                    modifierFlags:0
                        timestamp:0
                     windowNumber:0
                          context:nil
                      eventNumber:0
                       clickCount:1
                         pressure:1.0];
  EXPECT_CALL(observer, OnStatusIconClicked()).Times(1);
  [button mouseDown:left_mouse_down];
  testing::Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(0u, open_menu_count);

  // Control+left-click opens `item.menu` without dispatching a primary click.
  NSEvent* ctrl_left_mouse_down =
      [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
                         location:NSZeroPoint
                    modifierFlags:NSEventModifierFlagControl
                        timestamp:0
                     windowNumber:0
                          context:nil
                      eventNumber:0
                       clickCount:1
                         pressure:1.0];
  EXPECT_CALL(observer, OnStatusIconClicked()).Times(0);
  [button mouseDown:ctrl_left_mouse_down];
  EXPECT_EQ(1u, open_menu_count);

  // Right-click opens `item.menu` without dispatching a primary click.
  NSEvent* right_mouse_down =
      [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                         location:NSZeroPoint
                    modifierFlags:0
                        timestamp:0
                     windowNumber:0
                          context:nil
                      eventNumber:0
                       clickCount:1
                         pressure:1.0];
  [button rightMouseDown:right_mouse_down];
  EXPECT_EQ(2u, open_menu_count);

  // When `SetOpenMenuWithSecondaryClick(false)` is set, plain left-click opens
  // `item.menu` instead of dispatching a click event.
  icon->SetOpenMenuWithSecondaryClick(false);
  [button mouseDown:left_mouse_down];
  EXPECT_EQ(3u, open_menu_count);

  // Direct target/action invocation while a menu exists does not double-fire
  // `OnStatusIconClicked`.
  [button sendAction:button.action to:button.target];
  testing::Mock::VerifyAndClearExpectations(&observer);

  icon->RemoveObserver(&observer);
}

TEST_F(StatusIconMacTest, MenulessStatusIconClickAndAccessibilityPress) {
  std::unique_ptr<StatusIconMac> icon = std::make_unique<StatusIconMac>();
  MockStatusIconObserver observer;
  icon->AddObserver(&observer);

  EXPECT_CALL(observer, OnStatusIconClicked()).Times(1);
  [icon->GetItemForTesting().button performClick:nil];
  testing::Mock::VerifyAndClearExpectations(&observer);

  // `-[NSStatusBarButton accessibilityPerformPress]` dispatches the button's
  // target/action (`handleClick:`) on menu-less status items, though AppKit
  // returns `NO` in headless unit tests.
  EXPECT_CALL(observer, OnStatusIconClicked()).Times(1);
  [icon->GetItemForTesting().button accessibilityPerformPress];
  testing::Mock::VerifyAndClearExpectations(&observer);

  icon->RemoveObserver(&observer);
}

TEST_F(StatusIconMacTest, SetContextMenuNullClearsItemMenu) {
  std::unique_ptr<StatusIconMenuModel> model =
      std::make_unique<StatusIconMenuModel>(nullptr);
  model->AddItem(0, u"Menu Title");

  std::unique_ptr<StatusIconMac> icon = std::make_unique<StatusIconMac>();
  icon->SetOpenMenuWithSecondaryClick(true);
  icon->SetContextMenu(std::move(model));
  ASSERT_NE(nil, icon->GetItemForTesting().menu);
  EXPECT_TRUE(icon->HasStatusIconMenu());

  icon->SetContextMenu(nullptr);
  EXPECT_EQ(nil, icon->GetItemForTesting().menu);
  EXPECT_FALSE(icon->HasStatusIconMenu());
}

TEST_F(StatusIconMacTest, SecondaryClickMenuUpdatesOnModelStateChange) {
  std::unique_ptr<StatusIconMenuModel> model =
      std::make_unique<StatusIconMenuModel>(nullptr);
  model->AddItem(0, u"Open Gemini in Chrome");
  StatusIconMenuModel* raw_model = model.get();

  std::unique_ptr<StatusIconMac> icon = std::make_unique<StatusIconMac>();
  icon->SetOpenMenuWithSecondaryClick(true);
  icon->SetContextMenu(std::move(model));
  ASSERT_EQ(1, icon->GetItemForTesting().menu.numberOfItems);
  EXPECT_NSEQ(@"Open Gemini in Chrome",
              [icon->GetItemForTesting().menu itemAtIndex:0].title);

  raw_model->ChangeLabelForCommandId(0, u"Close Gemini in Chrome");
  ASSERT_EQ(1, icon->GetItemForTesting().menu.numberOfItems);
  EXPECT_NSEQ(@"Close Gemini in Chrome",
              [icon->GetItemForTesting().menu itemAtIndex:0].title);
}

TEST_F(StatusIconMacTest, ModelChangeWhileMenuOpenDefersRebuildUntilClosed) {
  class RecordingMenuDelegate : public StatusIconMenuModel::Delegate {
   public:
    void ExecuteCommand(int command_id, int event_flags) override {
      executed_commands_.push_back(command_id);
    }
    const std::vector<int>& executed_commands() const {
      return executed_commands_;
    }

   private:
    std::vector<int> executed_commands_;
  };

  RecordingMenuDelegate menu_delegate;
  std::unique_ptr<StatusIconMenuModel> model =
      std::make_unique<StatusIconMenuModel>(&menu_delegate);
  model->AddItem(0, u"Open Gemini in Chrome");
  StatusIconMenuModel* raw_model = model.get();

  std::unique_ptr<StatusIconMac> icon = std::make_unique<StatusIconMac>();
  icon->SetOpenMenuWithSecondaryClick(true);
  icon->SetContextMenu(std::move(model));

  NSMenu* initial_menu = nil;
  @autoreleasepool {
    initial_menu = icon->GetItemForTesting().menu;
    ASSERT_NE(nil, initial_menu);

    // Simulate AppKit opening `initial_menu` and starting menu tracking.
    [initial_menu.delegate menuWillOpen:initial_menu];

    // Mutating the model while open must not replace (and cancel)
    // `initial_menu`.
    raw_model->AddItem(1, u"Settings");
    EXPECT_EQ(initial_menu, icon->GetItemForTesting().menu);
    EXPECT_EQ(1, icon->GetItemForTesting().menu.numberOfItems);

    // AppKit fires `menuDidClose:` and `NSMenuDidEndTrackingNotification`
    // before dispatching the chosen `NSMenuItem`'s action (whose `target` is
    // weak). The closing `MenuControllerCocoa` must remain alive until the
    // item action runs.
    [initial_menu.delegate menuDidClose:initial_menu];
    [NSNotificationCenter.defaultCenter
        postNotificationName:NSMenuDidEndTrackingNotification
                      object:initial_menu];
  }
  @autoreleasepool {
    [initial_menu performActionForItemAtIndex:0];
  }
  EXPECT_THAT(menu_delegate.executed_commands(), testing::ElementsAre(0));

  // Once the UI run loop turns, the deferred rebuild applies automatically.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return icon->GetItemForTesting().menu != initial_menu; }));
  EXPECT_EQ(2, icon->GetItemForTesting().menu.numberOfItems);
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
  EXPECT_TRUE(icon->GetItemForTesting().visible);

  // The icon is hidden when notified that the window will enter fullscreen.
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSWindowWillEnterFullScreenNotification
                    object:nil];
  EXPECT_FALSE(icon->GetItemForTesting().visible);

  // The icon is shown when the window exits fullscreen.
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSWindowDidExitFullScreenNotification
                    object:nil];
  EXPECT_TRUE(icon->GetItemForTesting().visible);

  // If only notified when a window has entered fullscreen, the icon doesn't
  // change its visibility. This is because
  // NSWindowWillEnterFullScreenNotification should always precede
  // NSWindowDidEnterFullScreenNotification.
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSWindowDidEnterFullScreenNotification
                    object:nil];
  EXPECT_TRUE(icon->GetItemForTesting().visible);

  // Notification that the window is leaving fullscreen is not enough to display
  // the icon again. This is to prevent the non-interactive window from
  // appearing, and will be handled by NSWindowDidExitFullScreenNotification.
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSWindowWillEnterFullScreenNotification
                    object:nil];
  EXPECT_FALSE(icon->GetItemForTesting().visible);
  [NSNotificationCenter.defaultCenter
      postNotificationName:NSWindowWillExitFullScreenNotification
                    object:nil];
  EXPECT_FALSE(icon->GetItemForTesting().visible);
}
