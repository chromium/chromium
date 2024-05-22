// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/post_save_compromised_bubble_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "ui/base/l10n/l10n_util.h"

PostSaveCompromisedBubbleController::PostSaveCompromisedBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          password_manager::metrics_util::
              AUTOMATIC_COMPROMISED_CREDENTIALS_REMINDER) {
  switch (delegate_->GetState()) {
    case password_manager::ui::PASSWORD_UPDATED_SAFE_STATE:
      type_ = BubbleType::kPasswordUpdatedSafeState;
      break;
    case password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX:
      type_ = BubbleType::kPasswordUpdatedWithMoreToFix;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  base::UmaHistogramEnumeration("PasswordBubble.CompromisedBubble.Type", type_);
}

PostSaveCompromisedBubbleController::~PostSaveCompromisedBubbleController() {
  // Make sure the interactions are reported even if Views didn't notify the
  // controller about the bubble being closed.
  OnBubbleClosing();
}

std::u16string PostSaveCompromisedBubbleController::GetBody() {
  switch (type_) {
    case BubbleType::kPasswordUpdatedSafeState: {
      std::u16string link = l10n_util::GetStringUTF16(
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT);
      size_t offset = 0;
      std::u16string body = l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_SAFE_STATE_BODY_MESSAGE_GOOGLE_PASSWORD_MANAGER,
          link, &offset);
      link_range_ = gfx::Range(offset, offset + link.size());
      return body;
    }
    case BubbleType::kPasswordUpdatedWithMoreToFix:
      return l10n_util::GetPluralStringFUTF16(
          IDS_PASSWORD_MANAGER_MORE_TO_FIX_BODY_MESSAGE_GOOGLE_PASSWORD_MANAGER,
          delegate_->GetTotalNumberCompromisedPasswords());
  }
}

gfx::Range PostSaveCompromisedBubbleController::GetSettingLinkRange() const {
  return link_range_;
}

std::u16string PostSaveCompromisedBubbleController::GetButtonText() const {
  switch (type_) {
    case BubbleType::kPasswordUpdatedSafeState:
      return std::u16string();
    case BubbleType::kPasswordUpdatedWithMoreToFix:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_CHECK_REMAINING_BUTTON);
  }
}

int PostSaveCompromisedBubbleController::GetImageID(bool dark) const {
  switch (type_) {
    case BubbleType::kPasswordUpdatedSafeState:
      return dark ? IDR_SAVED_PASSWORDS_SAFE_STATE_DARK
                  : IDR_SAVED_PASSWORDS_SAFE_STATE;
    case BubbleType::kPasswordUpdatedWithMoreToFix:
      return dark ? IDR_SAVED_PASSWORDS_NEUTRAL_STATE_DARK
                  : IDR_SAVED_PASSWORDS_NEUTRAL_STATE;
  }
}

void PostSaveCompromisedBubbleController::OnAccepted() {
  using password_manager::PasswordCheckReferrer;
  PasswordCheckReferrer referrer;
  switch (type_) {
    case BubbleType::kPasswordUpdatedSafeState:
      NOTREACHED_IN_MIGRATION();
      return;
    case BubbleType::kPasswordUpdatedWithMoreToFix:
      referrer = PasswordCheckReferrer::kMoreToFixBubble;
      break;
  }
  if (delegate_) {
    delegate_->NavigateToPasswordCheckup(referrer);
  }
}

void PostSaveCompromisedBubbleController::OnSettingsClicked() {
  if (delegate_) {
    delegate_->NavigateToPasswordManagerSettingsPage(
        password_manager::ManagePasswordsReferrer::kSafeStateBubble);
  }
}

std::u16string PostSaveCompromisedBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UPDATED_BUBBLE_TITLE);
}

void PostSaveCompromisedBubbleController::ReportInteractions() {}
