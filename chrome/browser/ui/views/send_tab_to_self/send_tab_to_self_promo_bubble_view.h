// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace send_tab_to_self {

// Shown when the user is signed out, offering a promotional sign-in flow.
class SendTabToSelfSignInPromoBubbleView
    : public LocationBarBubbleDelegateView {
  METADATA_HEADER(SendTabToSelfSignInPromoBubbleView,
                  LocationBarBubbleDelegateView)

 public:
  enum class PromoMode {
    kSignIn,
    kReauth,
  };

  SendTabToSelfSignInPromoBubbleView(views::BubbleAnchor anchor,
                                     content::WebContents* web_contents,
                                     PromoMode promo_mode);
  SendTabToSelfSignInPromoBubbleView(
      const SendTabToSelfSignInPromoBubbleView&) = delete;
  SendTabToSelfSignInPromoBubbleView& operator=(
      const SendTabToSelfSignInPromoBubbleView&) = delete;
  ~SendTabToSelfSignInPromoBubbleView() override;

  // views::View:
  // Sets up the header illustration.
  void AddedToWidget() override;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_PROMO_BUBBLE_VIEW_H_
