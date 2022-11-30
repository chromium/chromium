// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_AUTO_LAUNCH_UTIL_H_
#define CHROME_INSTALLER_UTIL_AUTO_LAUNCH_UTIL_H_

// A namespace containing the platform specific implementation of setting Chrome
// to launch at user login.
namespace auto_launch_util {

// Requests that Chrome start in Background Mode at user login.
void EnableBackgroundStartAtLogin();

// Disables auto-starting Chrome in background mode at user login.
void DisableBackgroundStartAtLogin();

}  // namespace auto_launch_util

#endif  // CHROME_INSTALLER_UTIL_AUTO_LAUNCH_UTIL_H_
