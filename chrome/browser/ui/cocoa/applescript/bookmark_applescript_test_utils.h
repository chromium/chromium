// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_APPLESCRIPT_TEST_UTILS_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_APPLESCRIPT_TEST_UTILS_H_

#import <Foundation/Foundation.h>

#import "chrome/browser/ui/cocoa/applescript/bookmark_folder_applescript.h"
#include "chrome/test/base/in_process_browser_test.h"

// Used to emulate an active running script, useful for testing purposes.
@interface FakeScriptCommand : NSScriptCommand
@end

// The base class for all our bookmark related unit tests.
class BookmarkAppleScriptTest : public InProcessBrowserTest {
 public:
  BookmarkAppleScriptTest();
  ~BookmarkAppleScriptTest() override;
  void SetUpOnMainThread() override;

  Profile* profile() const;

 protected:
  BookmarkFolderAppleScript* __strong bookmark_bar_;
};

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_APPLESCRIPT_TEST_UTILS_H_
