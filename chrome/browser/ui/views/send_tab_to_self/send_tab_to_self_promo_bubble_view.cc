// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_promo_bubble_view.h"

#include <string>

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/manage_account_devices_link_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_util.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/features.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

int GetLabelStringId(
    send_tab_to_self::SendTabToSelfPromoBubbleView::PromoType promo_type,
    bool is_enhanced_ui) {
  switch (promo_type) {
    case send_tab_to_self::SendTabToSelfPromoBubbleView::PromoType::
        kSignInPromo:
    case send_tab_to_self::SendTabToSelfPromoBubbleView::PromoType::
        kAccountAwareSignInPromo:
      return is_enhanced_ui ? IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_BODY
                            : IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_LABEL;
    case send_tab_to_self::SendTabToSelfPromoBubbleView::PromoType::
        kNoTargetDevice:
      return IDS_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_LABEL;
  }
}

int GetButtonStringId(bool is_enhanced_ui) {
  return is_enhanced_ui ? IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_BUTTON_LABEL
                        : IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN;
}

int GetLabelPadding(views::LayoutProvider* provider, bool is_enhanced_ui) {
  return is_enhanced_ui ? provider->GetInsetsMetric(views::INSETS_DIALOG).left()
                        : provider->GetDistanceMetric(
                              views::DISTANCE_BUTTON_HORIZONTAL_PADDING);
}

}  // namespace

namespace send_tab_to_self {

SendTabToSelfPromoBubbleView::SendTabToSelfPromoBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    PromoType promo_type)
    : SendTabToSelfBubbleView(anchor, web_contents), promo_type_(promo_type) {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(
      gfx::Insets::TLBR(provider->GetDistanceMetric(
                            views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                        0, 0, 0));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  InitLayout();
}

void SendTabToSelfPromoBubbleView::InitLayout() {
  auto* provider = ChromeLayoutProvider::Get();
  const bool is_enhanced_ui =
      base::FeatureList::IsEnabled(kSendTabToSelfEnhancedDesktopUI);
  const bool show_signin_button =
      promo_type_ == PromoType::kSignInPromo ||
      promo_type_ == PromoType::kAccountAwareSignInPromo;

  // Configure title. Only shown for the sign-in promo case in enhanced UI.
  if (show_signin_button && is_enhanced_ui) {
    SetTitle(IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE);
  }

  // Configure body text label.
  auto* label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(GetLabelStringId(promo_type_, is_enhanced_ui)),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  // Add vertical separation below the text before the action button row
  // begins, ensuring the body doesn't run directly into the button.
  int bottom_margin = 0;
  if (is_enhanced_ui && show_signin_button) {
    bottom_margin = provider->GetDistanceMetric(
        views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT);
  }
  const int horizontal_padding = GetLabelPadding(provider, is_enhanced_ui);
  label->SetProperty(views::kMarginsKey,
                     gfx::Insets::TLBR(0, horizontal_padding, bottom_margin,
                                       horizontal_padding));

  // Configure accept button.
  if (show_signin_button) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
    SetButtonLabel(
        ui::mojom::DialogButton::kOk,
        l10n_util::GetStringUTF16(GetButtonStringId(is_enhanced_ui)));
    // base::Unretained() is safe here because this outlives the button.
    SetAcceptCallback(base::BindRepeating(
        &SendTabToSelfPromoBubbleView::HandleSignInButtonClicked,
        base::Unretained(this)));
    return;
  }

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  auto* link_view = AddChildView(
      BuildManageAccountDevicesLinkView(/*show_link=*/false, controller_));
  link_view->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(provider->GetDistanceMetric(
                          views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
                      0));
}

void SendTabToSelfPromoBubbleView::AddedToWidget() {
  if (!base::FeatureList::IsEnabled(kSendTabToSelfEnhancedDesktopUI)) {
    return;
  }

  switch (promo_type_) {
    case PromoType::kSignInPromo: {
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
      break;
    }
    case PromoType::kAccountAwareSignInPromo:
      break;
    case PromoType::kNoTargetDevice:
      break;
  }

  // Customize the dialog's OK button for the modernized promo layout.
  views::LabelButton* ok_button = GetOkButton();
  if (ok_button) {
    // Restrict focus behavior to accessibility/keyboard navigation only.
    // This completely prevents focus rings from rendering on standard mouse
    // click.
    ok_button->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  }
}

views::View* SendTabToSelfPromoBubbleView::GetInitiallyFocusedView() {
  // Prevent focus rings from drawing on dialog display by returning nullptr,
  // ensuring no default button gets focused until user keyboard interaction.
  return nullptr;
}

void SendTabToSelfPromoBubbleView::HandleSignInButtonClicked() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  GlobalBrowserCollection::GetInstance()
      ->FindBrowserWithTab(web_contents())
      ->GetFeatures()
      .signin_view_controller()
      ->ShowDiceAddAccountTab(signin_metrics::AccessPoint::kSendTabToSelfPromo,
                              /*email_hint=*/std::string());
#else
  NOTREACHED() << "The promo bubble shouldn't show if dice-support is disabled";
#endif
}

BEGIN_METADATA(SendTabToSelfPromoBubbleView)
END_METADATA

}  // namespace send_tab_to_self
