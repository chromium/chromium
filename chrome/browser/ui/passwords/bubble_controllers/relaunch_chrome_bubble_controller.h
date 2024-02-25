// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_RELAUNCH_CHROME_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_RELAUNCH_CHROME_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/prefs/pref_service.h"

// This controller manages the relaunch Chrome bubble, which is shown while
// password form is rendered and user has no keychain access.
class RelaunchChromeBubbleController : public PasswordBubbleControllerBase {
 public:
  explicit RelaunchChromeBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate,
      PrefService* prefs);
  ~RelaunchChromeBubbleController() override;

  std::u16string GetTitle() const override;
  std::u16string GetBody() const;
  std::u16string GetContinueButtonText() const;
  std::u16string GetNoThanksButtonText() const;

  // The user chose to relaunch the Chrome.
  void OnAccepted();
  // The user chose not to relaunch the Chrome.
  void OnCanceled();

 private:
  // PasswordBubbleControllerBase:
  void ReportInteractions() override;

  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;

  raw_ptr<PrefService> prefs_;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_RELAUNCH_CHROME_BUBBLE_CONTROLLER_H_
