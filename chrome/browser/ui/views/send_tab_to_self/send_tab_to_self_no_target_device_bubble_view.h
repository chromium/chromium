// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_BUBBLE_VIEW_H_

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace send_tab_to_self {

// Shown when the user is signed in but has no other active target devices.
class SendTabToSelfNoTargetDeviceBubbleView : public SendTabToSelfBubbleView {
  METADATA_HEADER(SendTabToSelfNoTargetDeviceBubbleView,
                  SendTabToSelfBubbleView)

 public:
  SendTabToSelfNoTargetDeviceBubbleView(views::BubbleAnchor anchor,
                                        content::WebContents* web_contents);
  SendTabToSelfNoTargetDeviceBubbleView(
      const SendTabToSelfNoTargetDeviceBubbleView&) = delete;
  SendTabToSelfNoTargetDeviceBubbleView& operator=(
      const SendTabToSelfNoTargetDeviceBubbleView&) = delete;
  ~SendTabToSelfNoTargetDeviceBubbleView() override;

 private:
  // Private helper to construct the view hierarchy.
  void InitLayout();
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_BUBBLE_VIEW_H_
