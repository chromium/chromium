// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_ai_mode/signin_promo_view.h"

#include <optional>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_delegate.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr base::TimeDelta kPromoSelfDismissalTimeout = base::Seconds(15);
}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kSearchAIModeSignInPromoFrameViewId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSearchAIModeSignInPromoViewId);

SearchAIModeSignInPromoView::SearchAIModeSignInPromoView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    base::WeakPtr<SearchAIModeSignInPromoController> controller)
    : LocationBarBubbleDelegateView(anchor, web_contents),
      controller_(std::move(controller)) {
  CHECK(web_contents);
  CHECK(base::FeatureList::IsEnabled(switches::kEnableSearchAIModeSigninPromo));

  SetProperty(views::kElementIdentifierKey, kSearchAIModeSignInPromoViewId);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetTitle(IDS_AI_SIGNIN_PROMO_TITLE);
  SetShowCloseButton(true);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_margins(BubbleSignInPromoView::GetBubbleSigninPromoMargins());

  auto* sign_in_promo = AddChildView(std::make_unique<BubbleSignInPromoView>(
      web_contents, signin_metrics::AccessPoint::kSearchAIModeBubble,
      /*data_id=*/std::nullopt));
  SetInitiallyFocusedView(sign_in_promo->GetSignInButton());
}

SearchAIModeSignInPromoView::~SearchAIModeSignInPromoView() {
  if (controller_) {
    controller_->OnViewIsDeleting();
  }
}

void SearchAIModeSignInPromoView::FireTimerForTesting() {
  CHECK_IS_TEST();
  self_dismissal_timer_.FireNow();
}

bool SearchAIModeSignInPromoView::IsTimerRunningForTesting() const {
  CHECK_IS_TEST();
  return self_dismissal_timer_.IsRunning();
}

void SearchAIModeSignInPromoView::WindowClosing() {
  if (controller_) {
    controller_->HandlePromoClosing(GetWidget()->closed_reason());
  }
}

void SearchAIModeSignInPromoView::AddedToWidget() {
  GetBubbleFrameView()->SetProperty(views::kElementIdentifierKey,
                                    kSearchAIModeSignInPromoFrameViewId);

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<views::ImageView>(
      bundle.GetThemedLottieImageNamed(IDR_SEARCH_AI_MODE_SIGNIN_PROMO_LOTTIE));
  image_view->GetViewAccessibility().SetIsInvisible(true);

  GetBubbleFrameView()->SetHeaderView(std::move(image_view));

  if (base::FeatureList::IsEnabled(
          switches::kSearchAIModeSignInPromoSelfDismissal)) {
    self_dismissal_timer_.Start(
        FROM_HERE, kPromoSelfDismissalTimeout,
        base::BindOnce(&SearchAIModeSignInPromoView::Close,
                       // Unretained is fine because the timer is owned by this
                       // object.
                       base::Unretained(this)));
  }
}

void SearchAIModeSignInPromoView::Close() {
  if (GetWidget()) {
    GetWidget()->Close();
  }
}

BEGIN_METADATA(SearchAIModeSignInPromoView)
END_METADATA
