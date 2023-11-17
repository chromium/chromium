// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/relaunch_chrome_bubble_controller.h"

#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr int kMaxNumberOfTimesBubbleIsShown = 3;

// Returns whether to use Google Chrome branded strings.
constexpr bool UsesPasswordManagerGoogleBranding() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}
}  // namespace

RelaunchChromeBubbleController::RelaunchChromeBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate,
    PrefService* prefs)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          password_manager::metrics_util::AUTOMATIC_RELAUNCH_CHROME_BUBBLE),
      prefs_(prefs) {}

RelaunchChromeBubbleController::~RelaunchChromeBubbleController() {
  OnBubbleClosing();
}

std::u16string RelaunchChromeBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(
      UsesPasswordManagerGoogleBranding()
          ? IDS_PASSWORD_MANAGER_RELAUNCH_CHROME_BUBBLE_TITLE_BRANDED
          : IDS_PASSWORD_MANAGER_RELAUNCH_CHROME_BUBBLE_TITLE_NON_BRANDED);
}

std::u16string RelaunchChromeBubbleController::GetBody() const {
  return l10n_util::GetStringUTF16(
      UsesPasswordManagerGoogleBranding()
          ? IDS_PASSWORD_MANAGER_RELAUNCH_CHROME_BUBBLE_DESCRIPTION_BRANDED
          : IDS_PASSWORD_MANAGER_RELAUNCH_CHROME_BUBBLE_DESCRIPTION_NON_BRANDED);
}

std::u16string RelaunchChromeBubbleController::GetContinueButtonText() const {
  return l10n_util::GetStringUTF16(
      UsesPasswordManagerGoogleBranding()
          ? IDS_PASSWORD_MANAGER_RELAUNCH_CHROME_BUBBLE_ACCEPT_BUTTON_BRANDED
          : IDS_PASSWORD_MANAGER_RELAUNCH_CHROME_BUBBLE_ACCEPT_BUTTON_NON_BRANDED);
}

std::u16string RelaunchChromeBubbleController::GetNoThanksButtonText() const {
  if (prefs_->GetInteger(
          password_manager::prefs::kRelaunchChromeBubbleDismissedCounter) >=
      kMaxNumberOfTimesBubbleIsShown) {
    return l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_RELAUNCH_CHROME_BUBBLE_NEVER_BUTTON);
  }

  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_RELAUNCH_CHROME_BUBBLE_CANCEL_BUTTON);
}

void RelaunchChromeBubbleController::OnAccepted() {
  delegate_->RelaunchChrome();
}

void RelaunchChromeBubbleController::OnCanceled() {
  prefs_->SetInteger(
      password_manager::prefs::kRelaunchChromeBubbleDismissedCounter,
      prefs_->GetInteger(
          password_manager::prefs::kRelaunchChromeBubbleDismissedCounter) +
          1);
}
