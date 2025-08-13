// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_CONTROLLER_H_

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_model.h"

class Profile;

namespace session_restore_infobar {

// This class is responsible for determining when the infobar should be shown
// and handling user interactions within the infobar.
class SessionRestoreInfobarController {
 public:
  SessionRestoreInfobarController();
  ~SessionRestoreInfobarController();

  SessionRestoreInfobarController(const SessionRestoreInfobarController&) =
      delete;
  SessionRestoreInfobarController& operator=(
      const SessionRestoreInfobarController&) = delete;

  // Performs checks to determine if the session restore infobar should be
  // displayed at startup.
  bool ShouldShowSessionRestoreInfobarOnStartup();

  // Check if the browser is actively restoring a previous session.
  // In such cases, the session restore infobar will not be displayed.
  bool CanShowInfobar(
      SessionRestoreInfobarModel::SessionRestoreMessageValue message_value,
      Profile* profile);

  // Creates or destroys the session restore infobar based on the checks made at
  // start up and current session restore preference.
  void CreateOrDestroySessionRestoreInfobar();

 private:
  Profile* GetProfile();
  SessionRestoreInfobarModel::SessionRestoreMessageValue GetModelValue();
  std::unique_ptr<SessionRestoreInfobarModel> model_;
};

}  // namespace session_restore_infobar

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_CONTROLLER_H_
