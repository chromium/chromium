// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/address_sign_in_promo_view.h"

#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#include "chrome/grit/browser_resources.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

AddressSignInPromoView::AddressSignInPromoView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    const AutofillProfile& autofill_profile,
    base::OnceClosure on_bubble_closed_callback)
    : AddressBubbleBaseView(anchor, web_contents),
      on_bubble_closed_callback_(std::move(on_bubble_closed_callback)) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetTitle(IDS_AUTOFILL_SIGNIN_PROMO_TITLE_ADDRESS);
  SetShowCloseButton(true);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_margins(BubbleSignInPromoView::GetBubbleSigninPromoMargins());

  // Show the sign in promo.
  auto* sign_in_promo = AddChildView(std::make_unique<BubbleSignInPromoView>(
      web_contents, signin_metrics::AccessPoint::kAddressBubble,
      syncer::LocalDataItemModel::DataId(autofill_profile.guid())));

  SetInitiallyFocusedView(sign_in_promo->GetSignInButton());
}

AddressSignInPromoView::~AddressSignInPromoView() = default;

void AddressSignInPromoView::AddedToWidget() {
  GetBubbleFrameView()->SetProperty(views::kElementIdentifierKey,
                                    kBubbleFrameViewId);

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<views::ImageView>(
      bundle.GetThemedLottieImageNamed(IDR_AUTOFILL_SAVE_ADDRESS_LOTTIE));
  image_view->GetViewAccessibility().SetIsInvisible(true);

  GetBubbleFrameView()->SetHeaderView(std::move(image_view));
}

void AddressSignInPromoView::Hide() {
  CloseBubble();

  // `WindowClosing()` runs asynchronously after the close task posted by
  // `CloseBubble()` completes, so notify the controller synchronously here to
  // clear its reference immediately without invoking the callback twice.
  if (on_bubble_closed_callback_) {
    std::move(on_bubble_closed_callback_).Run();
  }
}

void AddressSignInPromoView::WindowClosing() {
  if (on_bubble_closed_callback_) {
    std::move(on_bubble_closed_callback_).Run();
  }
}

BEGIN_METADATA(AddressSignInPromoView)
END_METADATA

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(AddressSignInPromoView,
                                      kBubbleFrameViewId);

}  // namespace autofill
