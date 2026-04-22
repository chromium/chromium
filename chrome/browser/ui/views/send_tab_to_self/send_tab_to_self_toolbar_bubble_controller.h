// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace send_tab_to_self {

class SendTabToSelfEntry;
class SendTabToSelfBubbleView;

class SendTabToSelfToolbarBubbleController {
 public:
  explicit SendTabToSelfToolbarBubbleController(BrowserWindowInterface* bwi);
  ~SendTabToSelfToolbarBubbleController();

  DECLARE_USER_DATA(SendTabToSelfToolbarBubbleController);

  static SendTabToSelfToolbarBubbleController* From(
      BrowserWindowInterface* bwi);

  void ShowBubble(const SendTabToSelfEntry& entry, views::BubbleAnchor anchor);
  void HideBubble();

  // Shows the "send tab to self" device picker bubble. This must only be called
  // as a direct result of user action.
  SendTabToSelfBubbleView* ShowDevicePickerBubble(
      content::WebContents* web_contents);

  // Shows the "send tab to self" promo bubble. This must only be called as a
  // direct result of user action.
  SendTabToSelfBubbleView* ShowPromoBubble(content::WebContents* web_contents,
                                           bool show_signin_button);

  bool IsBubbleShowing() const;

  SendTabToSelfToolbarBubbleView* bubble() {
    return static_cast<SendTabToSelfToolbarBubbleView*>(bubble_tracker_.view());
  }

 private:
  friend class ui::ScopedUnownedUserData<SendTabToSelfToolbarBubbleController>;

  views::ViewTracker bubble_tracker_;
  const raw_ref<BrowserWindowInterface> bwi_;
  ::ui::ScopedUnownedUserData<SendTabToSelfToolbarBubbleController>
      scoped_unowned_user_data_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TOOLBAR_BUBBLE_CONTROLLER_H_
