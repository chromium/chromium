// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/history_menu_cocoa_controller.h"

#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/cocoa/history_menu_bridge.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/mock_tab_restore_service.h"
#include "components/sessions/core/session_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"

@interface FakeHistoryMenuController : HistoryMenuCocoaController {
 @public
  // ivars are initialized to zero, so these all start out as NO.
  std::set<SessionID::id_type> _opened;
}
@end

@implementation FakeHistoryMenuController

- (void)openURLForItem:(const HistoryMenuBridge::HistoryItem*)item {
  _opened.insert(item->session_id.id());
}

@end  // FakeHistoryMenuController

class HistoryMenuCocoaControllerTest : public CocoaTest {
 public:
  HistoryMenuCocoaControllerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    CocoaTest::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Default");

    bridge_ = std::make_unique<HistoryMenuBridge>(profile_);
    bridge_->controller_ =
        [[FakeHistoryMenuController alloc] initWithBridge:bridge_.get()];
  }

  void TearDown() override {
    bridge_.reset();
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
    CocoaTest::TearDown();
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }
  TestingProfile* profile() { return profile_; }
  HistoryMenuBridge* bridge() { return bridge_.get(); }

  void SetController(HistoryMenuCocoaController* controller) {
    bridge_->controller_ = controller;
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
    return static_cast<FakeHistoryMenuController*>(bridge_->controller_);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<HistoryMenuBridge> bridge_;
};

TEST_F(HistoryMenuCocoaControllerTest, OpenURLForItem) {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"History"];
  CreateItems(menu);

  std::map<NSMenuItem*, std::unique_ptr<HistoryMenuBridge::HistoryItem>>&
      items = menu_item_map();
  for (const auto& pair : items) {
    HistoryMenuBridge::HistoryItem* item = pair.second.get();
    EXPECT_FALSE(controller()->_opened.count(item->session_id.id()));
    [controller() openHistoryMenuItem:pair.first];
    EXPECT_TRUE(controller()->_opened.count(item->session_id.id()));
  }
}

// Verifies that menu delegate callbacks (e.g., menuDidClose:) and item
// actions safely no-op after HistoryMenuBridge is destroyed
// (https://crbug.com/573702454).
TEST_F(HistoryMenuCocoaControllerTest, MenuCallbacksAfterBridgeDestroyed) {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"History"];
  FakeHistoryMenuController* ctrl = controller();
  [ctrl clearBridge];

  // These should safely no-op when the bridge pointer is cleared.
  [ctrl menuNeedsUpdate:menu];
  [ctrl menuWillOpen:menu];
  [ctrl menuDidClose:menu];
  NSMenuItem* item =
      [[NSMenuItem alloc] initWithTitle:@"Item"
                                 action:@selector(openHistoryMenuItem:)
                          keyEquivalent:@""];
  [ctrl openHistoryMenuItem:item];
}

TEST_F(HistoryMenuCocoaControllerTest, DoesNotOpenLockedLoadedProfile) {
  // Replace `FakeHistoryMenuController` (which overrides `-openURLForItem:`)
  // with a real `HistoryMenuCocoaController` so `RunInProfileSafely()` runs.
  HistoryMenuCocoaController* real_controller =
      [[HistoryMenuCocoaController alloc] initWithBridge:bridge()];
  SetController(real_controller);

  auto* mock_trs = static_cast<testing::NiceMock<MockTabRestoreService>*>(
      TabRestoreServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating([](content::BrowserContext*)
                                             -> std::unique_ptr<KeyedService> {
            return std::make_unique<testing::NiceMock<MockTabRestoreService>>();
          })));
  ASSERT_NE(nullptr, mock_trs);
  ON_CALL(*mock_trs, IsLoaded()).WillByDefault(testing::Return(true));

  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"History"];
  CreateItems(menu);
  ASSERT_FALSE(menu_item_map().empty());
  NSMenuItem* menu_item = menu_item_map().begin()->first;
  const SessionID expected_session_id =
      menu_item_map().begin()->second->session_id;

  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
  ProfileAttributesEntry* entry =
      profile_manager()
          ->profile_attributes_storage()
          ->GetProfileAttributesWithPath(profile()->GetPath());
  ASSERT_NE(nullptr, entry);
  entry->LockForceSigninProfile(true);

  EXPECT_CALL(*mock_trs, RestoreEntryById(testing::_, testing::_, testing::_))
      .Times(0);
  [real_controller openHistoryMenuItem:menu_item];
  testing::Mock::VerifyAndClearExpectations(mock_trs);

  // Unlocking the loaded profile allows the menu item to restore the session.
  entry->LockForceSigninProfile(false);
  EXPECT_CALL(
      *mock_trs,
      RestoreEntryById(
          nullptr, testing::Property(&SessionID::id, expected_session_id.id()),
          ui::mojom::WindowOpenDisposition::kUnknown))
      .Times(1);
  [real_controller openHistoryMenuItem:menu_item];
}
