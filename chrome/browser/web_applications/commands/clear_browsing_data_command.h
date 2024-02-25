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

// Clears the browsing data for web app, given the inclusive time range.
void ClearWebAppBrowsingData(const base::Time& begin_time,
                             const base::Time& end_time,
                             AllAppsLock& lock,
                             base::Value::Dict& debug_value);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_
