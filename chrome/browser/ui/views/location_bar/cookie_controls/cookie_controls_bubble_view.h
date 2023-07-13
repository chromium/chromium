// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "content/public/browser/web_contents.h"

class CookieControlsContentView;

namespace content {
class WebContents;
}

using OnCloseBubbleCallback = base::OnceCallback<void(views::View*)>;

// Bubble view used to display the user bypass ui. This bubble view is
// controlled by the CookieControlsBubbleViewController and contains a header
// and a content views.
class CookieControlsBubbleView : public LocationBarBubbleDelegateView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCookieControlsBubble);
  CookieControlsBubbleView(views::View* anchor_view,
                           content::WebContents* web_contents,
                           OnCloseBubbleCallback callback);

  ~CookieControlsBubbleView() override;

  void InitContentView(std::unique_ptr<CookieControlsContentView> view);
  void InitReloadingView(std::unique_ptr<View> view);

  void UpdateTitle(const std::u16string& title);
  void UpdateSubtitle(const std::u16string& subtitle);
  void UpdateFaviconImage(const gfx::Image& image, int favicon_view_id);

  void ShowContentView();
  void ShowReloadingView();

  CookieControlsContentView* content_view() { return content_view_; }
  View* reloading_view() { return reloading_view_; }

 private:
  // LocationBarBubbleDelegateView:
  void Init() override;
  void CloseBubble() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  raw_ptr<View> reloading_view_ = nullptr;
  raw_ptr<CookieControlsContentView> content_view_ = nullptr;

  OnCloseBubbleCallback callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_H_
