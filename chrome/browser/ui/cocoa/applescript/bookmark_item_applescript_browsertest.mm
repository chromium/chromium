// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_applescript_test_utils.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_item_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

using BookmarkItemAppleScriptTest = BookmarkAppleScriptTest;

namespace AppleScript {

namespace {

// Set and get title.
IN_PROC_BROWSER_TEST_F(BookmarkItemAppleScriptTest, GetAndSetTitle) {
  NSArray* bookmark_items = bookmark_bar_.bookmarkItems;
  BookmarkItemAppleScript* item1 = bookmark_items[0];
  item1.title = @"Foo";
  EXPECT_NSEQ(@"Foo", item1.title);
}

// Set and get URL.
IN_PROC_BROWSER_TEST_F(BookmarkItemAppleScriptTest, GetAndSetURL) {
  NSArray* bookmark_items = bookmark_bar_.bookmarkItems;
  BookmarkItemAppleScript* item1 = bookmark_items[0];
  item1.URL = @"http://foo-bar.org";
  EXPECT_EQ(GURL("http://foo-bar.org"),
            GURL(base::SysNSStringToUTF8(item1.URL)));

  // If scripter enters invalid URL.
  FakeScriptCommand* fake_script_command = [[FakeScriptCommand alloc] init];
  item1.URL = @"invalid-url.org";
  EXPECT_EQ(static_cast<int>(Error::kInvalidURL),
            fake_script_command.scriptErrorNumber);
}

// Creating bookmarks with javascript: URLs is controlled by a preference.
IN_PROC_BROWSER_TEST_F(BookmarkItemAppleScriptTest, GetAndSetJavascriptURL) {
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowJavascriptAppleEvents, false);

  NSArray* bookmark_items = bookmark_bar_.bookmarkItems;
  BookmarkItemAppleScript* item1 = bookmark_items[0];

  FakeScriptCommand* fake_script_command = [[FakeScriptCommand alloc] init];
  item1.URL = @"javascript:alert('hi');";
  EXPECT_EQ(static_cast<int>(Error::kJavaScriptUnsupported),
            fake_script_command.scriptErrorNumber);

  prefs->SetBoolean(prefs::kAllowJavascriptAppleEvents, true);
  item1.URL = @"javascript:alert('hi');";
  EXPECT_EQ(GURL("javascript:alert('hi');"),
            GURL(base::SysNSStringToUTF8(item1.URL)));
}

}  // namespace

}  // namespace AppleScript
