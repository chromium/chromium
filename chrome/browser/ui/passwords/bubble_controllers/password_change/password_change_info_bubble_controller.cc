// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_info_bubble_controller.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"

PasswordChangeInfoBubbleController::PasswordChangeInfoBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate,
    PasswordChangeDelegate::State state)
    : PasswordBubbleControllerBase(
          delegate,
          password_manager::metrics_util::UIDisplayDisposition::
              PASSWORD_CHANGE_BUBBLE),
      state_(state),
      password_change_delegate_(
          delegate_->GetPasswordChangeDelegate()->AsWeakPtr()) {
  password_change_delegate_->AddObserver(this);
}

PasswordChangeInfoBubbleController::~PasswordChangeInfoBubbleController() {
  OnBubbleClosing();
  if (!password_change_delegate_) {
    return;
  }
  password_change_delegate_->RemoveObserver(this);
}

std::u16string PasswordChangeInfoBubbleController::GetTitle() const {
  switch (state_) {
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_SIGN_IN_CHECK_TITLE);
    case PasswordChangeDelegate::State::kChangingPassword:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_INFO_BUBBLE_TITLE);
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
      NOTIMPLEMENTED();
      break;
  }
  NOTREACHED();
}

void PasswordChangeInfoBubbleController::ReportInteractions() {
  // TODO(crbug.com/381053884): Report metrics.
}

void PasswordChangeInfoBubbleController::OnStateChanged(
    PasswordChangeDelegate::State new_state) {
  if (new_state == state_) {
    return;
  }
  PasswordBubbleViewBase::CloseCurrentBubble();
}

void PasswordChangeInfoBubbleController::OnPasswordChangeStopped(
    PasswordChangeDelegate* delegate) {
  PasswordBubbleViewBase::CloseCurrentBubble();
}

void PasswordChangeInfoBubbleController::CancelPasswordChange() {
  CHECK(password_change_delegate_);
  PasswordBubbleViewBase::CloseCurrentBubble();
  password_change_delegate_->Stop();
}

std::u16string PasswordChangeInfoBubbleController::GetDisplayOrigin() {
  return url_formatter::FormatUrlForSecurityDisplay(
      password_change_delegate_->GetChangePasswordUrl(),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

void PasswordChangeInfoBubbleController::OnGooglePasswordManagerLinkClicked() {
  if (delegate_) {
    delegate_->NavigateToPasswordManagerSettingsPage(
        password_manager::ManagePasswordsReferrer::kPasswordChangeInfoBubble);
  }
}

std::u16string PasswordChangeInfoBubbleController::GetPrimaryAccountEmail() {
  Profile* profile = GetProfile();
  return base::UTF8ToUTF16(GetDisplayableAccountName(
      SyncServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile)));
}
