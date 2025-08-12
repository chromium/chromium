// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_MODEL_H_

#include "base/memory/raw_ref.h"
class PrefService;

namespace session_restore_infobar {

// `SessionRestoreInfobarModel` class is responsible for encapsulating the logic
// related to session restore functionality. It determines the appropriate
// message to display in the infobar based on the user's current settings.
class SessionRestoreInfobarModel {
 public:
  // Enum for session restore message values. Based on the message value, there
  // will be a different message displayed on the infobar.
  enum SessionRestoreMessageValue {
    OpenNewTabPage,
    ContinueWhereLeftOff,
    OpenSpecificPages
  };

  explicit SessionRestoreInfobarModel(PrefService& prefs);
  ~SessionRestoreInfobarModel();

  // Retrieves a value indicating the user's preference for session restore
  // behavior. This value is used to determine if a session restore message
  // should be displayed.
  SessionRestoreMessageValue GetSessionRestoreMessageValue();

  // Sets the infobar delegate to display the correct message in the infobar.
  void SetInfobarDelegate();

 private:
  raw_ref<PrefService> prefs_;
};

}  // namespace session_restore_infobar

#endif  // CHROME_BROWSER_UI_VIEWS_SESSION_RESTORE_INFOBAR_SESSION_RESTORE_INFOBAR_MODEL_H_
