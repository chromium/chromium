// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_ai_mode/signin_promo_view.h"

#include <optional>

#include "chrome/browser/ui/signin/promos/bubble_signin_promo_delegate.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kSearchAIModeSignInPromoFrameViewId);

SearchAIModeSignInPromoView::SearchAIModeSignInPromoView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    base::WeakPtr<SearchAIModeSignInPromoController> controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(std::move(controller)) {
  CHECK(web_contents);
  CHECK(base::FeatureList::IsEnabled(switches::kEnableSearchAIModeSigninPromo));

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
    controller_->OnBubbleClosed();
  }
}

void SearchAIModeSignInPromoView::AddedToWidget() {
  GetBubbleFrameView()->SetProperty(views::kElementIdentifierKey,
                                    kSearchAIModeSignInPromoFrameViewId);
  // TODO(crbug.com/486858498): Add the dialog's image here once available.
}

// TODO(crbug.com/486858498): Implement self-dismissal logic after X seconds.

BEGIN_METADATA(SearchAIModeSignInPromoView)
END_METADATA
