// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_promo_bubble_view.h"

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/manage_account_devices_link_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace send_tab_to_self {

SendTabToSelfPromoBubbleView::SendTabToSelfPromoBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    bool show_signin_button)
    : SendTabToSelfBubbleView(anchor_view, web_contents),
      controller_(SendTabToSelfBubbleController::CreateOrGetFromWebContents(
                      web_contents)
                      ->AsWeakPtr()) {
  DCHECK(controller_);

  SetShowCloseButton(true);
  SetTitle(IDS_SEND_TAB_TO_SELF);

  auto* provider = ChromeLayoutProvider::Get();
  set_fixed_width(
      provider->GetDistanceMetric(views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_margins(
      gfx::Insets::TLBR(provider->GetDistanceMetric(
                            views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                        0, 0, 0));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  const std::u16string label_text = l10n_util::GetStringUTF16(
      show_signin_button ? IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_LABEL
                         : IDS_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_LABEL);
  auto* label = AddChildView(std::make_unique<views::Label>(
      label_text, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, provider->GetDistanceMetric(
                             views::DISTANCE_BUTTON_HORIZONTAL_PADDING)));

  if (show_signin_button) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
    SetButtonLabel(ui::mojom::DialogButton::kOk,
                   l10n_util::GetStringUTF16(
                       IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN));
    // base::Unretained() is safe here because this outlives the button.
    SetAcceptCallback(base::BindRepeating(
        &SendTabToSelfPromoBubbleView::OnSignInButtonClicked,
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

SendTabToSelfPromoBubbleView::~SendTabToSelfPromoBubbleView() {
  if (controller_)
    controller_->OnBubbleClosed();
}

void SendTabToSelfPromoBubbleView::Hide() {
  CloseBubble();
}

void SendTabToSelfPromoBubbleView::AddedToWidget() {
  if (!controller_->show_back_button())
    return;

  // Adding a title view will replace the default title.
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<sharing_hub::TitleWithBackButtonView>(
          base::BindRepeating(
              &SendTabToSelfPromoBubbleView::OnBackButtonClicked,
              base::Unretained(this)),
          GetWindowTitle()));
}

void SendTabToSelfPromoBubbleView::OnSignInButtonClicked() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  chrome::FindBrowserWithTab(web_contents())
      ->signin_view_controller()
      ->ShowDiceAddAccountTab(
          signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO,
          /*email_hint=*/std::string());
#else
  NOTREACHED() << "The promo bubble shouldn't show if dice-support is disabled";
#endif
}

void SendTabToSelfPromoBubbleView::OnBackButtonClicked() {
  if (controller_)
    controller_->OnBackButtonPressed();
  CloseBubble();
}

}  // namespace send_tab_to_self
