// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/privacy_notice_bubble_view_controller.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"

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
  // TODO(crbug.com/381053884): Add string.
  return u"Lorem ipsum";
}

void PrivacyNoticeBubbleViewController::ReportInteractions() {
  // TODO(crbug.com/381053884): Report metrics.
}

void PrivacyNoticeBubbleViewController::AcceptNotice() {
  password_change_delegate_->OnPrivacyNoticeAccepted();
}
