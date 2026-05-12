// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace send_tab_to_self {

SendTabToSelfBubbleView::SendTabToSelfBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents)
    : LocationBarBubbleDelegateView(anchor, web_contents),
      controller_(SendTabToSelfBubbleController::CreateOrGetFromWebContents(
                      web_contents)
                      ->AsWeakPtr()) {
  DCHECK(controller_);
  SetShowCloseButton(true);
  SetTitle(IDS_SEND_TAB_TO_SELF);
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

SendTabToSelfBubbleView::~SendTabToSelfBubbleView() = default;

void SendTabToSelfBubbleView::Hide() {
  CloseBubble();
}

void SendTabToSelfBubbleView::AddedToWidget() {
  if (!controller_ || !controller_->show_back_button()) {
    return;
  }

  // Adding a title view will replace the default title.
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<sharing_hub::TitleWithBackButtonView>(
          base::BindRepeating(&SendTabToSelfBubbleView::BackButtonPressed,
                              base::Unretained(this)),
          GetWindowTitle()));
}

void SendTabToSelfBubbleView::BackButtonPressed() {
  if (controller_) {
    controller_->OnBackButtonPressed();
    Hide();
  }
}

BEGIN_METADATA(SendTabToSelfBubbleView)
END_METADATA

}  // namespace send_tab_to_self
