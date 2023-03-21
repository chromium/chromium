// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_ERROR_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_ERROR_APPLESCRIPT_H_

namespace AppleScript {

enum class Error {
  // Error when default profile cannot be obtained.
  kGetProfile = 1,
  // Error when bookmark model fails to load.
  kBookmarkModelLoad,
  // Error when bookmark folder cannot be created.
  kCreateBookmarkFolder,
  // Error when bookmark item cannot be created.
  kCreateBookmarkItem,
  // Error when URL entered is invalid.
  kInvalidURL,
  // Error when printing cannot be initiated.
  kInitiatePrinting,
  // Error when invalid tab save type is entered.
  kInvalidSaveType,
  // Error when invalid browser mode is entered.
  kInvalidMode,
  // Error when tab index is out of bounds.
  kInvalidTabIndex,
  // Error when mode is set after browser window is created.
  kSetMode,
  // Error when index of browser window is out of bounds.
  kWrongIndex,
  // Error when JavaScript execution is disabled.
  kJavaScriptUnsupported
};

// This function sets an error message to the currently executing command.
void SetError(Error error_code);
}

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_ERROR_APPLESCRIPT_H_
