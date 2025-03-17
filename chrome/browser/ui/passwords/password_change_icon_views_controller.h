// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_ICON_VIEWS_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_ICON_VIEWS_CONTROLLER_H_

#include "base/functional/callback.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "content/public/browser/web_contents.h"

// Controller for the `PasswordChangeIconViews`. It observes the password change
// state changes and triggers view updates.
class PasswordChangeIconViewsController
    : public PasswordChangeDelegate::Observer {
 public:
  PasswordChangeIconViewsController(
      base::RepeatingClosure update_ui_callback,
      base::RepeatingClosure update_visibility_callback);
  PasswordChangeIconViewsController(const PasswordChangeIconViewsController&) =
      delete;
  PasswordChangeIconViewsController& operator=(
      const PasswordChangeIconViewsController&) = delete;
  ~PasswordChangeIconViewsController() override;

  void SetPasswordChangeDelegate(PasswordChangeDelegate* delegate);

  PasswordChangeDelegate::State GetCurrentState() const;

 private:
  // PasswordChangeDelegate::Observer
  void OnStateChanged(PasswordChangeDelegate::State state) override;
  void OnPasswordChangeStopped(PasswordChangeDelegate* delegate) override;

  // Updates the icon and the label in the view.
  base::RepeatingClosure update_ui_callback_;
  // Hides/shows the view. This needs to be called if password change flow is
  // finished/canceled.
  base::RepeatingClosure update_visibility_callback_;
  raw_ptr<PasswordChangeDelegate> password_change_delegate_ = nullptr;
  PasswordChangeDelegate::State state_ =
      static_cast<PasswordChangeDelegate::State>(-1);

  base::ScopedObservation<PasswordChangeDelegate,
                          PasswordChangeDelegate::Observer>
      scoped_observation_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_CHANGE_ICON_VIEWS_CONTROLLER_H_
