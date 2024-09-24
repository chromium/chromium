// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_saved_confirmation_view.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/webauthn/passkey_saved_confirmation_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

PasskeySavedConfirmationView::PasskeySavedConfirmationView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowIcon(true);
  SetTitle(controller_.GetTitle());
  SetLayoutManager(std::make_unique<views::FillLayout>());

  size_t offset;
  const std::u16string link =
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_PASSKEY_SAVED_LINK);
  std::u16string text = l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_GPM_PASSKEY_SAVED_LABEL, link, &offset);

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(text);
  label->SetTextContext(views::style::CONTEXT_BUBBLE_FOOTER);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->AddStyleRange(
      gfx::Range(offset, offset + link.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PasskeySavedConfirmationView::OnGooglePasswordManagerLinkClicked,
          base::Unretained(this))));
  AddChildView(std::move(label));
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

void PasskeySavedConfirmationView::OnGooglePasswordManagerLinkClicked() {
  controller_.OnGooglePasswordManagerLinkClicked();
  CloseBubble();
}

BEGIN_METADATA(PasskeySavedConfirmationView)
END_METADATA
