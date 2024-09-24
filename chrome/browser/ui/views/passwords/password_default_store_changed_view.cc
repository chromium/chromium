// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_default_store_changed_view.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/layout/fill_layout.h"

PasswordDefaultStoreChangedView::PasswordDefaultStoreChangedView(
    content::WebContents* web_contents,
    views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller_.GetContinueButtonText());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 controller_.GetGoToSettingsButtonText());

  SetShowIcon(true);
  SetTitle(controller_.GetTitle());

  auto label = std::make_unique<views::Label>();
  label->SetText(controller_.GetBody());
  label->SetMultiLine(/*multi_line=*/true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  AddChildView(std::move(label));

  SetAcceptCallback(base::BindOnce(
      &DefaultStoreChangedBubbleController::OnContinueButtonClicked,
      base::Unretained(&controller_)));
  SetCancelCallback(base::BindOnce(
      &DefaultStoreChangedBubbleController::OnNavigateToSettingsButtonClicked,
      base::Unretained(&controller_)));
}

PasswordDefaultStoreChangedView::~PasswordDefaultStoreChangedView() = default;

DefaultStoreChangedBubbleController*
PasswordDefaultStoreChangedView::GetController() {
  return &controller_;
}

const DefaultStoreChangedBubbleController*
PasswordDefaultStoreChangedView::GetController() const {
  return &controller_;
}

ui::ImageModel PasswordDefaultStoreChangedView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(GooglePasswordManagerVectorIcon(),
                                        ui::kColorIcon);
}

BEGIN_METADATA(PasswordDefaultStoreChangedView)
END_METADATA
