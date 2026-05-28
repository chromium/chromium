// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/test/android/side_panel_android_browser_test_base.h"

#include "base/android/device_info.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/side_panel/android/android_side_panel_enabled_fn.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

// static:
BrowserWindowInterface*
SidePanelAndroidBrowserTestBase::GetLastActiveBrowser() {
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
  CHECK(browser);
  return browser;
}

// static:
tabs::TabInterface*
SidePanelAndroidBrowserTestBase::GetActiveTabInLastActiveBrowser() {
  auto* browser = GetLastActiveBrowser();
  auto* tab_list = TabListInterface::From(browser);
  CHECK(tab_list) << "The browser window has no TabListInterface.";

  auto* tab = tab_list->GetActiveTab();
  CHECK(tab) << "No active tab.";
  return tab;
}

SidePanelAndroidBrowserTestBase::SidePanelAndroidBrowserTestBase() {
  // Note:
  //
  // Java code reads the cached `kEnableAndroidSidePanel` flag because we
  // use this flag to decide whether to inflate the main layout that contains
  // the side panel container, which happens _before_ the native library is
  // loaded.
  //
  // As of Mar 25, 2026, there was no way to override a cached flag in native
  // browser tests, so http://crrev.com/c/7689838 made the default value of
  // the cached flag `true` in tests.
  //
  // However, we still need to explicitly enable the flag here:
  //
  // On a newly installed ChromeBrowserTests APK, the `SharedPreferences`
  // backing the cached flag is empty so http://crrev.com/c/7689838 makes the
  // test pass the 1st run.
  //
  // After the 1st run, the `SharedPreferences` will contain the key for the
  // cached flag, but the default value of the cached flag won't be persisted.
  // If we don't explicitly enable the flag here, the cached flag value will
  // be `false` on subsequent runs and the tests will fail.
  feature_list_.InitAndEnableFeature(chrome::android::kEnableAndroidSidePanel);
}

SidePanelAndroidBrowserTestBase::~SidePanelAndroidBrowserTestBase() = default;

void SidePanelAndroidBrowserTestBase::SetUp() {
  if (!base::android::device_info::is_desktop() &&
      !base::android::device_info::is_tablet()) {
    GTEST_SKIP() << "Side panel is for large form factors; skipping the test "
                    "on others.";
  }

  // Despite the flag setup in the constructor, not all bots can see the
  // flag's "default value in tests". For example, bots with
  // "is_chrome_branded=true" don't read the "default value in tests". For
  // more details, please see `CachedFlag.java`.
  //
  // Here we'll do a final check of the flag and skip all tests if the flag
  // isn't enabled in tests.
  if (!AndroidSidePanelEnabledFn::IsEnabled()) {
    GTEST_SKIP() << "Android Side Panel is disabled";
  }

  BrowserWindowAndroidBrowserTestBase::SetUp();
}
