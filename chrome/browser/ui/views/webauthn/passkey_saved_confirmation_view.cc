// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_saved_confirmation_view.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/webauthn/passkey_saved_confirmation_controller.h"
#include "components/vector_icons/vector_icons.h"

PasskeySavedConfirmationView::PasskeySavedConfirmationView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetShowIcon(true);
  SetTitle(controller_.GetTitle());

  // TODO(b/345242100): Add passkey label row.
  // TODO(b/345242100): Add manage passkeys and passwords button.
}

PasskeySavedConfirmationView::~PasskeySavedConfirmationView() = default;

PasskeySavedConfirmationController*
PasskeySavedConfirmationView::GetController() {
  return &controller_;
}

const PasskeySavedConfirmationController*
PasskeySavedConfirmationView::GetController() const {
  return &controller_;
}

ui::ImageModel PasskeySavedConfirmationView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}
