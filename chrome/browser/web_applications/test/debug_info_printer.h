// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_DEBUG_INFO_PRINTER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_DEBUG_INFO_PRINTER_H_

#include <vector>

#include "base/time/time.h"

class Profile;

namespace web_app::test {

void LogDebugInfoToConsole(const std::vector<Profile*>& profiles,
                           base::TimeDelta time_ago_for_system_log_capture);

}  // namespace web_app::test

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_DEBUG_INFO_PRINTER_H_
