// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_NEW_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_NEW_BUBBLE_VIEW_H_

#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"

// The experimental new implementation of the Views page info UI (under a flag
// PageInfoV2Desktop). Current implementation (PageInfoBubbleView) will be
// deprecated when the redesign is finished.
class PageInfoNewBubbleView : public PageInfoBubbleViewBase {
 public:
  PageInfoNewBubbleView(views::View* anchor_view,
                        const gfx::Rect& anchor_rect,
                        gfx::NativeView parent_window,
                        Profile* profile,
                        content::WebContents* web_contents,
                        const GURL& url,
                        PageInfoClosingCallback closing_callback);

  ~PageInfoNewBubbleView() override;

 private:
  // PageInfoBubbleViewBase:
  gfx::Size CalculatePreferredSize() const override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void WebContentsDestroyed() override;
  void ChildPreferredSizeChanged(views::View* child) override;

  views::View* page_container_ = nullptr;

  // The presenter that controls the Page Info UI.
  std::unique_ptr<PageInfo> presenter_;

  PageInfoClosingCallback closing_callback_;

  std::unique_ptr<PageInfoUiDelegate> ui_delegate_;

  base::WeakPtrFactory<PageInfoNewBubbleView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_NEW_BUBBLE_VIEW_H_
