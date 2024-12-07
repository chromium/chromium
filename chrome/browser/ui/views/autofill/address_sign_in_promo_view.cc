// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/address_sign_in_promo_view.h"

#include "chrome/browser/ui/autofill/address_bubbles_controller.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/promos/autofill_bubble_signin_promo_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

AddressSignInPromoView::AddressSignInPromoView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    base::OnceCallback<void(content::WebContents*)> move_address_callback)
    : AddressBubbleBaseView(anchor_view, web_contents),
      web_contents_(web_contents) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetTitle(IDS_AUTOFILL_SIGNIN_PROMO_TITLE_ADDRESS);
  SetShowCloseButton(true);
  // TODO(crbug.com/382447697): Change this to focus the full bubble instead of
  // the close button.
  SetInitiallyFocusedView(this);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  // Add an accessibility alert view first so that it does not overlap with
  // any other child view.
  views::View* accessibility_alert =
      AddChildView(std::make_unique<views::View>());

  // Show the sign in promo.
  auto sign_in_promo = std::make_unique<AutofillBubbleSignInPromoView>(
      web_contents, signin_metrics::AccessPoint::ACCESS_POINT_ADDRESS_BUBBLE,
      std::move(move_address_callback));
  AddChildView(std::move(sign_in_promo));

  // Notify the screen reader that the bubble changed.
  views::ViewAccessibility& ax = accessibility_alert->GetViewAccessibility();
  ax.SetRole(ax::mojom::Role::kAlert);
  ax.SetName(GetWindowTitle(), ax::mojom::NameFrom::kAttribute);
  accessibility_alert->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

AddressSignInPromoView::~AddressSignInPromoView() = default;

void AddressSignInPromoView::AddedToWidget() {
  GetBubbleFrameView()->SetProperty(views::kElementIdentifierKey,
                                    kBubbleFrameViewId);
  GetBubbleFrameView()->SetHeaderView(
      std::make_unique<ThemeTrackingNonAccessibleImageView>(
          ui::ImageModel::FromResourceId(IDR_SAVE_ADDRESS),
          ui::ImageModel::FromResourceId(IDR_SAVE_ADDRESS_DARK),
          base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                              base::Unretained(this))));
}

void AddressSignInPromoView::Hide() {
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void AddressSignInPromoView::WindowClosing() {
  AddressBubblesController::FromWebContents(web_contents_)->OnBubbleClosed();
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AddressSignInPromoView,
                                      kBubbleFrameViewId);

}  // namespace autofill
