// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_applescript_utils_test.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_item_applescript.h"
#import "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

using BookmarkItemAppleScriptTest = BookmarkAppleScriptTest;

namespace {

// Set and get title.
IN_PROC_BROWSER_TEST_F(BookmarkItemAppleScriptTest, GetAndSetTitle) {
  NSArray* bookmarkItems = [bookmarkBar_.get() bookmarkItems];
  BookmarkItemAppleScript* item1 = [bookmarkItems objectAtIndex:0];
  [item1 setTitle:@"Foo"];
  EXPECT_NSEQ(@"Foo", [item1 title]);
}

// Set and get URL.
IN_PROC_BROWSER_TEST_F(BookmarkItemAppleScriptTest, GetAndSetURL) {
  NSArray* bookmarkItems = [bookmarkBar_.get() bookmarkItems];
  BookmarkItemAppleScript* item1 = [bookmarkItems objectAtIndex:0];
  [item1 setURL:@"http://foo-bar.org"];
  EXPECT_EQ(GURL("http://foo-bar.org"),
            GURL(base::SysNSStringToUTF8([item1 URL])));

  // If scripter enters invalid URL.
  base::scoped_nsobject<FakeScriptCommand> fakeScriptCommand(
      [[FakeScriptCommand alloc] init]);
  [item1 setURL:@"invalid-url.org"];
  EXPECT_EQ((int)AppleScript::errInvalidURL,
            [fakeScriptCommand.get() scriptErrorNumber]);
}

// Creating bookmarks with javascript: URLs is controlled by a preference.
IN_PROC_BROWSER_TEST_F(BookmarkItemAppleScriptTest, GetAndSetJavascriptURL) {
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowJavascriptAppleEvents, false);

  NSArray* bookmarkItems = [bookmarkBar_.get() bookmarkItems];
  BookmarkItemAppleScript* item1 = [bookmarkItems objectAtIndex:0];

  base::scoped_nsobject<FakeScriptCommand> fakeScriptCommand(
      [[FakeScriptCommand alloc] init]);
  [item1 setURL:@"javascript:alert('hi');"];
  EXPECT_EQ(AppleScript::ErrorCode::errJavaScriptUnsupported,
            [fakeScriptCommand.get() scriptErrorNumber]);

  prefs->SetBoolean(prefs::kAllowJavascriptAppleEvents, true);
  [item1 setURL:@"javascript:alert('hi');"];
  EXPECT_EQ(GURL("javascript:alert('hi');"),
            GURL(base::SysNSStringToUTF8([item1 URL])));
}

}  // namespace
