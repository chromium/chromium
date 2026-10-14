// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_promo_bubble_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_delegate.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#endif

namespace send_tab_to_self {

SendTabToSelfSignInPromoBubbleView::SendTabToSelfSignInPromoBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    PromoMode promo_mode)
    : SendTabToSelfBubbleView(anchor, web_contents) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  set_margins(BubbleSignInPromoView::GetBubbleSigninPromoMargins());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetTitle(promo_mode == PromoMode::kReauth
               ? IDS_SEND_TAB_TO_SELF_VERIFY_ITS_YOU_PROMO_TITLE
               : IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE);

  // Use the shared, callback-based promo delegate to resume the STTS flow
  // automatically after sign-in.
  auto delegate = std::make_unique<DefaultBubbleSignInPromoDelegate>(
      *web_contents, signin_metrics::AccessPoint::kSendTabToSelfPromo,
      base::BindOnce(
          &send_tab_to_self::SendTabToSelfBubbleController::ShowBubble,
          controller_,
          controller_->entry_point().value_or(ShareEntryPoint::kShareSheet),
          /*show_back_button=*/false));
  auto* sign_in_promo = AddChildView(std::make_unique<BubbleSignInPromoView>(
      web_contents, signin_metrics::AccessPoint::kSendTabToSelfPromo,
      std::move(delegate)));

  SetInitiallyFocusedView(sign_in_promo->GetSignInButton());
#else
  NOTREACHED() << "The promo bubble shouldn't show if dice-support is disabled";
#endif
}

SendTabToSelfSignInPromoBubbleView::~SendTabToSelfSignInPromoBubbleView() =
    default;

void SendTabToSelfSignInPromoBubbleView::AddedToWidget() {
  SendTabToSelfBubbleView::AddedToWidget();

  auto& bundle = ui::ResourceBundle::GetSharedInstance();
  // Purely visual illustration; main label handles accessibility.
  auto header_view = std::make_unique<views::ImageView>(
      bundle.GetThemedLottieImageNamed(IDR_INSTANT_HANDOFF_ILLUSTRATION));
  header_view->GetViewAccessibility().SetIsInvisible(true);

  gfx::Size preferred_size = header_view->GetPreferredSize();
  if (preferred_size.width()) {
    const float scale =
        static_cast<float>(ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_BUBBLE_PREFERRED_WIDTH)) /
        preferred_size.width();
    preferred_size = gfx::ScaleToRoundedSize(preferred_size, scale);
    header_view->SetImageSize(preferred_size);
  }
  GetBubbleFrameView()->SetHeaderView(std::move(header_view));
}

BEGIN_METADATA(SendTabToSelfSignInPromoBubbleView)
END_METADATA

}  // namespace send_tab_to_self
