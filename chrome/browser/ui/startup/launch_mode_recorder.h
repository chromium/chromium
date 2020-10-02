// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_LAUNCH_MODE_RECORDER_H_
#define CHROME_BROWSER_UI_STARTUP_LAUNCH_MODE_RECORDER_H_

#include "base/optional.h"

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered,
//   and (b) new constants should only be appended at the end of the
//   enumeration.
enum class LaunchMode {
  kToBeDecided = 0,  // Possibly direct launch or via a shortcut.
  // Launched as an installed web application in a standalone window.
  kAsWebAppInWindow = 1,
  kWithUrls = 2,             // Launched with urls in the cmd line.
  kOther = 3,                // Not launched from a shortcut.
  kShortcutNoName = 4,       // Launched from shortcut but no name available.
  kShortcutUnknown = 5,      // Launched from user-defined shortcut.
  kShortcutQuickLaunch = 6,  // Launched from the quick launch bar.
  kShortcutDesktop = 7,      // Launched from a desktop shortcut.
  kShortcutTaskbar = 8,      // Launched from the taskbar.
  kUserExperiment = 9,       // Launched after acceptance of a user experiment.
  kOtherOS = 10,             // Result bucket for OSes with no coverage here.
  kMacUndockedDiskLaunch = 11,  // Undocked launch from disk.
  kMacDockedDiskLaunch = 12,    // Docked launch from disk.
  kMacUndockedDMGLaunch = 13,   // Undocked launch from a dmg.
  kMacDockedDMGLaunch = 14,     // Docked launch from a dmg.
  kMacDockStatusError = 15,     // Error determining dock status.
  kMacDMGStatusError = 16,      // Error determining dmg status.
  kMacDockDMGStatusError = 17,  // Error determining dock and dmg status.
  // Launched from toast notification activation on Windows.
  kWinPlatformNotification = 18,
  kShortcutStartMenu = 19,  // A Windows Start Menu shortcut.
  // Started as a logon stub for the Google Credential Provider for Windows.
  kCredentialProviderSignIn = 20,
  // Launched as an installed web application in a browser tab.
  kAsWebAppInTab = 21,
  kUnknownWebApp = 22,  // The requested web application was not installed.
  kMaxValue = kUnknownWebApp,
};

class LaunchModeRecorder {
 public:
  LaunchModeRecorder();
  LaunchModeRecorder(const LaunchModeRecorder&) = delete;
  LaunchModeRecorder& operator=(const LaunchModeRecorder&) = delete;
  ~LaunchModeRecorder();

  // Only sets |mode_| if it has not already been set.
  void SetLaunchMode(LaunchMode mode);

 private:
  base::Optional<LaunchMode> mode_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_LAUNCH_MODE_RECORDER_H_
