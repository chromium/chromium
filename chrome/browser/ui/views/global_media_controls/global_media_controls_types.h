// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_GLOBAL_MEDIA_CONTROLS_TYPES_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_GLOBAL_MEDIA_CONTROLS_TYPES_H_

// The entry point through which the dialog was opened.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GlobalMediaControlsEntryPoint {
  // Through the Global Media Controls toolbar icon.
  kToolbarIcon = 0,
  // Through the use of the Presentation API (Cast) by a web page.
  kPresentation = 1,
  // Through the ChromeOS System Tray
  kSystemTray = 2,
  kMaxValue = kSystemTray,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GlobalMediaControlsCastActionAndEntryPoint {
  kStartViaToolbarIcon = 0,
  kStopViaToolbarIcon = 1,
  kStartViaPresentation = 2,
  kStopViaPresentation = 3,
  kStartViaSystemTray = 4,
  kStopViaSystemTray = 5,
  kMaxValue = kStopViaSystemTray,
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_GLOBAL_MEDIA_CONTROLS_TYPES_H_
