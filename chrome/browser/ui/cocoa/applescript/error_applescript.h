// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_ERROR_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_ERROR_APPLESCRIPT_H_

#import <Cocoa/Cocoa.h>

namespace AppleScript {

enum ErrorCode {
  // Error when default profile cannot be obtained.
  errGetProfile = 1,
  // Error when bookmark model fails to load.
  errBookmarkModelLoad,
  // Error when bookmark folder cannot be created.
  errCreateBookmarkFolder,
  // Error when bookmark item cannot be created.
  errCreateBookmarkItem,
  // Error when URL entered is invalid.
  errInvalidURL,
  // Error when printing cannot be initiated.
  errInitiatePrinting,
  // Error when invalid tab save type is entered.
  errInvalidSaveType,
  // Error when invalid browser mode is entered.
  errInvalidMode,
  // Error when tab index is out of bounds.
  errInvalidTabIndex,
  // Error when mode is set after browser window is created.
  errSetMode,
  // Error when index of browser window is out of bounds.
  errWrongIndex,
  // Error when JavaScript execution is disabled.
  errJavaScriptUnsupported
};

// This function sets an error message to the currently executing command.
void SetError(ErrorCode errorCode);
}

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_ERROR_APPLESCRIPT_H_
