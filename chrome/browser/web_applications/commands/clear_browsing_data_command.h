// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/values.h"

namespace web_app {

class AllAppsLock;

// Clears web app specific browsing data from the web app system for a given
// time range. This includes last launch times, last badging times, and seen
// manifest URLs. This is used as part of the general browsing data clearing
// mechanism in Chrome.
void ClearWebAppBrowsingData(const base::Time& begin_time,
                             const base::Time& end_time,
                             AllAppsLock& lock,
                             base::Value::Dict& debug_value);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_
