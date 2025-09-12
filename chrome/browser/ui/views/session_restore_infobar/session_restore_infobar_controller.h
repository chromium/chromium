// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_model.h"

class Profile;

namespace content {
class WebContents;
}

namespace session_restore_infobar {

// This class is responsible for determining when the  session restore infobar
// should be shown and handling user interactions within the infobar.
class SessionRestoreInfobarController {
 public:
  SessionRestoreInfobarController(Profile& profile,
                                  bool was_restarted,
                                  bool is_post_crash_launch);
  ~SessionRestoreInfobarController();

  SessionRestoreInfobarController(const SessionRestoreInfobarController&) =
      delete;
  SessionRestoreInfobarController& operator=(
      const SessionRestoreInfobarController&) = delete;

  // Creates or destroys the session restore infobar based on the current state.
  void CreateOrDestroySessionRestoreInfobar(content::WebContents& web_contents);

  // Gets the message type to be displayed for the infobar.
  SessionRestoreInfoBarDelegate::InfobarMessageType GetInfobarMessageType();

 private:
  const raw_ref<Profile> profile_;
  std::unique_ptr<SessionRestoreInfobarModel> model_;
};

}  // namespace session_restore_infobar

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_CONTROLLER_H_
