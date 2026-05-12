// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

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
  METADATA_HEADER(SendTabToSelfPromoBubbleView, SendTabToSelfBubbleView)

 public:
  enum class PromoType {
    kSignInPromo,
    // TODO(crbug.com/488252159): Implement the modernized signed-out case in
    // an account-aware state by showing the profile icon/avatar header.
    kAccountAwareSignInPromo,
    kNoTargetDevice,
  };

  // Bubble will be anchored to `anchor`.
  SendTabToSelfPromoBubbleView(views::BubbleAnchor anchor,
                               content::WebContents* web_contents,
                               PromoType promo_type);

  SendTabToSelfPromoBubbleView(const SendTabToSelfPromoBubbleView&) = delete;
  SendTabToSelfPromoBubbleView& operator=(const SendTabToSelfPromoBubbleView&) =
      delete;

  // views::WidgetObserver:
  // Sets up header illustration or custom profile styling.
  void AddedToWidget() override;

  // views::BubbleDialogDelegateView:
  // Override to prevent the OK button from receiving initial focus on display.
  views::View* GetInitiallyFocusedView() override;

 private:
  // Private helper to construct the view hierarchy.
  void InitLayout();

  // Launches the Dice sign-in tab.
  void HandleSignInButtonClicked();

  // The mode/variant of this promo bubble.
  const PromoType promo_type_;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_
