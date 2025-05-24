// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/privacy_notice_bubble_view_controller.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace metrics_util = password_manager::metrics_util;

PrivacyNoticeBubbleViewController::PrivacyNoticeBubbleViewController(
    base::WeakPtr<PasswordsModelDelegate> delegate)
    : PasswordBubbleControllerBase(
          delegate,
          password_manager::metrics_util::UIDisplayDisposition::
              PASSWORD_CHANGE_BUBBLE),
      password_change_delegate_(
          delegate_->GetPasswordChangeDelegate()->AsWeakPtr()) {}

PrivacyNoticeBubbleViewController::~PrivacyNoticeBubbleViewController() {
  OnBubbleClosing();
}

std::u16string PrivacyNoticeBubbleViewController::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_PASSWORD_CHANGE_PRIVACY_NOTICE_TITLE);
}

void PrivacyNoticeBubbleViewController::ReportInteractions() {
  base::UmaHistogramEnumeration(
      "PasswordManager.PasswordChange.PrivacyNoticeBubble", dismissal_reason_,
      metrics_util::NUM_UI_RESPONSES);
}

void PrivacyNoticeBubbleViewController::AcceptNotice() {
  dismissal_reason_ = metrics_util::CLICKED_ACCEPT;
  password_change_delegate_->OnPrivacyNoticeAccepted();
}

void PrivacyNoticeBubbleViewController::Cancel() {
  dismissal_reason_ = metrics_util::CLICKED_CANCEL;
  CHECK(password_change_delegate_);
  password_change_delegate_->Stop();
}
