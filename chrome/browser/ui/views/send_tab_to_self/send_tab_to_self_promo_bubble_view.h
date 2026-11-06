// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_

#include "chrome/browser/ui/signin/promos/bubble_signin_promo_dialog_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace send_tab_to_self {

// Shown when the user is signed out or in a sign-in pending state, offering a
// promotional sign-in or reauth flow.
class SendTabToSelfSignInPromoBubbleView : public BubbleSignInPromoDialogView {
  METADATA_HEADER(SendTabToSelfSignInPromoBubbleView,
                  BubbleSignInPromoDialogView)

 public:
  SendTabToSelfSignInPromoBubbleView(views::BubbleAnchor anchor,
                                     content::WebContents* web_contents);
  SendTabToSelfSignInPromoBubbleView(
      const SendTabToSelfSignInPromoBubbleView&) = delete;
  SendTabToSelfSignInPromoBubbleView& operator=(
      const SendTabToSelfSignInPromoBubbleView&) = delete;
  ~SendTabToSelfSignInPromoBubbleView() override;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_
