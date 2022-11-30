// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_log.h"

#include "base/numerics/safe_conversions.h"

namespace web_app {

LauncherLog::LauncherLog() : key_(GetLauncherLogRegistryKey(KEY_SET_VALUE)) {}

void LauncherLog::Log(WebAppLauncherLaunchResult value) {
  key_.WriteValue(kPwaLauncherResult, base::saturated_cast<DWORD>(value));
}

}  // namespace web_app
