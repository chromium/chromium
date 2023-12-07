// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/window_size_autosaver.h"

#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

constexpr char kPath[] = "WindowSizeAutosaverTest";

class WindowSizeAutosaverTest : public BrowserWithTestWindowTest {
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    window_ = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(100, 101, 150, 151)
                  styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskResizable
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window_.releasedWhenClosed = NO;
    static_cast<user_prefs::PrefRegistrySyncable*>(
        profile()->GetPrefs()->DeprecatedGetPrefRegistry())
        ->RegisterDictionaryPref(kPath);
  }

  void TearDown() override {
    [window_ close];
    BrowserWithTestWindowTest::TearDown();
  }

 public:
  CocoaTestHelper cocoa_test_helper_;
  NSWindow* __strong window_;
};

TEST_F(WindowSizeAutosaverTest, RestoresAndSavesPos) {
  PrefService* pref = profile()->GetPrefs();
  ASSERT_TRUE(pref);

  // Check to make sure there is no existing pref for window placement.
  const base::Value::Dict& placement = pref->GetDict(kPath);
  EXPECT_TRUE(placement.empty());

  // Replace the window with one that doesn't have resize controls.
  [window_ close];
  window_ = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 101, 150, 151)
                                        styleMask:NSWindowStyleMaskTitled
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
  window_.releasedWhenClosed = NO;

  // Ask the window to save its position, then check that a preference
  // exists.  We're technically passing in a pointer to the user prefs
  // and not the local state prefs, but a PrefService* is a
  // PrefService*, and this is a unittest.

  {
    NSRect frame = [window_ frame];
    // Empty state, shouldn't restore:
    [[maybe_unused]] WindowSizeAutosaver* sizeSaver =
        [[WindowSizeAutosaver alloc] initWithWindow:window_
                                        prefService:pref
                                               path:kPath];
    EXPECT_EQ(NSMinX(frame), NSMinX([window_ frame]));
    EXPECT_EQ(NSMinY(frame), NSMinY([window_ frame]));
    EXPECT_EQ(NSWidth(frame), NSWidth([window_ frame]));
    EXPECT_EQ(NSHeight(frame), NSHeight([window_ frame]));

    // Move and resize window, should store position but not size.
    [window_ setFrame:NSMakeRect(300, 310, 250, 252) display:NO];
  }

  // Another window movement -- shouldn't be recorded.
  [window_ setFrame:NSMakeRect(400, 420, 160, 162) display:NO];

  {
    // Should restore last stored position, but not size.
    [[maybe_unused]] WindowSizeAutosaver* sizeSaver =
        [[WindowSizeAutosaver alloc] initWithWindow:window_
                                        prefService:pref
                                               path:kPath];
    EXPECT_EQ(300, NSMinX([window_ frame]));
    EXPECT_EQ(310, NSMinY([window_ frame]));
    EXPECT_EQ(160, NSWidth([window_ frame]));
    EXPECT_EQ(162, NSHeight([window_ frame]));
  }

  // ...and it should be in the profile, too.
  const base::Value::Dict& windowPref = pref->GetDict(kPath);
  EXPECT_FALSE(windowPref.FindInt("left").has_value());
  EXPECT_FALSE(windowPref.FindInt("right").has_value());
  EXPECT_FALSE(windowPref.FindInt("top").has_value());
  EXPECT_FALSE(windowPref.FindInt("bottom").has_value());
  std::optional<int> x = windowPref.FindInt("x");
  std::optional<int> y = windowPref.FindInt("y");
  ASSERT_TRUE(x.has_value());
  ASSERT_TRUE(y.has_value());
  EXPECT_EQ(300, x.value());
  EXPECT_EQ(310, y.value());
}

