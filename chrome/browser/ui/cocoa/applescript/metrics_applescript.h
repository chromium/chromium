// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_METRICS_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_METRICS_APPLESCRIPT_H_

namespace AppleScript {

// The AppleScript verb commands that are being recorded in a histogram. These
// values should not be re-ordered or removed.
enum AppleScriptCommand {
  TAB_CLOSE = 0,
  TAB_COPY,
  TAB_CUT,
  TAB_EXECUTE_JAVASCRIPT,
  TAB_GO_BACK,
  TAB_GO_FORWARD,
  TAB_PASTE,
  TAB_PRINT,
  TAB_REDO,
  TAB_RELOAD,
  TAB_SAVE,
  TAB_SELECT_ALL,
  TAB_STOP,
  TAB_UNDO,
  TAB_VIEW_SOURCE,
  WINDOW_CLOSE,
  WINDOW_ENTER_PRESENTATION_MODE,
  WINDOW_EXIT_PRESENTATION_MODE,
  APPLESCRIPT_COMMAND_EVENTS_COUNT
};

// Logs the sample's UMA metrics into the AppleScript.CommandEvent histogram
void LogAppleScriptUMA(AppleScriptCommand sample);

}  // namespace

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_METRICS_APPLESCRIPT_H_
