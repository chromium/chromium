// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/tab_applescript.h"

#import <Foundation/Foundation.h>

#include <utility>

#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/applescript/applescript_test_utils.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace AppleScript {

namespace {

class TabAppleScriptTest : public InProcessBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override { disabler_.SetUp(); }

  DevToolsDisabler disabler_;
};

// Calls the method that handles the "View Source" command and returns the
// script error number.
int ExecuteViewSourceCommand(TabAppleScript* tab_applescript) {
  FakeScriptCommand* fake_script_command = [[FakeScriptCommand alloc] init];
  [tab_applescript handlesViewSourceScriptCommand:nil];
  return fake_script_command.scriptErrorNumber;
}

// Calls the method that handles the "Execute Javascript" command and returns
// the script error number.
int ExecuteJavascriptCommand(TabAppleScript* tab_applescript) {
  FakeScriptCommand* fake_script_command = [[FakeScriptCommand alloc] init];
  [tab_applescript handlesExecuteJavascriptScriptCommand:nil];
  return fake_script_command.scriptErrorNumber;
}

IN_PROC_BROWSER_TEST_F(TabAppleScriptTest, Creation) {
  TabAppleScript* tab_applescript =
      [[TabAppleScript alloc] initWithWebContents:nullptr];
  EXPECT_FALSE(tab_applescript);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  tab_applescript = [[TabAppleScript alloc] initWithWebContents:web_contents];
  EXPECT_TRUE(tab_applescript);
}

IN_PROC_BROWSER_TEST_F(TabAppleScriptTest, ViewSource) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabAppleScript* tab_applescript =
      [[TabAppleScript alloc] initWithWebContents:web_contents];

  disabler_.DisableDevTools();
  EXPECT_EQ(std::to_underlying(Error::kDevToolsUnsupported),
            ExecuteViewSourceCommand(tab_applescript));
}

IN_PROC_BROWSER_TEST_F(TabAppleScriptTest, ExecuteJavascript) {
  Profile* profile = browser()->profile();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabAppleScript* tab_applescript =
      [[TabAppleScript alloc] initWithWebContents:web_contents];

  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kAllowJavascriptAppleEvents, false);
  EXPECT_EQ(std::to_underlying(Error::kJavaScriptUnsupported),
            ExecuteJavascriptCommand(tab_applescript));

  prefs->SetBoolean(prefs::kAllowJavascriptAppleEvents, true);
  EXPECT_EQ(0, ExecuteJavascriptCommand(tab_applescript));

  disabler_.DisableDevTools();
  EXPECT_EQ(std::to_underlying(Error::kDevToolsUnsupported),
            ExecuteJavascriptCommand(tab_applescript));
}

}  // namespace

}  // namespace AppleScript
