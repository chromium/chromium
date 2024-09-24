// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/biometric_authentication_for_filling_bubble_view.h"

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

BiometricAuthenticationForFillingBubbleView::
    BiometricAuthenticationForFillingBubbleView(
        content::WebContents* web_contents,
        views::View* anchor_view,
        PrefService* prefs,
        DisplayReason display_reason)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(
          PasswordsModelDelegateFromWebContents(web_contents),
          prefs,
          display_reason == AUTOMATIC
              ? PasswordBubbleControllerBase::DisplayReason::kAutomatic
              : PasswordBubbleControllerBase::DisplayReason::kUserAction) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller_.GetContinueButtonText());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 controller_.GetNoThanksButtonText());

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(controller_.GetBody());
  label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  AddChildView(std::move(label));

  SetAcceptCallback(base::BindOnce(
      &BiometricAuthenticationForFillingBubbleController::OnAccepted,
      base::Unretained(&controller_)));

  SetCancelCallback(base::BindOnce(
      &BiometricAuthenticationForFillingBubbleController::OnCanceled,
      base::Unretained(&controller_)));
}

BiometricAuthenticationForFillingBubbleView::
    ~BiometricAuthenticationForFillingBubbleView() = default;

BiometricAuthenticationForFillingBubbleController*
BiometricAuthenticationForFillingBubbleView::GetController() {
  return &controller_;
}

const BiometricAuthenticationForFillingBubbleController*
BiometricAuthenticationForFillingBubbleView::GetController() const {
  return &controller_;
}

void BiometricAuthenticationForFillingBubbleView::AddedToWidget() {
  SetBubbleHeader(controller_.GetImageID(/*dark=*/false),
                  controller_.GetImageID(/*dark=*/true));
}

BEGIN_METADATA(BiometricAuthenticationForFillingBubbleView)
END_METADATA
