// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_ISOLATED_WEB_APP_APPLY_UPDATE_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_ISOLATED_WEB_APP_APPLY_UPDATE_RESULT_H_

#include "base/types/expected.h"

namespace web_app {

// Represents an error during the application of a pending IWA update.
struct IsolatedWebAppApplyUpdateCommandError {
  std::string message;
};

std::ostream& operator<<(std::ostream& os,
                         const IsolatedWebAppApplyUpdateCommandError& error);

using IsolatedWebAppApplyUpdateCommandResult =
    base::expected<void, IsolatedWebAppApplyUpdateCommandError>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_ISOLATED_WEB_APP_APPLY_UPDATE_RESULT_H_
