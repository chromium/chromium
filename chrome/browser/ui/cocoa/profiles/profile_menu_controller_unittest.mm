// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/profile_menu_controller.h"

#include <stddef.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/cocoa/test/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/test/run_loop_testing.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"

class ProfileMenuControllerTest : public CocoaProfileTest {
 public:
  ProfileMenuControllerTest() { RebuildController(); }

  void SetUp() override {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(profile());

    // Spin the runloop so |-initializeMenu| gets called.
    chrome::testing::NSRunLoopRunAllPending();
  }

  void RebuildController() {
    item_.reset([[NSMenuItem alloc] initWithTitle:@"Users"
                                           action:nil
                                    keyEquivalent:@""]);
    controller_.reset(
        [[ProfileMenuController alloc] initWithMainMenuItem:item_]);
  }

  void TestBottomItems() {
    NSMenu* menu = [controller() menu];
    NSInteger count = [menu numberOfItems];

    ASSERT_GE(count, 4);

    NSMenuItem* item = [menu itemAtIndex:count - 4];
    EXPECT_TRUE([item isSeparatorItem]);

    item = [menu itemAtIndex:count - 3];
    EXPECT_EQ(@selector(editProfile:), [item action]);

    item = [menu itemAtIndex:count - 2];
    EXPECT_TRUE([item isSeparatorItem]);

    item = [menu itemAtIndex:count - 1];
    EXPECT_EQ(@selector(newProfile:), [item action]);
  }

  void VerifyProfileNamedIsActive(NSString* title, int line) {
    for (NSMenuItem* item in [[controller() menu] itemArray]) {
      if ([[item title] isEqualToString:title]) {
        EXPECT_EQ(NSOnState, [item state]) << [[item title] UTF8String]
          << " (from line " << line << ")";
      } else {
        EXPECT_EQ(NSOffState, [item state]) << [[item title] UTF8String]
          << " (from line " << line << ")";
      }
    }
  }

  ProfileMenuController* controller() { return controller_.get(); }

  NSMenuItem* menu_item() { return item_.get(); }

 private:
  base::scoped_nsobject<NSMenuItem> item_;
  base::scoped_nsobject<ProfileMenuController> controller_;
};

TEST_F(ProfileMenuControllerTest, InitializeMenu) {
  NSMenu* menu = [controller() menu];
  // Profile, <sep>, Edit, <sep>, New.
  ASSERT_EQ(5, [menu numberOfItems]);

  TestBottomItems();

  EXPECT_FALSE([menu_item() isHidden]);
}

TEST_F(ProfileMenuControllerTest, CreateItemWithTitle) {
  NSMenuItem* item =
      [controller() createItemWithTitle:@"Title"
                                 action:@selector(someSelector:)];
  EXPECT_NSEQ(@"Title", [item title]);
  EXPECT_EQ(controller(), [item target]);
  EXPECT_EQ(@selector(someSelector:), [item action]);
  EXPECT_NSEQ(@"", [item keyEquivalent]);
}

TEST_F(ProfileMenuControllerTest, RebuildMenu) {
  NSMenu* menu = [controller() menu];
  EXPECT_EQ(5, [menu numberOfItems]);

  EXPECT_FALSE([menu_item() isHidden]);

  // Create some more profiles on the manager.
  TestingProfileManager* manager = testing_profile_manager();
  manager->CreateTestingProfile("Profile 2");
  manager->CreateTestingProfile("Profile 3");

  // Verify that the menu got rebuilt.
  ASSERT_EQ(7, [menu numberOfItems]);

  NSMenuItem* item = [menu itemAtIndex:0];
  EXPECT_EQ(@selector(switchToProfileFromMenu:), [item action]);

  item = [menu itemAtIndex:1];
  EXPECT_EQ(@selector(switchToProfileFromMenu:), [item action]);

  item = [menu itemAtIndex:2];
  EXPECT_EQ(@selector(switchToProfileFromMenu:), [item action]);

  TestBottomItems();

  EXPECT_FALSE([menu_item() isHidden]);
}

