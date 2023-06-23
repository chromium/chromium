// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view.h"

#include <string>
#include "content/public/browser/web_contents.h"

CookieControlsBubbleView::CookieControlsBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents)
    : LocationBarBubbleDelegateView(anchor_view, web_contents) {
  SetShowTitle(true);
  SetShowCloseButton(true);
  SetButtons(ui::DIALOG_BUTTON_NONE);
}

CookieControlsBubbleView::~CookieControlsBubbleView() = default;

std::u16string CookieControlsBubbleView::GetWindowTitle() const {
  // TODO(crbug.com/1446230): use proper resource strings for title.
  return u"New Bubble title";
}
