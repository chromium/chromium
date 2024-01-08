// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_VIEW_H_

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

namespace send_tab_to_self {

// The cross-platform UI interface which displays the share bubble.
// This object is responsible for its own lifetime.
class SendTabToSelfBubbleView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(SendTabToSelfBubbleView, LocationBarBubbleDelegateView)

 public:
  ~SendTabToSelfBubbleView() override = default;

  // Called to close the bubble and prevent future callbacks into the
  // controller.
  virtual void Hide() = 0;

 protected:
  SendTabToSelfBubbleView(views::View* anchor_view,
                          content::WebContents* web_contents);
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_VIEW_H_
