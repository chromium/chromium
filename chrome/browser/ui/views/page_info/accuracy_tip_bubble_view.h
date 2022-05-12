// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ACCURACY_TIP_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ACCURACY_TIP_BUBBLE_VIEW_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "components/accuracy_tips/accuracy_tip_interaction.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/permissions/permission_request_manager.h"

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
class AccuracyTipBubbleView
    : public PageInfoBubbleViewBase,
      public permissions::PermissionRequestManager::Observer {
 public:
  using AccuracyTipInteraction = accuracy_tips::AccuracyTipInteraction;

  // If |anchor_view| is nullptr, or has no Widget, |parent_window| may be
  // provided to ensure this bubble is closed when the parent closes.
  //
  // |close_callback| will be called when the bubble is destroyed. The argument
  // indicates what action (if any) the user took to close the bubble.
  // If |show_opt_out| is true, an opt-out button is shown. Otherwise an
  // "ignore" button.
  AccuracyTipBubbleView(
      views::View* anchor_view,
      const gfx::Rect& anchor_rect,
      gfx::NativeView parent_window,
      content::WebContents* web_contents,
      accuracy_tips::AccuracyTipStatus status,
      bool show_opt_out,
      base::OnceCallback<void(AccuracyTipInteraction)> close_callback);
  ~AccuracyTipBubbleView() override;

  AccuracyTipBubbleView(const AccuracyTipBubbleView&) = delete;
  AccuracyTipBubbleView& operator=(const AccuracyTipBubbleView&) = delete;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // permissions::PermissionRequestManager::Observer:
  void OnBubbleAdded() override;

 private:
  void OpenHelpCenter();
  void OnSecondaryButtonClicked(AccuracyTipInteraction action);

  // WebContentsObserver:
  void DidChangeVisibleSecurityState() override;

  base::OnceCallback<void(AccuracyTipInteraction)> close_callback_;
  AccuracyTipInteraction action_taken_ = AccuracyTipInteraction::kNoAction;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_ACCURACY_TIP_BUBBLE_VIEW_H_
