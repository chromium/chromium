// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_promo_bubble_view.h"

#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/manage_account_devices_link_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/features.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

int GetSignInLabelStringId(bool is_enhanced_ui) {
  return is_enhanced_ui ? IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_BODY
                        : IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_LABEL;
}

int GetSignInButtonStringId(bool is_enhanced_ui) {
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

SendTabToSelfNoTargetDeviceBubbleView::SendTabToSelfNoTargetDeviceBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents)
    : SendTabToSelfBubbleView(anchor, web_contents) {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(
      gfx::Insets::TLBR(provider->GetDistanceMetric(
                            views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                        0, 0, 0));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  InitLayout();
}

SendTabToSelfNoTargetDeviceBubbleView::
    ~SendTabToSelfNoTargetDeviceBubbleView() = default;

void SendTabToSelfNoTargetDeviceBubbleView::InitLayout() {
  auto* provider = ChromeLayoutProvider::Get();
  const bool is_enhanced_ui =
      base::FeatureList::IsEnabled(kSendTabToSelfEnhancedDesktopUI);

  // Configure body text label.
  auto* label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  const int horizontal_padding = GetLabelPadding(provider, is_enhanced_ui);
  label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, horizontal_padding, /*bottom=*/0,
                        horizontal_padding));

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  auto* link_view = AddChildView(
      BuildManageAccountDevicesLinkView(/*show_link=*/false, controller_));
  link_view->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(provider->GetDistanceMetric(
                          views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
                      0));
}

SendTabToSelfSignInPromoBubbleView::SendTabToSelfSignInPromoBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    bool is_account_aware)
    : SendTabToSelfBubbleView(anchor, web_contents),
      is_account_aware_(is_account_aware) {
  auto* provider = ChromeLayoutProvider::Get();
  set_margins(
      gfx::Insets::TLBR(provider->GetDistanceMetric(
                            views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                        0, 0, 0));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  InitLayout();
}

SendTabToSelfSignInPromoBubbleView::~SendTabToSelfSignInPromoBubbleView() =
    default;

void SendTabToSelfSignInPromoBubbleView::InitLayout() {
  auto* provider = ChromeLayoutProvider::Get();
  const bool is_enhanced_ui =
      base::FeatureList::IsEnabled(kSendTabToSelfEnhancedDesktopUI);

  // Configure title. Only shown in enhanced UI.
  if (is_enhanced_ui) {
    SetTitle(IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE);
  }

  // Configure body text label.
  auto* label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(GetSignInLabelStringId(is_enhanced_ui)),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  int bottom_margin = 0;
  if (is_enhanced_ui) {
    bottom_margin = provider->GetDistanceMetric(
        views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT);
  }
  const int horizontal_padding = GetLabelPadding(provider, is_enhanced_ui);
  label->SetProperty(views::kMarginsKey,
                     gfx::Insets::TLBR(0, horizontal_padding, bottom_margin,
                                       horizontal_padding));

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(GetSignInButtonStringId(is_enhanced_ui)));
  // base::Unretained() is safe here because this outlives the button.
  SetAcceptCallback(base::BindOnce(
      &SendTabToSelfSignInPromoBubbleView::HandleSignInButtonClicked,
      base::Unretained(this)));
}

void SendTabToSelfSignInPromoBubbleView::AddedToWidget() {
  if (!base::FeatureList::IsEnabled(kSendTabToSelfEnhancedDesktopUI)) {
    return;
  }

  if (is_account_aware_) {
    // TODO(crbug.com/488252159): Implement the modernized signed-out case in
    // an account-aware state by showing the profile icon/avatar header.
  } else {
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

  // Customize the dialog's OK button for the modernized promo layout.
  views::LabelButton* ok_button = GetOkButton();
  if (ok_button) {
    // Restrict focus behavior to accessibility/keyboard navigation only.
    // This completely prevents focus rings from rendering on standard mouse
    // click.
    ok_button->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  }
}

views::View* SendTabToSelfSignInPromoBubbleView::GetInitiallyFocusedView() {
  // Prevent focus rings from drawing on dialog display by returning nullptr,
  // ensuring no default button gets focused until user keyboard interaction.
  return nullptr;
}

void SendTabToSelfSignInPromoBubbleView::HandleSignInButtonClicked() {
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

BEGIN_METADATA(SendTabToSelfNoTargetDeviceBubbleView)
END_METADATA

BEGIN_METADATA(SendTabToSelfSignInPromoBubbleView)
END_METADATA

}  // namespace send_tab_to_self
