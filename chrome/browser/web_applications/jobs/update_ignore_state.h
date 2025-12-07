// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UPDATE_IGNORE_STATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UPDATE_IGNORE_STATE_H_

#include "base/values.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {
class AppLock;

// Stores state in the web app depicting that any pending updates surfaced to
// the user has been ignored, and notifies observers to make state changes if
// needed.
void SetWebAppPendingUpdateAsIgnored(const webapps::AppId& app_id,
                                     AppLock& lock,
                                     base::Value::Dict& debug_value);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UPDATE_IGNORE_STATE_H_
