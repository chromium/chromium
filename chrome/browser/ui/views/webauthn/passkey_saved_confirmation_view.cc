// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_saved_confirmation_view.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/webauthn/authenticator_common_views.h"
#include "chrome/browser/ui/webauthn/passkey_saved_confirmation_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/fill_layout.h"

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
  set_title_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // TODO(b/345242100): Pass the username.
  AddChildView(CreatePasskeyIconWithLabelRow(vector_icons::kPasskeyIcon, u""));

  std::u16string button_label =
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_MANAGE_PASSWORDS_AND_PASSKEYS);
  SetFootnoteView(std::make_unique<RichHoverButton>(
      base::BindRepeating(
          &PasskeySavedConfirmationView::OnManagePasswordsAndPasskeysClicked,
          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsIcon,
                                     ui::kColorIcon),
      button_label,
      /*secondary_text=*/std::u16string(), button_label,
      /*subtitle_text=*/std::u16string(),
      ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                     ui::kColorIconSecondary),
      /*state_icon=*/std::nullopt));
  set_footnote_margins(gfx::Insets());
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

void PasskeySavedConfirmationView::OnManagePasswordsAndPasskeysClicked() {
  controller_.OnManagePasswordsAndPasskeysClicked();
  CloseBubble();
}
