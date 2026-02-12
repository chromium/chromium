// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/test/android/browser_window_android_browsertest_base.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/feed/feed_feature_list.h"

BrowserWindowAndroidBrowserTestBase::BrowserWindowAndroidBrowserTestBase() {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {// Enable incognito windows on Android.
       feed::kAndroidOpenIncognitoAsWindow},
      /*disabled_features=*/{});
}

BrowserWindowAndroidBrowserTestBase::~BrowserWindowAndroidBrowserTestBase() =
    default;

void BrowserWindowAndroidBrowserTestBase::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  AndroidBrowserTest::SetUpDefaultCommandLine(command_line);

  // Disable the first-run experience (FRE) so that when a function under
  // test launches an Intent for ChromeTabbedActivity, ChromeTabbedActivity
  // will be shown instead of FirstRunActivity.
  command_line->AppendSwitch("disable-fre");
}
