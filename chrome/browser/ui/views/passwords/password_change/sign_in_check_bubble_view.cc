// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/sign_in_check_bubble_view.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "ui/base/metadata/metadata_impl_macros.h"

SignInCheckBubbleView::SignInCheckBubbleView(content::WebContents* web_contents,
                                             views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {}

SignInCheckBubbleView::~SignInCheckBubbleView() = default;

PasswordBubbleControllerBase* SignInCheckBubbleView::GetController() {
  return &controller_;
}

const PasswordBubbleControllerBase* SignInCheckBubbleView::GetController()
    const {
  return &controller_;
}

BEGIN_METADATA(SignInCheckBubbleView)
END_METADATA
