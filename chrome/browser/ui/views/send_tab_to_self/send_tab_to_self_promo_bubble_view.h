// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

namespace send_tab_to_self {

class SendTabToSelfBubbleController;

// View to promo the send-tab-to-self feature when it can't be used yet. There
// are 2 cases.
// a) User is signed out. The view will have a button to offer sign in.
// b) User is signed in but has no other signed-in device to share a tab to. The
// view will contain text explaining they can use the feature by signing in on
// another device.
class SendTabToSelfPromoBubbleView : public SendTabToSelfBubbleView {
 public:
  // Bubble will be anchored to |anchor_view|.
  SendTabToSelfPromoBubbleView(views::View* anchor_view,
                               content::WebContents* web_contents,
                               bool show_signin_button);

  ~SendTabToSelfPromoBubbleView() override;

  SendTabToSelfPromoBubbleView(const SendTabToSelfPromoBubbleView&) = delete;
  SendTabToSelfPromoBubbleView& operator=(const SendTabToSelfPromoBubbleView&) =
      delete;

  // SendTabToSelfBubbleView:
  void Hide() override;

  // views::BubbleDialogDelegateView:
  void AddedToWidget() override;

 private:
  void OnSignInButtonClicked();

  void OnBackButtonClicked();

  const base::WeakPtr<SendTabToSelfBubbleController> controller_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_
