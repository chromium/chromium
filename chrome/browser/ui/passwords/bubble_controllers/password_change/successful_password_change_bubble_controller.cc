// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/successful_password_change_bubble_controller.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_util.h"
#include "components/favicon_base/favicon_types.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace metrics_util = password_manager::metrics_util;

SuccessfulPasswordChangeBubbleController::
    SuccessfulPasswordChangeBubbleController(
        base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          delegate,
          password_manager::metrics_util::UIDisplayDisposition::
              PASSWORD_CHANGE_BUBBLE) {}

SuccessfulPasswordChangeBubbleController::
    ~SuccessfulPasswordChangeBubbleController() {
  OnBubbleClosing();
}

std::u16string SuccessfulPasswordChangeBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGED_TITLE);
}

void SuccessfulPasswordChangeBubbleController::ReportInteractions() {
  base::UmaHistogramEnumeration("PasswordManager.PasswordChange.SuccessBubble",
                                dismissal_reason_,
                                metrics_util::NUM_UI_RESPONSES);
}

void SuccessfulPasswordChangeBubbleController::OpenPasswordManager() {
  dismissal_reason_ = metrics_util::CLICKED_MANAGE_PASSWORD;
  if (delegate_) {
    delegate_->NavigateToPasswordManagerSettingsPage(
        password_manager::ManagePasswordsReferrer::kPasswordChangeInfoBubble);
  }
}

void SuccessfulPasswordChangeBubbleController::FinishPasswordChange() {
  dismissal_reason_ = metrics_util::CLICKED_ACCEPT;
}

void SuccessfulPasswordChangeBubbleController::AuthenticateUser(
    base::OnceCallback<void(bool)> auth_callback) {
  std::u16string message;
#if BUILDFLAG(IS_MAC)
  message = l10n_util::GetStringUTF16(
      IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
#elif BUILDFLAG(IS_WIN)
  message = l10n_util::GetStringUTF16(IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT);
#endif
  if (delegate_) {
    delegate_->AuthenticateUserWithMessage(message, std::move(auth_callback));
  }
}

std::u16string SuccessfulPasswordChangeBubbleController::GetUsername() const {
  return delegate_->PasswordChangeUsername();
}

std::u16string SuccessfulPasswordChangeBubbleController::GetNewPassword()
    const {
  return delegate_->PasswordChangeNewPassword();
}

void SuccessfulPasswordChangeBubbleController::RequestFavicon(
    base::OnceCallback<void(const gfx::Image&)> favicon_ready_callback) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(GetProfile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  favicon::GetFaviconImageForPageURL(
      favicon_service, GetWebContents()->GetVisibleURL(),
      favicon_base::IconType::kFavicon,
      base::BindOnce(
          [](base::OnceCallback<void(const gfx::Image&)> favicon_ready_callback,
             const favicon_base::FaviconImageResult& result) {
            std::move(favicon_ready_callback).Run(result.image);
          },
          std::move(favicon_ready_callback)),
      &favicon_tracker_);
}

base::WeakPtr<SuccessfulPasswordChangeBubbleController>
SuccessfulPasswordChangeBubbleController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
