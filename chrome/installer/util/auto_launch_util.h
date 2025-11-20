// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_AUTO_LAUNCH_UTIL_H_
#define CHROME_INSTALLER_UTIL_AUTO_LAUNCH_UTIL_H_

#include <string>

// A namespace containing the platform specific implementation of setting Chrome
// to launch at user login.
namespace auto_launch_util {

// Different launch modes that can be registered with the OS.
enum class StartupLaunchMode { kBackground };

std::wstring GetAutoLaunchKeyName();

// Requests that Chrome start in Background Mode at user login.
void EnableStartAtLogin(StartupLaunchMode startup_launch_mode);

// Disables auto-starting Chrome in background mode at user login.
void DisableStartAtLogin();

}  // namespace auto_launch_util

#endif  // CHROME_INSTALLER_UTIL_AUTO_LAUNCH_UTIL_H_
