// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_ANDROID_ANDROID_SIDE_PANEL_ENABLED_FN_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_ANDROID_ANDROID_SIDE_PANEL_ENABLED_FN_H_

// The C++ counter part of the Java `AndroidSidePanelEnabledFn`.
class AndroidSidePanelEnabledFn {
 public:
  AndroidSidePanelEnabledFn() = delete;

  // Returns true if the Android Side Panel should be enabled.
  static bool IsEnabled();
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_ANDROID_ANDROID_SIDE_PANEL_ENABLED_FN_H_
