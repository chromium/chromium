// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/biometric_authentication_confirmation_bubble_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

BiometricAuthenticationConfirmationBubbleView::
    BiometricAuthenticationConfirmationBubbleView(
        content::WebContents* web_contents,
        views::View* anchor_view)
    : PasswordBubbleViewBase(web_contents,
                             anchor_view,
                             /*easily_dismissable=*/true),
      controller_(PasswordsModelDelegateFromWebContents(web_contents)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  AddChildView(CreateGooglePasswordManagerLabel(
  /*text_message_id=*/
#if BUILDFLAG(IS_MAC)
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_CONFIRMATION_DESCRIPTION_MAC,
#elif BUILDFLAG(IS_WIN)
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_CONFIRMATION_DESCRIPTION_WIN,
#elif BUILDFLAG(IS_CHROMEOS_ASH)
      IDS_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATION_CONFIRMATION_DESCRIPTION_CHROMEOS,
#endif
      /*link_message_id=*/
      IDS_PASSWORD_MANAGER_SETTINGS,
      base::BindRepeating(&BiometricAuthenticationConfirmationBubbleView::
                              StyledLabelLinkClicked,
                          base::Unretained(this))));
}

BiometricAuthenticationConfirmationBubbleView::
    ~BiometricAuthenticationConfirmationBubbleView() = default;

BiometricAuthenticationConfirmationBubbleController*
BiometricAuthenticationConfirmationBubbleView::GetController() {
  return &controller_;
}

const BiometricAuthenticationConfirmationBubbleController*
BiometricAuthenticationConfirmationBubbleView::GetController() const {
  return &controller_;
}

void BiometricAuthenticationConfirmationBubbleView::AddedToWidget() {
  SetBubbleHeader(controller_.GetImageID(/*dark=*/false),
                  controller_.GetImageID(/*dark=*/true));
}

void BiometricAuthenticationConfirmationBubbleView::StyledLabelLinkClicked() {
  controller_.OnNavigateToSettingsLinkClicked();
  CloseBubble();
}

BEGIN_METADATA(BiometricAuthenticationConfirmationBubbleView)
END_METADATA
