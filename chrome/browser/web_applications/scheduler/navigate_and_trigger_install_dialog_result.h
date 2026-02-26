// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_NAVIGATE_AND_TRIGGER_INSTALL_DIALOG_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_NAVIGATE_AND_TRIGGER_INSTALL_DIALOG_RESULT_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"

namespace web_app {

enum class NavigateAndTriggerInstallDialogResult {
  // The command failed, e.g. due to navigation error or the site not being
  // installable.
  kFailure,
  // The web app was already installed.
  kAlreadyInstalled,
  // The install dialog was successfully shown to the user.
  kDialogShown,
  // The system was shut down before the command could complete.
  kShutdown,
};

std::ostream& operator<<(std::ostream& os,
                         NavigateAndTriggerInstallDialogResult result);

// The navigation will always succeed. The `result` indicates whether the
// command was able to trigger the install dialog.
using NavigateAndTriggerInstallDialogCommandCallback =
    base::OnceCallback<void(NavigateAndTriggerInstallDialogResult result)>;
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_NAVIGATE_AND_TRIGGER_INSTALL_DIALOG_RESULT_H_
