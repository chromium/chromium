// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/post_save_compromised_bubble_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

PostSaveCompromisedBubbleView::PostSaveCompromisedBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*auto_dismissable=*/false),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  std::u16string button = controller_.GetButtonText();
  if (button.empty()) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  } else {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
    SetButtonLabel(ui::mojom::DialogButton::kOk, std::move(button));
  }

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(controller_.GetBody());
  label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  gfx::Range range = controller_.GetSettingLinkRange();
  if (!range.is_empty()) {
    label->AddStyleRange(
        range,
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &PostSaveCompromisedBubbleController::OnSettingsClicked,
            base::Unretained(&controller_))));
  }
  AddChildView(std::move(label));

  SetAcceptCallback(
      base::BindOnce(&PostSaveCompromisedBubbleController::OnAccepted,
                     base::Unretained(&controller_)));
  SetShowIcon(true);
}

PostSaveCompromisedBubbleView::~PostSaveCompromisedBubbleView() = default;

PostSaveCompromisedBubbleController*
PostSaveCompromisedBubbleView::GetController() {
  return &controller_;
}

const PostSaveCompromisedBubbleController*
PostSaveCompromisedBubbleView::GetController() const {
  return &controller_;
}

ui::ImageModel PostSaveCompromisedBubbleView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

void PostSaveCompromisedBubbleView::AddedToWidget() {
  SetBubbleHeader(controller_.GetImageID(/*dark=*/false),
                  controller_.GetImageID(/*dark=*/true));
}

BEGIN_METADATA(PostSaveCompromisedBubbleView)
END_METADATA
