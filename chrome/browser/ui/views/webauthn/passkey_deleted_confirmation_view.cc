// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_deleted_confirmation_view.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/webauthn/passkey_deleted_confirmation_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

PasskeyDeletedConfirmationView::PasskeyDeletedConfirmationView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason display_reason)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents),
                  display_reason == AUTOMATIC
                      ? password_manager::metrics_util::
                            AUTOMATIC_PASSKEY_DELETED_CONFIRMATION
                      : password_manager::metrics_util::
                            MANUAL_PASSKEY_DELETED_CONFIRMATION) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowIcon(true);
  SetTitle(controller_.GetTitle());
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_PASSKEY_UPDATE_NEEDED_LABEL));
  label->SetTextContext(views::style::CONTEXT_LABEL);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  AddChildView(std::move(label));
}

PasskeyDeletedConfirmationView::~PasskeyDeletedConfirmationView() = default;

PasskeyDeletedConfirmationController*
PasskeyDeletedConfirmationView::GetController() {
  return &controller_;
}

const PasskeyDeletedConfirmationController*
PasskeyDeletedConfirmationView::GetController() const {
  return &controller_;
}

ui::ImageModel PasskeyDeletedConfirmationView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

BEGIN_METADATA(PasskeyDeletedConfirmationView)
END_METADATA
