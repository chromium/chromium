// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_log_reporter.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/launcher_log_util.h"

namespace web_app {

void RecordPwaLauncherResult() {
  base::win::RegKey launcher_log_key(
      GetLauncherLogRegistryKey(KEY_READ | KEY_SET_VALUE));
  if (launcher_log_key.HasValue(kPwaLauncherResult)) {
    // Record the result of the last launch attempt by a PWA launcher.
    DWORD result;
    if (launcher_log_key.ReadValueDW(kPwaLauncherResult, &result) ==
            ERROR_SUCCESS &&
        result >= 0 &&
        result <= static_cast<DWORD>(WebAppLauncherLaunchResult::kMaxValue)) {
      base::UmaHistogramEnumeration(
          "WebApp.Launcher.LaunchResult",
          static_cast<WebAppLauncherLaunchResult>(result));
    }

    // Remove the just-recorded launch result from the registry to prevent the
    // same result being recorded more than once. The next launch attempt by a
    // PWA launcher will recreate the value.
    launcher_log_key.DeleteValue(kPwaLauncherResult);
  }
}

}  // namespace web_app
