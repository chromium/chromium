// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;

namespace send_tab_to_self {

class SendTabToSelfEntry;

class SendTabToSelfToolbarBubbleController {
 public:
  explicit SendTabToSelfToolbarBubbleController(Browser* browser);

  void ShowBubble(const SendTabToSelfEntry& entry, views::View* anchor_view);
  void HideBubble();

  bool IsBubbleShowing() const;

  SendTabToSelfToolbarBubbleView* bubble() {
    return static_cast<SendTabToSelfToolbarBubbleView*>(bubble_tracker_.view());
  }

 private:
  views::ViewTracker bubble_tracker_;
  raw_ptr<const Browser> browser_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_CONTROLLER_H_
