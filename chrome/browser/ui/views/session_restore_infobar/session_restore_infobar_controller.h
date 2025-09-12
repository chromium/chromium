// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_CONTROLLER_H_

#include <memory>

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"

class Profile;
namespace session_restore_infobar {

class SessionRestoreInfobarModel;

// This class is responsible for determining when the  session restore infobar
// should be shown and handling user interactions within the infobar.
class SessionRestoreInfobarController {
 public:
  SessionRestoreInfobarController();
  ~SessionRestoreInfobarController();

  SessionRestoreInfobarController(const SessionRestoreInfobarController&) =
      delete;
  SessionRestoreInfobarController& operator=(
      const SessionRestoreInfobarController&) = delete;

  // Checks if a session restore prompt should be shown for the given profile
  // and browser state. If so, it begins tracking tabs to show the infobar.
  void MaybeShowInfoBar(Profile& profile,
                        bool was_restarted,
                        bool is_post_crash_launch);

 private:
  SessionRestoreInfoBarDelegate::InfobarMessageType GetInfobarMessageType();
  std::unique_ptr<SessionRestoreInfobarModel> model_;
};

}  // namespace session_restore_infobar

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_CONTROLLER_H_
