// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_APPLESCRIPT_UTILS_TEST_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_APPLESCRIPT_UTILS_TEST_H_

#import <Cocoa/Cocoa.h>
#import <objc/objc-runtime.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_folder_applescript.h"
#include "chrome/test/base/in_process_browser_test.h"

// Used to emulate an active running script, useful for testing purposes.
@interface FakeScriptCommand : NSScriptCommand {
  Method _originalMethod;
  Method _alternateMethod;
}
@end


// The base class for all our bookmark releated unit tests.
class BookmarkAppleScriptTest : public InProcessBrowserTest {
 public:
  BookmarkAppleScriptTest();
  ~BookmarkAppleScriptTest() override;
  void SetUpOnMainThread() override;

  Profile* profile() const;

 protected:
  base::scoped_nsobject<BookmarkFolderAppleScript> bookmarkBar_;
};

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_APPLESCRIPT_UTILS_TEST_H_
