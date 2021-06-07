// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ACCURACY_TIP_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ACCURACY_TIP_BUBBLE_VIEW_H_

#include "base/callback.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/accuracy_tips/accuracy_tip_ui.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class View;
class Widget;
}  // namespace views

// When Chrome displays an accuracy tip, we create a bubble view anchored to the
// lock icon. Accuracy tip info is also displayed in the usual
// PageInfoBubbleView, just less prominently.
class AccuracyTipBubbleView : public PageInfoBubbleViewBase {
 public:
  using AccuracyTipUI = accuracy_tips::AccuracyTipUI;

  // If |anchor_view| is nullptr, or has no Widget, |parent_window| may be
  // provided to ensure this bubble is closed when the parent closes.
  //
  // |close_callback| will be called when the bubble is destroyed. The argument
  // indicates what action (if any) the user took to close the bubble.
  AccuracyTipBubbleView(
      views::View* anchor_view,
      const gfx::Rect& anchor_rect,
      gfx::NativeView parent_window,
      content::WebContents* web_contents,
      accuracy_tips::AccuracyTipStatus status,
      base::OnceCallback<void(AccuracyTipUI::Interaction)> close_callback);
  ~AccuracyTipBubbleView() override;

  AccuracyTipBubbleView(const AccuracyTipBubbleView&) = delete;
  AccuracyTipBubbleView& operator=(const AccuracyTipBubbleView&) = delete;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void OpenHelpCenter();

  // WebContentsObserver:
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void DidChangeVisibleSecurityState() override;

  base::OnceCallback<void(AccuracyTipUI::Interaction)> close_callback_;
  AccuracyTipUI::Interaction action_taken_ =
      AccuracyTipUI::Interaction::kNoAction;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ACCURACY_TIP_BUBBLE_VIEW_H_
