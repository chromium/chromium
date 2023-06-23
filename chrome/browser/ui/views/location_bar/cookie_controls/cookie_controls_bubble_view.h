// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "content/public/browser/web_contents.h"

namespace content {
class WebContents;
}

// Bubble view used to display the user bypass ui. This bubble view is
// controlled by the CookieControlsBubbleViewController and contains a header
// and a content views.
class CookieControlsBubbleView : public LocationBarBubbleDelegateView {
 public:
  CookieControlsBubbleView(views::View* anchor_view,
                           content::WebContents* web_contents);

  ~CookieControlsBubbleView() override;

 private:
  // LocationBarBubbleDelegateView:
  std::u16string GetWindowTitle() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_H_
