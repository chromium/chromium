// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_VIEW_H_

namespace send_tab_to_self {

// The cross-platform UI interface which displays the share bubble.
// This object is responsible for its own lifetime.
class SendTabToSelfBubbleView {
 public:
  virtual ~SendTabToSelfBubbleView() = default;

  // Called to close the bubble and prevent future callbacks into the
  // controller.
  virtual void Hide() = 0;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_VIEW_H_
