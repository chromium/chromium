// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/promos/bubble_signin_promo_dialog_view.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_delegate.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

BubbleSignInPromoDialogView::BubbleSignInPromoDialogView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    signin_metrics::AccessPoint access_point,
    std::unique_ptr<BubbleSignInPromoDelegate> delegate,
    int signin_title_resource_id,
    std::optional<int> verify_title_resource_id,
    std::optional<int> lottie_resource_id)
    : LocationBarBubbleDelegateView(anchor, web_contents),
      lottie_resource_id_(lottie_resource_id) {
  CHECK(web_contents);
  CHECK(delegate);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetOriginalProfile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  const signin_util::SignedInState signed_in_state =
      signin_util::GetSignedInState(identity_manager);
  // `kSyncPaused` is only reachable from `SendTabToSelfSignInPromoBubbleView`
  // (for legacy syncing users in a persistent auth error state); other
  // `BubbleSignInPromoDialogView` subclasses do not trigger when sync is
  // paused.
  const bool is_verify_state =
      signed_in_state == signin_util::SignedInState::kSignInPending ||
      signed_in_state == signin_util::SignedInState::kSyncPaused;

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(true);
  SetTitle(is_verify_state
               ? verify_title_resource_id.value_or(signin_title_resource_id)
               : signin_title_resource_id);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_margins(BubbleSignInPromoView::GetBubbleSigninPromoMargins());

  auto* sign_in_promo = AddChildView(std::make_unique<BubbleSignInPromoView>(
      web_contents, access_point, std::move(delegate)));
  SetInitiallyFocusedView(sign_in_promo->GetSignInButton());
}

BubbleSignInPromoDialogView::~BubbleSignInPromoDialogView() = default;

void BubbleSignInPromoDialogView::AddedToWidget() {
  LocationBarBubbleDelegateView::AddedToWidget();
  if (!lottie_resource_id_.has_value()) {
    return;
  }

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto header_view = std::make_unique<views::ImageView>(
      bundle.GetThemedLottieImageNamed(*lottie_resource_id_));
  header_view->GetViewAccessibility().SetIsInvisible(true);

  gfx::Size preferred_size = header_view->GetPreferredSize();
  if (preferred_size.width()) {
    const float scale =
        static_cast<float>(views::LayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_BUBBLE_PREFERRED_WIDTH)) /
        preferred_size.width();
    preferred_size = gfx::ScaleToRoundedSize(preferred_size, scale);
    header_view->SetImageSize(preferred_size);
  }
  GetBubbleFrameView()->SetHeaderView(std::move(header_view));
}

BEGIN_METADATA(BubbleSignInPromoDialogView)
END_METADATA
