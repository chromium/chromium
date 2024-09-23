// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_confirmation_bubble_view.h"

#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

MandatoryReauthConfirmationBubbleView::MandatoryReauthConfirmationBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    MandatoryReauthBubbleController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

void MandatoryReauthConfirmationBubbleView::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void MandatoryReauthConfirmationBubbleView::Hide() {
  CloseBubble();
  WindowClosing();
  controller_ = nullptr;
}

void MandatoryReauthConfirmationBubbleView::AddedToWidget() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto* mandatory_reauth_opt_in_banner =
      bundle.GetImageSkiaNamed(IDR_AUTOFILL_MANDATORY_REAUTH_CONFIRMATION);
  GetBubbleFrameView()->SetHeaderView(
      std::make_unique<ThemeTrackingNonAccessibleImageView>(
          *mandatory_reauth_opt_in_banner, *mandatory_reauth_opt_in_banner,
          base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                              base::Unretained(this))));
}

std::u16string MandatoryReauthConfirmationBubbleView::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void MandatoryReauthConfirmationBubbleView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

MandatoryReauthConfirmationBubbleView::
    ~MandatoryReauthConfirmationBubbleView() = default;

void MandatoryReauthConfirmationBubbleView::OnSettingsLinkClicked() {
  autofill_metrics::LogMandatoryReauthOptInConfirmationBubbleMetric(
      autofill_metrics::MandatoryReauthOptInConfirmationBubbleMetric::
          kSettingsLinkClicked);
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  chrome::ShowSettingsSubPage(browser, chrome::kPaymentsSubPage);
}

void MandatoryReauthConfirmationBubbleView::Init() {
  SetID(DialogViewId::MAIN_CONTENT_VIEW_LOCAL);
  SetProperty(views::kMarginsKey, gfx::Insets());
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  std::u16string link_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_MANDATORY_REAUTH_CONFIRMATION_SETTINGS_LINK);
  size_t link_offset;
  std::u16string explanation_text = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_MANDATORY_REAUTH_CONFIRMATION_EXPLANATION, link_text,
      &link_offset);
  views::StyledLabel::RangeStyleInfo style_info =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &MandatoryReauthConfirmationBubbleView::OnSettingsLinkClicked,
          weak_ptr_factory_.GetWeakPtr()));
  AddChildView(views::Builder<views::StyledLabel>()
                   .SetID(DialogViewId::SETTINGS_LABEL)
                   .SetText(explanation_text)
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetDefaultTextStyle(views::style::STYLE_SECONDARY)
                   .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                   .AddStyleRange(gfx::Range(link_offset,
                                             link_offset + link_text.length()),
                                  style_info)
                   .Build());
}

}  // namespace autofill
