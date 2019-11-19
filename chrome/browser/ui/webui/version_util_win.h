// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VERSION_UTIL_WIN_H_
#define CHROME_BROWSER_UI_WEBUI_VERSION_UTIL_WIN_H_

#include <string>

#include "base/strings/string16.h"

namespace version_utils {
namespace win {

// Return the marketing version of Windows OS, this may return an empty string
// if values returned by base::win::OSinfo are not defined.
std::string GetFullWindowsVersion();

// Return a formatted version of the update cohort string
// IDS_VERSION_UI_COHORT_NAME filled with the update cohort of this Chromium
// install.
base::string16 GetCohortVersionInfo();

}  // namespace win
}  // namespace version_utils

#endif  // CHROME_BROWSER_UI_WEBUI_VERSION_UTIL_WIN_H_
