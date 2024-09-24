// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_not_accepted_bubble_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

PasskeyNotAcceptedBubbleView::PasskeyNotAcceptedBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason display_reason)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents),
                  display_reason == AUTOMATIC
                      ? password_manager::metrics_util::
                            AUTOMATIC_PASSKEY_NOT_ACCEPTED_BUBBLE
                      : password_manager::metrics_util::
                            MANUAL_PASSKEY_NOT_ACCEPTED_BUBBLE) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowIcon(true);
  SetTitle(controller_.GetTitle());
  SetLayoutManager(std::make_unique<views::FillLayout>());

  size_t offset;
  const std::u16string link =
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_GPM_PASSKEY_DELETED_LINK);
  std::u16string text = l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_GPM_PASSKEY_DELETED_LABEL, link, &offset);

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(text);
  label->SetTextContext(views::style::CONTEXT_BUBBLE_FOOTER);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->AddStyleRange(
      gfx::Range(offset, offset + link.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PasskeyNotAcceptedBubbleView::OnGooglePasswordManagerLinkClicked,
          base::Unretained(this))));
  AddChildView(std::move(label));
}

PasskeyNotAcceptedBubbleView::~PasskeyNotAcceptedBubbleView() = default;

PasskeyNotAcceptedBubbleController*
PasskeyNotAcceptedBubbleView::GetController() {
  return &controller_;
}

const PasskeyNotAcceptedBubbleController*
PasskeyNotAcceptedBubbleView::GetController() const {
  return &controller_;
}

ui::ImageModel PasskeyNotAcceptedBubbleView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

void PasskeyNotAcceptedBubbleView::OnGooglePasswordManagerLinkClicked() {
  controller_.OnGooglePasswordManagerLinkClicked();
  CloseBubble();
}

BEGIN_METADATA(PasskeyNotAcceptedBubbleView)
END_METADATA