TEST_F(ProfileMenuControllerTest, InsertItems) {
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  ASSERT_EQ(0, [menu numberOfItems]);

  // Even with one profile items can still be inserted.
  BOOL result = [controller() insertItemsIntoMenu:menu
                                         atOffset:0
                                         fromDock:NO];
  EXPECT_TRUE(result);
  EXPECT_EQ(1, [menu numberOfItems]);
  [menu removeAllItems];

  // Same for use in building the dock menu.
  result = [controller() insertItemsIntoMenu:menu
                                    atOffset:0
                                    fromDock:YES];
  EXPECT_FALSE(result);
  EXPECT_EQ(0, [menu numberOfItems]);
  [menu removeAllItems];

  // Create one more profile on the manager.
  TestingProfileManager* manager = testing_profile_manager();
  manager->CreateTestingProfile("Profile 2");

  // With more than one profile, insertItems should return YES.
  result = [controller() insertItemsIntoMenu:menu
                                    atOffset:0
                                    fromDock:NO];
  EXPECT_TRUE(result);
  ASSERT_EQ(2, [menu numberOfItems]);

  NSMenuItem* item = [menu itemAtIndex:0];
  EXPECT_EQ(@selector(switchToProfileFromMenu:), [item action]);

  item = [menu itemAtIndex:1];
  EXPECT_EQ(@selector(switchToProfileFromMenu:), [item action]);
  [menu removeAllItems];

  // And for the dock, the selector should be different and there should be a
  // header item.
  result = [controller() insertItemsIntoMenu:menu
                                    atOffset:0
                                    fromDock:YES];
  EXPECT_TRUE(result);
  ASSERT_EQ(3, [menu numberOfItems]);

  // First item is a label item.
  item = [menu itemAtIndex:0];
  EXPECT_FALSE([item isEnabled]);

  item = [menu itemAtIndex:1];
  EXPECT_EQ(@selector(switchToProfileFromDock:), [item action]);

  item = [menu itemAtIndex:2];
  EXPECT_EQ(@selector(switchToProfileFromDock:), [item action]);
}

TEST_F(ProfileMenuControllerTest, InitialActiveBrowser) {
  [controller() activeBrowserChangedTo:NULL];
  VerifyProfileNamedIsActive(l10n_util::GetNSString(IDS_DEFAULT_PROFILE_NAME),
                             __LINE__);
}

// Note: BrowserList::SetLastActive() is typically called as part of
// BrowserWindow::Show() and when a Browser becomes active. We don't need a full
// BrowserWindow, so it is called manually.
TEST_F(ProfileMenuControllerTest, SetActiveAndRemove) {
  NSMenu* menu = [controller() menu];
  TestingProfileManager* manager = testing_profile_manager();
  TestingProfile* profile2 = manager->CreateTestingProfile("Profile 2");
  TestingProfile* profile3 = manager->CreateTestingProfile("Profile 3");
  ASSERT_EQ(7, [menu numberOfItems]);

  // Create a browser and "show" it.
  Browser::CreateParams profile2_params(profile2, true);
  std::unique_ptr<Browser> p2_browser(
      CreateBrowserWithTestWindowForParams(&profile2_params));
  BrowserList::SetLastActive(p2_browser.get());
  VerifyProfileNamedIsActive(@"Profile 2", __LINE__);

  // Close the browser and make sure it's still active.
  p2_browser.reset();
  VerifyProfileNamedIsActive(@"Profile 2", __LINE__);

  // Open a new browser and make sure it takes effect.
  Browser::CreateParams profile3_params(profile3, true);
  std::unique_ptr<Browser> p3_browser(
      CreateBrowserWithTestWindowForParams(&profile3_params));
  BrowserList::SetLastActive(p3_browser.get());
  VerifyProfileNamedIsActive(@"Profile 3", __LINE__);

  p3_browser.reset();
  VerifyProfileNamedIsActive(@"Profile 3", __LINE__);
}

TEST_F(ProfileMenuControllerTest, DeleteActiveProfile) {
  TestingProfileManager* manager = testing_profile_manager();

  manager->CreateTestingProfile("Profile 2");
  TestingProfile* profile3 = manager->CreateTestingProfile("Profile 3");
  ASSERT_EQ(3U, manager->profile_manager()->GetNumberOfProfiles());

  const base::FilePath profile3_path = profile3->GetPath();
  manager->DeleteTestingProfile("Profile 3");

  // Simulate an unloaded profile by setting the "last used" local state pref
  // the profile that was just deleted.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         profile3_path.BaseName().MaybeAsASCII());

  // Simulate the active browser changing to NULL and ensure a profile doesn't
  // get created by disallowing IO operations temporarily.
  const bool io_was_allowed = base::ThreadRestrictions::SetIOAllowed(false);
  [controller() activeBrowserChangedTo:NULL];
  base::ThreadRestrictions::SetIOAllowed(io_was_allowed);
}

TEST_F(ProfileMenuControllerTest, AddProfileDisabled) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kBrowserAddPersonEnabled, false);

  RebuildController();
  // Spin the runloop so |-initializeMenu| gets called.
  chrome::testing::NSRunLoopRunAllPending();

  NSMenu* menu = [controller() menu];
  NSInteger count = [menu numberOfItems];

  ASSERT_GE(count, 2);

  NSMenuItem* item = [menu itemAtIndex:count - 2];
  EXPECT_TRUE([item isSeparatorItem]);

  item = [menu itemAtIndex:count - 1];
  EXPECT_EQ(@selector(editProfile:), [item action]);
}
