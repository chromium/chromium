// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
BrowserWindowInterface* CreateBrowserWindowSync(
    BrowserWindowInterface::Type type,
    Profile* profile) {
  BrowserWindowCreateParams create_params =
      BrowserWindowCreateParams(type, *profile, /*from_user_gesture=*/false);

  return CreateBrowserWindow(std::move(create_params));
}

BrowserWindowInterface* CreateBrowserWindowAsync(
    BrowserWindowInterface::Type type,
    Profile* profile) {
  BrowserWindowCreateParams create_params =
      BrowserWindowCreateParams(type, *profile, /*from_user_gesture=*/false);

  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(create_params), future.GetCallback());
  return future.Get();
}

void AssertBrowserWindow(BrowserWindowInterface* browser_window,
                         BrowserWindowInterface::Type expected_type,
                         Profile* expected_profile,
                         bool expect_fully_initialized = false) {
  ASSERT_NE(browser_window, nullptr);
  EXPECT_EQ(browser_window->GetType(), expected_type);
  EXPECT_EQ(browser_window->GetProfile(), expected_profile);

  if (expect_fully_initialized) {
    auto* tab_list_interface = TabListInterface::From(browser_window);
    ASSERT_NE(tab_list_interface, nullptr);
    EXPECT_EQ(tab_list_interface->GetTabCount(), 1);
    EXPECT_EQ(tab_list_interface->GetActiveIndex(), 0);
    ASSERT_NE(tab_list_interface->GetActiveTab(), nullptr);
  }
}
}  // namespace

class CreateBrowserWindowAndroidBrowserTest : public AndroidBrowserTest {
 public:
  CreateBrowserWindowAndroidBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {// Disable ChromeTabbedActivity instance limit so that the total number
         // of
         // windows created by the entire test suite won't be limited.
         //
         // See MultiWindowUtils#getMaxInstances() for the reason:
         // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/multiwindow/MultiWindowUtils.java;l=209;drc=0bcba72c5246a910240b311def40233f7d3f15af
         chrome::android::kDisableInstanceLimit,

         // Enable incognito windows on Android.
         feed::kAndroidOpenIncognitoAsWindow},
        /*disabled_features=*/{});
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    AndroidBrowserTest::SetUpDefaultCommandLine(command_line);

    // Disable the first-run experience (FRE) so that when a function under
    // test launches an Intent for ChromeTabbedActivity, ChromeTabbedActivity
    // will be shown instead of FirstRunActivity.
    command_line->AppendSwitch("disable-fre");

    // Force DeviceInfo#isDesktop() to be true so that the kDisableInstanceLimit
    // flag in the constructor can be effective when running tests on an
    // emulator without "--force-desktop-android".
    //
    // See MultiWindowUtils#getMaxInstances() for the reason:
    // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/multiwindow/MultiWindowUtils.java;l=213;drc=0bcba72c5246a910240b311def40233f7d3f15af
    command_line->AppendSwitch(switches::kForceDesktopAndroid);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowSync_RegularProfile_ReturnsRegularBrowserWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* profile = GetProfile();

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowSync(type, profile);

  AssertBrowserWindow(new_browser_window, type, profile);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowSync_IncognitoProfile_ReturnsIncognitoBrowserWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowSync(type, incognito_profile);

  AssertBrowserWindow(new_browser_window, type, incognito_profile);
}

IN_PROC_BROWSER_TEST_F(CreateBrowserWindowAndroidBrowserTest,
                       CreateBrowserWindowSync_Popup_ReturnsPopupWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_POPUP;
  Profile* profile = GetProfile();

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowSync(type, profile);

  AssertBrowserWindow(new_browser_window, type, profile);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowSync_UnsupportedWindowType_ReturnsNull) {
  auto type =
      BrowserWindowInterface::Type::TYPE_APP;  // not supported on Android
  Profile* profile = GetProfile();

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowSync(type, profile);

  EXPECT_EQ(new_browser_window, nullptr);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_RegularProfile_TriggersCallbackWithRegularBrowserWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* profile = GetProfile();

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowAsync(type, profile);

  AssertBrowserWindow(new_browser_window, type, profile,
                      /*expect_fully_initialized=*/true);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_IncognitoProfile_TriggersCallbackWithIncognitoBrowserWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowAsync(type, incognito_profile);

  AssertBrowserWindow(new_browser_window, type, incognito_profile,
                      /*expect_fully_initialized=*/true);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_Popup_TriggersCallbackWithPopupWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_POPUP;
  Profile* profile = GetProfile();

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowAsync(type, profile);

  AssertBrowserWindow(new_browser_window, type, profile,
                      /*expect_fully_initialized=*/true);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_UnsupportedWindowType_TriggersCallbackWithNull) {
  auto type =
      BrowserWindowInterface::Type::TYPE_APP;  // not supported on Android
  Profile* profile = GetProfile();

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowAsync(type, profile);

  EXPECT_EQ(new_browser_window, nullptr);
}
