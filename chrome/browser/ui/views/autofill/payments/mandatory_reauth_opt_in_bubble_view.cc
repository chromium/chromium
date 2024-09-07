// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_opt_in_bubble_view.h"

#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

MandatoryReauthOptInBubbleView::MandatoryReauthOptInBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    MandatoryReauthBubbleController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller) {
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller->GetAcceptButtonText());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 controller->GetCancelButtonText());
  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

void MandatoryReauthOptInBubbleView::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void MandatoryReauthOptInBubbleView::Hide() {
  CloseBubble();
  WindowClosing();
  controller_ = nullptr;
}

void MandatoryReauthOptInBubbleView::AddedToWidget() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto* mandatory_reauth_opt_in_banner =
      bundle.GetImageSkiaNamed(IDR_AUTOFILL_MANDATORY_REAUTH_OPT_IN);
  GetBubbleFrameView()->SetHeaderView(
      std::make_unique<ThemeTrackingNonAccessibleImageView>(
          *mandatory_reauth_opt_in_banner, *mandatory_reauth_opt_in_banner,
          base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                              base::Unretained(this))));
}

std::u16string MandatoryReauthOptInBubbleView::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void MandatoryReauthOptInBubbleView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

MandatoryReauthOptInBubbleView::~MandatoryReauthOptInBubbleView() = default;

void MandatoryReauthOptInBubbleView::Init() {
  SetID(DialogViewId::MAIN_CONTENT_VIEW_LOCAL);
  SetProperty(views::kMarginsKey, gfx::Insets());
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddChildView(views::Builder<views::Label>()
                   .SetText(controller_->GetExplanationText())
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetTextStyle(views::style::STYLE_SECONDARY)
                   .SetMultiLine(true)
                   .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                   .Build());
}

}  // namespace autofill
