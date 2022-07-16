// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_BUBBLE_VIEW_H_

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace content {
class WebContents;
}  // namespace content

// A bubble that appears near the omnibar when the user clicks a
// |WebAuthnIconView|, instructing the user that they can insert and tap their
// security key to log-in to the website.
class WebAuthnBubbleView : public LocationBarBubbleDelegateView {
 public:
  // Creates a WebAuthnBubbleView owned by its widget.
  static WebAuthnBubbleView* Create(
      const std::string& relying_party_id,
      content::WebContents* web_contents);

  WebAuthnBubbleView(const std::string& relying_party_id,
                     views::View* anchor_view,
                     content::WebContents* web_contents);
  ~WebAuthnBubbleView() override;
  WebAuthnBubbleView(const WebAuthnBubbleView& other) = delete;
  WebAuthnBubbleView& operator=(const WebAuthnBubbleView& other) = delete;

  // LocationBarBubbleDelegateView:
  std::u16string GetWindowTitle() const override;
  void Init() override;

 private:
  std::string relying_party_id_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_WEBAUTHN_BUBBLE_VIEW_H_
