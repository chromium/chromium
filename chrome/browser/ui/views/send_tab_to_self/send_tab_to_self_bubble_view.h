// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace send_tab_to_self {

class SendTabToSelfBubbleController;

// The cross-platform UI interface which displays the share bubble.
// This object is responsible for its own lifetime.
class SendTabToSelfBubbleView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(SendTabToSelfBubbleView, LocationBarBubbleDelegateView)

 public:
  ~SendTabToSelfBubbleView() override;

  // Called to close the bubble and prevent future callbacks into the
  // controller.
  void Hide();

  // views::BubbleDialogDelegateView:
  void AddedToWidget() override;

 protected:
  SendTabToSelfBubbleView(views::BubbleAnchor anchor,
                          content::WebContents* web_contents);

  void BackButtonPressed();

  base::WeakPtr<SendTabToSelfBubbleController> controller_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_VIEW_H_
