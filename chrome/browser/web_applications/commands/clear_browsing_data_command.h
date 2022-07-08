// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_

#include "base/time/time.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

// Clears the browsing data for web app, given the inclusive time range.
void ClearWebAppBrowsingData(base::Time begin_time,
                             base::Time end_time,
                             WebAppProvider* provider,
                             base::OnceClosure done);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CLEAR_BROWSING_DATA_COMMAND_H_
