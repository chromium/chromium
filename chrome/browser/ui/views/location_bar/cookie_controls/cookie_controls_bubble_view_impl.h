// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget.h"

class CookieControlsContentView;

namespace content {
class WebContents;
}

using OnCloseBubbleCallback = base::OnceCallback<void(views::View*)>;

class CookieControlsBubbleViewImpl : public CookieControlsBubbleView,
                                     public LocationBarBubbleDelegateView {
  METADATA_HEADER(CookieControlsBubbleViewImpl, LocationBarBubbleDelegateView)

 public:
  CookieControlsBubbleViewImpl(views::View* anchor_view,
                               content::WebContents* web_contents,
                               OnCloseBubbleCallback callback);
  ~CookieControlsBubbleViewImpl() override;

  // CookieControlsBubbleView:
  void InitContentView(
      std::unique_ptr<CookieControlsContentView> view) override;
  void InitReloadingView(std::unique_ptr<View> view) override;

  void UpdateTitle(const std::u16string& title) override;
  void UpdateSubtitle(const std::u16string& subtitle) override;
  void UpdateFaviconImage(const gfx::Image& image,
                          int favicon_view_id) override;

  void SwitchToReloadingView() override;

  CookieControlsContentView* GetContentView() override;
  View* GetReloadingView() override;

  void CloseWidget() override;

  base::CallbackListSubscription RegisterOnUserClosedContentViewCallback(
      base::RepeatingClosureList::CallbackType callback) override;

 protected:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  // LocationBarBubbleDelegateView:
  void Init() override;
  void CloseBubble() override;
  bool OnCloseRequested(views::Widget::ClosedReason close_reason) override;

  raw_ptr<View> reloading_view_ = nullptr;
  raw_ptr<CookieControlsContentView> content_view_ = nullptr;

  base::RepeatingClosureList on_user_closed_content_view_callback_list_;
  OnCloseBubbleCallback callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_COOKIE_CONTROLS_COOKIE_CONTROLS_BUBBLE_VIEW_IMPL_H_
