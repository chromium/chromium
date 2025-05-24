// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/password_change_info_bubble_controller.h"

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace metrics_util = password_manager::metrics_util;

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
  if (state_ == PasswordChangeDelegate::State::kWaitingForChangePasswordForm) {
    return l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_UI_SIGN_IN_CHECK_TITLE);
  }
  if (state_ == PasswordChangeDelegate::State::kChangingPassword) {
    return l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_INFO_BUBBLE_TITLE);
  }
  NOTREACHED();
}

void PasswordChangeInfoBubbleController::ReportInteractions() {
  base::UmaHistogramEnumeration(
      "PasswordManager.PasswordChange.InformationBubble", dismissal_reason_,
      metrics_util::NUM_UI_RESPONSES);
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
  dismissal_reason_ = metrics_util::CLICKED_CANCEL;
  CHECK(password_change_delegate_);
  PasswordBubbleViewBase::CloseCurrentBubble();
  password_change_delegate_->Stop();
}

std::u16string PasswordChangeInfoBubbleController::GetDisplayOrigin() {
  return password_change_delegate_->GetDisplayOrigin();
}

void PasswordChangeInfoBubbleController::OnGooglePasswordManagerLinkClicked() {
  dismissal_reason_ = metrics_util::CLICKED_MANAGE_PASSWORD;
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
