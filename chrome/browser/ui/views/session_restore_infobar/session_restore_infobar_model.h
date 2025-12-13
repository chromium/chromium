// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_MODEL_H_

#include "base/memory/raw_ref.h"

class PrefService;
class Profile;

namespace content {
class WebContents;
}

namespace session_restore_infobar {

// `SessionRestoreInfobarModel` class is responsible for encapsulating the logic
// related to session restore functionality. It determines the appropriate
// message to display in the infobar based on the user's current settings.
class SessionRestoreInfobarModel {
 public:
  // Enum for session restore message values. Based on the message value, there
  // will be a different message displayed on the infobar.
  enum class SessionRestoreMessageValue {
    kOpenNewTabPage,
    kContinueWhereLeftOff,
    kOpenSpecificPages
  };

  explicit SessionRestoreInfobarModel(Profile& profile,
                                      bool is_post_crash_launch);
  ~SessionRestoreInfobarModel();

  SessionRestoreInfobarModel(const SessionRestoreInfobarModel&) = delete;
  SessionRestoreInfobarModel& operator=(const SessionRestoreInfobarModel&) =
      delete;

  // Retrieves a value indicating the user's preference for session restore
  // behavior. This value is used to determine if a session restore message
  // should be displayed.
  SessionRestoreMessageValue GetSessionRestoreMessageValue() const;

  // Checks if the infobar should be shown on startup.
  bool ShouldShowOnStartup() const;

  // Returns true if the session restore preference is currently using its
  // default value, and has not been set.
  bool IsDefaultSessionRestorePref() const;

  // Returns true if the session restore setting has changed since the infobar
  // was shown.
  bool HasSessionRestoreSettingChanged(const PrefService& prefs) const;

 private:
  const raw_ref<Profile> profile_;
  const bool is_post_crash_launch_;
  const int initial_restore_on_startup_value_;
};

}  // namespace session_restore_infobar

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_MODEL_H_
