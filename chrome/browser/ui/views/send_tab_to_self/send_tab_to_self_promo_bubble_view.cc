// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_promo_bubble_view.h"

#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

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
  if (IsEnhancedUiEnabled()) {
    InitEnhancedLayout(promo_mode);
    return;
  }
#endif
  CHECK(promo_mode == PromoMode::kSignIn);
  InitBasicLayout();
}

SendTabToSelfSignInPromoBubbleView::~SendTabToSelfSignInPromoBubbleView() =
    default;

void SendTabToSelfSignInPromoBubbleView::InitBasicLayout() {
  set_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));

  // Configure body text label.
  auto* label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN));
  // base::Unretained() is safe here because this outlives the button.
  SetAcceptCallback(base::BindOnce(
      &SendTabToSelfSignInPromoBubbleView::HandleSignInButtonClicked,
      base::Unretained(this)));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void SendTabToSelfSignInPromoBubbleView::InitEnhancedLayout(
    PromoMode promo_mode) {
  set_margins(BubbleSignInPromoView::GetBubbleSigninPromoMargins());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetTitle(promo_mode == PromoMode::kReauth
               ? IDS_SEND_TAB_TO_SELF_VERIFY_ITS_YOU_PROMO_TITLE
               : IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE);

  // Use the shared, callback-based promo delegate to resume the STTS flow
  // automatically after sign-in.
  auto delegate = std::make_unique<DefaultBubbleSignInPromoDelegate>(
      *web_contents(), signin_metrics::AccessPoint::kSendTabToSelfPromo,
      base::BindOnce(
          &send_tab_to_self::SendTabToSelfBubbleController::ShowBubble,
          controller_,
          controller_->entry_point().value_or(ShareEntryPoint::kShareSheet),
          /*show_back_button=*/false));
  auto* sign_in_promo = AddChildView(std::make_unique<BubbleSignInPromoView>(
      web_contents(), signin_metrics::AccessPoint::kSendTabToSelfPromo,
      std::move(delegate)));

  SetInitiallyFocusedView(sign_in_promo->GetSignInButton());
}
#endif

void SendTabToSelfSignInPromoBubbleView::AddedToWidget() {
  SendTabToSelfBubbleView::AddedToWidget();
  if (!IsEnhancedUiEnabled()) {
    return;
  }

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

bool SendTabToSelfSignInPromoBubbleView::IsEnhancedUiEnabled() const {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  return base::FeatureList::IsEnabled(kSendTabToSelfEnhancedDesktopUI);
#else
  return false;
#endif
}

views::View* SendTabToSelfSignInPromoBubbleView::GetInitiallyFocusedView() {
  if (HasConfiguredInitiallyFocusedView()) {
    return views::BubbleDialogDelegateView::GetInitiallyFocusedView();
  }
  return nullptr;
}

void SendTabToSelfSignInPromoBubbleView::HandleSignInButtonClicked() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  SigninViewController::From(
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          web_contents()))
      ->ShowDiceAddAccountTab(signin_metrics::AccessPoint::kSendTabToSelfPromo,
                              /*email_hint=*/std::string());
#else
  NOTREACHED() << "The promo bubble shouldn't show if dice-support is disabled";
#endif
}

BEGIN_METADATA(SendTabToSelfSignInPromoBubbleView)
END_METADATA

}  // namespace send_tab_to_self