TEST_F(WindowSizeAutosaverTest, RestoresAndSavesRect) {
  PrefService* pref = profile()->GetPrefs();
  ASSERT_TRUE(pref);

  // Check to make sure there is no existing pref for window placement.
  const base::Value::Dict& placement = pref->GetDict(kPath);
  EXPECT_TRUE(placement.empty());

  // Ask the window to save its position, then check that a preference
  // exists.  We're technically passing in a pointer to the user prefs
  // and not the local state prefs, but a PrefService* is a
  // PrefService*, and this is a unittest.

  {
    NSRect frame = [window_ frame];
    // Empty state, shouldn't restore:
    [[maybe_unused]] WindowSizeAutosaver* sizeSaver =
        [[WindowSizeAutosaver alloc] initWithWindow:window_
                                        prefService:pref
                                               path:kPath];
    EXPECT_EQ(NSMinX(frame), NSMinX([window_ frame]));
    EXPECT_EQ(NSMinY(frame), NSMinY([window_ frame]));
    EXPECT_EQ(NSWidth(frame), NSWidth([window_ frame]));
    EXPECT_EQ(NSHeight(frame), NSHeight([window_ frame]));

    // Move and resize window, should store
    [window_ setFrame:NSMakeRect(300, 310, 250, 252) display:NO];
  }

  // Another window movement -- shouldn't be recorded.
  [window_ setFrame:NSMakeRect(400, 420, 160, 162) display:NO];

  {
    // Should restore last stored size
    [[maybe_unused]] WindowSizeAutosaver* sizeSaver =
        [[WindowSizeAutosaver alloc] initWithWindow:window_
                                        prefService:pref
                                               path:kPath];
    EXPECT_EQ(300, NSMinX([window_ frame]));
    EXPECT_EQ(310, NSMinY([window_ frame]));
    EXPECT_EQ(250, NSWidth([window_ frame]));
    EXPECT_EQ(252, NSHeight([window_ frame]));
  }

  // ...and it should be in the profile, too.
  const base::Value::Dict& windowPref = pref->GetDict(kPath);
  EXPECT_FALSE(windowPref.FindInt("x").has_value());
  EXPECT_FALSE(windowPref.FindInt("y").has_value());
  std::optional<int> x1 = windowPref.FindInt("left");
  std::optional<int> x2 = windowPref.FindInt("right");
  std::optional<int> y1 = windowPref.FindInt("top");
  std::optional<int> y2 = windowPref.FindInt("bottom");
  ASSERT_TRUE(x1.has_value());
  ASSERT_TRUE(x2.has_value());
  ASSERT_TRUE(y1.has_value());
  ASSERT_TRUE(y2.has_value());
  EXPECT_EQ(300, x1.value());
  EXPECT_EQ(310, y1.value());
  EXPECT_EQ(300 + 250, x2.value());
  EXPECT_EQ(310 + 252, y2.value());
}

// http://crbug.com/39625
TEST_F(WindowSizeAutosaverTest, DoesNotRestoreButClearsEmptyRect) {
  PrefService* pref = profile()->GetPrefs();
  ASSERT_TRUE(pref);

  ScopedDictPrefUpdate update(pref, kPath);
  base::Value::Dict& windowPref = update.Get();
  windowPref.Set("left", 50);
  windowPref.Set("right", 50);
  windowPref.Set("top", 60);
  windowPref.Set("bottom", 60);

  {
    // Window rect shouldn't change...
    NSRect frame = [window_ frame];
    [[maybe_unused]] WindowSizeAutosaver* sizeSaver =
        [[WindowSizeAutosaver alloc] initWithWindow:window_
                                        prefService:pref
                                               path:kPath];
    EXPECT_EQ(NSMinX(frame), NSMinX([window_ frame]));
    EXPECT_EQ(NSMinY(frame), NSMinY([window_ frame]));
    EXPECT_EQ(NSWidth(frame), NSWidth([window_ frame]));
    EXPECT_EQ(NSHeight(frame), NSHeight([window_ frame]));
  }

  // ...and it should be gone from the profile, too.
  EXPECT_FALSE(windowPref.FindInt("x").has_value());
  EXPECT_FALSE(windowPref.FindInt("y").has_value());
  ASSERT_FALSE(windowPref.FindInt("left").has_value());
  ASSERT_FALSE(windowPref.FindInt("right").has_value());
  ASSERT_FALSE(windowPref.FindInt("top").has_value());
  ASSERT_FALSE(windowPref.FindInt("bottom").has_value());
}

}  // namespace
