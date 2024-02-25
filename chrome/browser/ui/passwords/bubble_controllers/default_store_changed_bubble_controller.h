// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_DEFAULT_STORE_CHANGED_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_DEFAULT_STORE_CHANGED_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

// This controller manages the bubble which is shown if default password store
// was changed without user interaction.
class DefaultStoreChangedBubbleController
    : public PasswordBubbleControllerBase {
 public:
  explicit DefaultStoreChangedBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);
  ~DefaultStoreChangedBubbleController() override;

  std::u16string GetTitle() const override;

  std::u16string GetBody() const;
  std::u16string GetContinueButtonText() const;
  std::u16string GetGoToSettingsButtonText() const;

  void OnContinueButtonClicked();
  void OnNavigateToSettingsButtonClicked();

 private:
  // PasswordBubbleControllerBase:
  void ReportInteractions() override;

  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_DEFAULT_STORE_CHANGED_BUBBLE_CONTROLLER_H_
