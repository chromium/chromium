// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/privacy_notice_view.h"

#include "chrome/browser/ui/passwords/bubble_controllers/password_change/privacy_notice_bubble_view_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"

PrivacyNoticeView::PrivacyNoticeView(content::WebContents* web_contents,
                                     views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetAcceptCallback(
      base::BindOnce(&PrivacyNoticeBubbleViewController::AcceptNotice,
                     base::Unretained(&controller_)));
}

PrivacyNoticeView::~PrivacyNoticeView() = default;

PasswordBubbleControllerBase* PrivacyNoticeView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* PrivacyNoticeView::GetController() const {
  return &controller_;
}

BEGIN_METADATA(PrivacyNoticeView)
END_METADATA
