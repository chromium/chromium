// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_applescript_utils_test.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/tab_applescript.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using TabAppleScriptTest = InProcessBrowserTest;

namespace {

// Calls the method that handles the "Execute Javascript" command and returns
// the script error number.
int ExecuteJavascriptCommand(TabAppleScript* tab_applescript) {
  base::scoped_nsobject<FakeScriptCommand> fakeScriptCommand(
      [[FakeScriptCommand alloc] init]);
  [tab_applescript handlesExecuteJavascriptScriptCommand:nil];
  return [NSScriptCommand currentCommand].scriptErrorNumber;
}

IN_PROC_BROWSER_TEST_F(TabAppleScriptTest, Creation) {
  base::scoped_nsobject<TabAppleScript> tab_applescript(
      [[TabAppleScript alloc] initWithWebContents:nullptr]);
  EXPECT_FALSE(tab_applescript);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab_applescript.reset(
      [[TabAppleScript alloc] initWithWebContents:web_contents]);
  EXPECT_TRUE(tab_applescript);
}

IN_PROC_BROWSER_TEST_F(TabAppleScriptTest, ExecuteJavascript) {
  Profile* profile = browser()->profile();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::scoped_nsobject<TabAppleScript> tab_applescript(
      [[TabAppleScript alloc] initWithWebContents:web_contents]);

  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kAllowJavascriptAppleEvents, false);
  EXPECT_EQ(AppleScript::ErrorCode::errJavaScriptUnsupported,
            ExecuteJavascriptCommand(tab_applescript.get()));

  prefs->SetBoolean(prefs::kAllowJavascriptAppleEvents, true);
  EXPECT_EQ(0, ExecuteJavascriptCommand(tab_applescript.get()));
}

}  // namespace
