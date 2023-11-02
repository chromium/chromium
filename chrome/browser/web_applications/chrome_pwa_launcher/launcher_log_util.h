// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAUNCHER_LOG_UTIL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAUNCHER_LOG_UTIL_H_

#include "base/win/registry.h"

namespace web_app {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WebAppLauncherLaunchResult {
  kSuccess = 0,
  kStarted = 1,
  kError = 2,
  kMaxValue = kError
};

extern const wchar_t kPwaLauncherResult[];

base::win::RegKey GetLauncherLogRegistryKey(REGSAM access);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_CHROME_PWA_LAUNCHER_LAUNCHER_LOG_UTIL_H_
