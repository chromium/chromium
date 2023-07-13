// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_H_

#include "ui/base/interaction/element_identifier.h"

class CookieControlsContentView;

namespace gfx {
class Image;
}

namespace views {
class View;
}

// Bubble view used to display the user bypass ui. This bubble view is
// controlled by the CookieControlsBubbleViewController and contains a header
// and a content views.
class CookieControlsBubbleView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCookieControlsBubble);

  virtual ~CookieControlsBubbleView() = default;

  virtual void InitContentView(
      std::unique_ptr<CookieControlsContentView> view) = 0;
  virtual void InitReloadingView(std::unique_ptr<views::View> view) = 0;

  virtual void UpdateTitle(const std::u16string& title) = 0;
  virtual void UpdateSubtitle(const std::u16string& subtitle) = 0;
  virtual void UpdateFaviconImage(const gfx::Image& image,
                                  int favicon_view_id) = 0;

  virtual void ShowContentView() = 0;
  virtual void ShowReloadingView() = 0;

  virtual CookieControlsContentView* GetContentView() = 0;
  virtual views::View* GetReloadingView() = 0;

  virtual void CloseWidget() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_H_
