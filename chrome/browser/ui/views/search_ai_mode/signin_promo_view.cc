// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_ai_mode/signin_promo_view.h"

#include <optional>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_delegate.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_controller.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr base::TimeDelta kPromoSelfDismissalTimeout = base::Seconds(15);

std::unique_ptr<views::ImageView> CreateHeaderImageView(int lottie_res_id) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<views::ImageView>(
      bundle.GetThemedLottieImageNamed(lottie_res_id));
  image_view->GetViewAccessibility().SetIsInvisible(true);
  return image_view;
}

}  // namespace

AIModeSignInPromoViewBase::AIModeSignInPromoViewBase(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    base::WeakPtr<AIModeSignInPromoControllerBase> controller,
    signin_metrics::AccessPoint access_point)
    : LocationBarBubbleDelegateView(anchor, web_contents),
      controller_(std::move(controller)) {
  CHECK(web_contents);

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(true);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_margins(BubbleSignInPromoView::GetBubbleSigninPromoMargins());

  auto* sign_in_promo = AddChildView(
      std::make_unique<BubbleSignInPromoView>(web_contents, access_point,
                                              /*data_id=*/std::nullopt));
  SetInitiallyFocusedView(sign_in_promo->GetSignInButton());
}

AIModeSignInPromoViewBase::~AIModeSignInPromoViewBase() {
  if (controller_) {
    controller_->OnViewIsDeleting();
  }
}

void AIModeSignInPromoViewBase::WindowClosing() {
  if (controller_) {
    controller_->HandlePromoClosing(GetWidget()->closed_reason());
  }
}

void AIModeSignInPromoViewBase::Close() {
  if (GetWidget()) {
    GetWidget()->Close();
  }
}

BEGIN_METADATA(AIModeSignInPromoViewBase)
END_METADATA

// SearchAIModeSignInPromoView -------------------------------------------------

DEFINE_ELEMENT_IDENTIFIER_VALUE(kSearchAIModeSignInPromoFrameViewId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSearchAIModeSignInPromoViewId);

SearchAIModeSignInPromoView::SearchAIModeSignInPromoView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    base::WeakPtr<SearchAIModeSignInPromoController> controller)
    : AIModeSignInPromoViewBase(
          anchor,
          web_contents,
          std::move(controller),
          signin_metrics::AccessPoint::kSearchAIModeBubble) {
  CHECK(base::FeatureList::IsEnabled(switches::kEnableSearchAIModeSigninPromo));
  SetProperty(views::kElementIdentifierKey, kSearchAIModeSignInPromoViewId);
  SetTitle(IDS_AI_SIGNIN_PROMO_TITLE);
}

SearchAIModeSignInPromoView::~SearchAIModeSignInPromoView() = default;

void SearchAIModeSignInPromoView::FireTimerForTesting() {
  CHECK_IS_TEST();
  self_dismissal_timer_.FireNow();
}

bool SearchAIModeSignInPromoView::IsTimerRunningForTesting() const {
  CHECK_IS_TEST();
  return self_dismissal_timer_.IsRunning();
}

void SearchAIModeSignInPromoView::AddedToWidget() {
  GetBubbleFrameView()->SetProperty(views::kElementIdentifierKey,
                                    kSearchAIModeSignInPromoFrameViewId);
  GetBubbleFrameView()->SetHeaderView(
      CreateHeaderImageView(IDR_SEARCH_AI_MODE_SIGNIN_PROMO_LOTTIE));

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

BEGIN_METADATA(SearchAIModeSignInPromoView)
END_METADATA

// ComposeboxDriveSignInPromoView ---------------------------------------------

DEFINE_ELEMENT_IDENTIFIER_VALUE(kComposeboxDriveSignInPromoFrameViewId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kComposeboxDriveSignInPromoViewId);

ComposeboxDriveSignInPromoView::ComposeboxDriveSignInPromoView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    base::WeakPtr<ComposeboxDriveSignInPromoController> controller)
    : AIModeSignInPromoViewBase(anchor,
                                web_contents,
                                std::move(controller),
                                signin_metrics::AccessPoint::
                                    kComposeboxDriveContextMenuOptionBubble) {
  CHECK(base::FeatureList::IsEnabled(
      omnibox::kComposeboxDriveContextMenuOptionSigninPromo));
  SetProperty(views::kElementIdentifierKey, kComposeboxDriveSignInPromoViewId);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetOriginalProfile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  signin_util::SignedInState signed_in_state =
      signin_util::GetSignedInState(identity_manager);

  SetTitle(signed_in_state == signin_util::SignedInState::kSignInPending
               ? IDS_COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION_VERIFY_PROMO_TITLE
               : IDS_COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION_SIGNIN_PROMO_TITLE);
}

ComposeboxDriveSignInPromoView::~ComposeboxDriveSignInPromoView() = default;

void ComposeboxDriveSignInPromoView::AddedToWidget() {
  GetBubbleFrameView()->SetProperty(views::kElementIdentifierKey,
                                    kComposeboxDriveSignInPromoFrameViewId);
  auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      ui::ImageModel::FromResourceId(IDR_COMPOSEBOX_DRIVE_SIGNIN_PROMO_LIGHT),
      ui::ImageModel::FromResourceId(IDR_COMPOSEBOX_DRIVE_SIGNIN_PROMO_DARK),
      base::BindRepeating(&views::BubbleDialogDelegate::background_color,
                          base::Unretained(this)));
  GetBubbleFrameView()->SetHeaderView(std::move(image_view));
}

BEGIN_METADATA(ComposeboxDriveSignInPromoView)
END_METADATA
