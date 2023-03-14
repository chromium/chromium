// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/history_menu_cocoa_controller.h"

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/cocoa/history_menu_bridge.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface FakeHistoryMenuController : HistoryMenuCocoaController {
 @public
  BOOL _opened[3];
}
@end

@implementation FakeHistoryMenuController

- (instancetype)initTest {
  if ((self = [super init])) {
    _opened[1] = NO;
    _opened[2] = NO;
  }
  return self;
}

- (void)openURLForItem:(const HistoryMenuBridge::HistoryItem*)item {
  _opened[item->session_id.id()] = YES;
}

@end  // FakeHistoryMenuController

class HistoryMenuCocoaControllerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    ASSERT_TRUE(profile());

    bridge_ = std::make_unique<HistoryMenuBridge>(profile());
    bridge_->controller_.reset(
        [[FakeHistoryMenuController alloc] initWithBridge:bridge_.get()]);
    [controller() initTest];
  }

  void TearDown() override {
    bridge_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  void CreateItems(NSMenu* menu) {
    auto item = std::make_unique<HistoryMenuBridge::HistoryItem>();
    item->url = GURL("http://google.com");
    item->session_id = SessionID::FromSerializedValue(1);
    bridge_->AddItemToMenu(std::move(item), menu, HistoryMenuBridge::kVisited,
                           0);

    item = std::make_unique<HistoryMenuBridge::HistoryItem>();
    item->url = GURL("http://apple.com");
    item->session_id = SessionID::FromSerializedValue(2);
    bridge_->AddItemToMenu(std::move(item), menu, HistoryMenuBridge::kVisited,
                           1);
  }

  std::map<NSMenuItem*, std::unique_ptr<HistoryMenuBridge::HistoryItem>>&
  menu_item_map() {
    return bridge_->menu_item_map_;
  }

  FakeHistoryMenuController* controller() {
    return static_cast<FakeHistoryMenuController*>(bridge_->controller_.get());
  }

 private:
  CocoaTestHelper cocoa_test_helper_;
  std::unique_ptr<HistoryMenuBridge> bridge_;
};

TEST_F(HistoryMenuCocoaControllerTest, OpenURLForItem) {
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"History"]);
  CreateItems(menu.get());

  std::map<NSMenuItem*, std::unique_ptr<HistoryMenuBridge::HistoryItem>>&
      items = menu_item_map();
  for (const auto& pair : items) {
    HistoryMenuBridge::HistoryItem* item = pair.second.get();
    EXPECT_FALSE(controller()->_opened[item->session_id.id()]);
    [controller() openHistoryMenuItem:pair.first];
    EXPECT_TRUE(controller()->_opened[item->session_id.id()]);
  }
}
