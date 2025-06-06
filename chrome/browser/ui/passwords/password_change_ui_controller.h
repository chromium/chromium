// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/password_change_delegate.h"

namespace content {
class WebContents;
}  // namespace content

// Responsible for creating and displaying appropriate views based on the
// current state of the password change flow.
class PasswordChangeUIController {
 public:
  explicit PasswordChangeUIController(
      PasswordChangeDelegate* password_change_delegate,
      base::WeakPtr<content::WebContents> web_contents);
  ~PasswordChangeUIController();

  // Updates the `state_` and the UI.
  void UpdateState(PasswordChangeDelegate::State state);

 private:
  // Handles clicking accept button on the currently displayed dialog.
  void OnDialogAccepted();

  // Controls password change process. Owns this class.
  const raw_ptr<PasswordChangeDelegate> password_change_delegate_;

  base::WeakPtr<content::WebContents> web_contents_;

  // Current state of the password change flow.
  PasswordChangeDelegate::State state_ =
      static_cast<PasswordChangeDelegate::State>(-1);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_UI_CONTROLLER_H_
