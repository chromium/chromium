// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/subpage_view.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace autofill {
DEFINE_ELEMENT_IDENTIFIER_VALUE(kOfferNotificationBubbleElementId);

OfferNotificationBubbleViews::OfferNotificationBubbleViews(
    views::BubbleAnchor anchor_view,
    content::WebContents* web_contents,
    OfferNotificationBubbleController* controller)
    : AutofillLocationBarBubble(anchor_view,
                                web_contents,
                                /*autosize=*/true),
      controller_(controller) {
  CHECK(controller, base::NotFatalUntil::M162);
  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetProperty(views::kElementIdentifierKey, kOfferNotificationBubbleElementId);
}

OfferNotificationBubbleViews::~OfferNotificationBubbleViews() {
  Hide();
}

void OfferNotificationBubbleViews::Hide() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsUiClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void OfferNotificationBubbleViews::Init() {
  // Offers have no CTA buttons.
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  // Create bubble content:
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kVertical);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  const AutofillOfferData* offer = controller_->GetOffer();
  CHECK(offer);
  CHECK(offer->GetOfferDetailsUrl().is_valid());

  std::vector<size_t> offsets;
  const std::u16string label_text = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_WALLET_DIRECT_OFFER_REMINDER_BODY_TEXT,
      /*a=*/std::u16string(), /*b=*/std::u16string(), &offsets);
  CHECK_EQ(offsets.size(), 2u);

  wallet_direct_offer_label_ =
      AddChildView(std::make_unique<views::StyledLabel>());
  wallet_direct_offer_label_->SetText(label_text);
  wallet_direct_offer_label_->SetTextContext(
      ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL);
  wallet_direct_offer_label_->SetDefaultTextStyle(
      views::style::STYLE_SECONDARY);
  wallet_direct_offer_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  wallet_direct_offer_label_->AddStyleRange(
      gfx::Range(offsets[0], offsets[1]),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &OfferNotificationBubbleViews::OnOfferDetailsLinkClicked,
          base::Unretained(this))));
}

void OfferNotificationBubbleViews::AddedToWidget() {
  // Offers are surfaced from the user's Google Wallet, so they show the Google
  // Wallet logo next to the title.
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(
          GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_WALLET));

  // The Google Wallet logo is shown next to the title, so include it in the
  // accessible title.
  SetAccessibleTitle(l10n_util::GetStringFUTF16(
      IDS_CONCAT_TWO_STRINGS_WITH_COMMA, GetWindowTitle(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_GOOGLE_WALLET_LOGO_ACCESSIBLE_NAME)));
}

views::View* OfferNotificationBubbleViews::GetInitiallyFocusedView() {
  // The bubble has no dialog buttons, so focus the close button.
  views::BubbleFrameView* frame_view = GetBubbleFrameView();
  return frame_view ? frame_view->close_button() : nullptr;
}

std::u16string OfferNotificationBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void OfferNotificationBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsUiClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

void OfferNotificationBubbleViews::OnWidgetDestroying(views::Widget* widget) {
  LocationBarBubbleDelegateView::OnWidgetDestroying(widget);
  if (!widget->IsClosed()) {
    return;
  }
  CHECK_NE(widget->closed_reason(),
           views::Widget::ClosedReason::kCancelButtonClicked,
           base::NotFatalUntil::M162);
}

void OfferNotificationBubbleViews::OnOfferDetailsLinkClicked() {
  CHECK(controller_->GetOffer()->GetOfferDetailsUrl().is_valid(),
        base::NotFatalUntil::M162);
  web_contents()->OpenURL(
      content::OpenURLParams::CreateBrowserInitiated(
          GURL(controller_->GetOffer()->GetOfferDetailsUrl()),
          WindowOpenDisposition::kNewForegroundTab, ui::PAGE_TRANSITION_LINK),
      /*navigation_handle_callback=*/{});
}

BEGIN_METADATA(OfferNotificationBubbleViews)
END_METADATA

}  // namespace autofill
