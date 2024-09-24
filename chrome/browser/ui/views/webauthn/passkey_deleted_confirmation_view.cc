// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_deleted_confirmation_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
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
  SetShowIcon(true);
  SetTitle(controller_.GetTitle());
  set_title_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_GOT_IT_BUTTON));
  SetAcceptCallback(base::BindOnce(
      &PasskeyDeletedConfirmationController::OnGotItButtonClicked,
      base::Unretained(&controller_)));

  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_MANAGE_PASSKEYS_BUTTON));
  SetButtonStyle(ui::mojom::DialogButton::kCancel, ui::ButtonStyle::kTonal);
  SetCancelCallback(base::BindOnce(
      &PasskeyDeletedConfirmationView::OnManagePasskeysButtonClicked,
      base::Unretained(this)));

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

void PasskeyDeletedConfirmationView::OnManagePasskeysButtonClicked() {
  controller_.OnManagePasskeysButtonClicked();
  CloseBubble();
}

BEGIN_METADATA(PasskeyDeletedConfirmationView)
END_METADATA
