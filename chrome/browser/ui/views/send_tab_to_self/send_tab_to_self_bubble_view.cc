// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace send_tab_to_self {

SendTabToSelfBubbleView::SendTabToSelfBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents)
    : LocationBarBubbleDelegateView(anchor, web_contents),
      controller_(
          SendTabToSelfBubbleController::GetOrCreateForWebContents(web_contents)
              ->AsWeakPtr()) {
  CHECK(controller_, base::NotFatalUntil::M162);
  SetShowCloseButton(true);
  SetTitle(IDS_SEND_TAB_TO_SELF);
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

SendTabToSelfBubbleView::~SendTabToSelfBubbleView() = default;

void SendTabToSelfBubbleView::Hide() {
  CloseBubble();
}

BEGIN_METADATA(SendTabToSelfBubbleView)
END_METADATA

}  // namespace send_tab_to_self
