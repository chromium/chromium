// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_SAFETY_TIP_PAGE_INFO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_SAFETY_TIP_PAGE_INFO_BUBBLE_VIEW_H_

#include "chrome/browser/reputation/safety_tip_ui.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/visibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label.h"

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

// When Chrome displays a safety tip, we create a stripped-down bubble view
// without all of the details. Safety tip info is still displayed in the usual
// PageInfoBubbleView, just less prominently.
class SafetyTipPageInfoBubbleView : public PageInfoBubbleViewBase,
                                    public views::ButtonListener {
 public:
  // If |anchor_view| is nullptr, or has no Widget, |parent_window| may be
  // provided to ensure this bubble is closed when the parent closes.
  //
  // |close_callback| will be called when the bubble is destroyed. The argument
  // indicates what action (if any) the user took to close the bubble.
  SafetyTipPageInfoBubbleView(
      views::View* anchor_view,
      const gfx::Rect& anchor_rect,
      gfx::NativeView parent_window,
      content::WebContents* web_contents,
      security_state::SafetyTipStatus safety_tip_status,
      const GURL& suggested_url,
      base::OnceCallback<void(SafetyTipInteraction)> close_callback);
  ~SafetyTipPageInfoBubbleView() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* button, const ui::Event& event) override;

 private:
  friend class SafetyTipPageInfoBubbleViewBrowserTest;

  void ExecuteLeaveCommand();
  void OpenHelpCenter();

  views::Button* GetLeaveButtonForTesting() { return leave_button_; }

  // WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void DidChangeVisibleSecurityState() override;

  const security_state::SafetyTipStatus safety_tip_status_;

  // The URL of the page the Safety Tip suggests you intended to go to, when
  // applicable (for SafetyTipStatus::kLookalike).
  const GURL suggested_url_;

  views::StyledLabel* info_button_;
  views::Button* ignore_button_;
  views::Button* leave_button_;
  base::OnceCallback<void(SafetyTipInteraction)> close_callback_;
  SafetyTipInteraction action_taken_ = SafetyTipInteraction::kNoAction;

  DISALLOW_COPY_AND_ASSIGN(SafetyTipPageInfoBubbleView);
};

// Creates and returns a safety tip bubble. Used in unit tests.
PageInfoBubbleViewBase* CreateSafetyTipBubbleForTesting(
    gfx::NativeView parent_view,
    content::WebContents* web_contents,
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& suggested_url,
    base::OnceCallback<void(SafetyTipInteraction)> close_callback);

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_SAFETY_TIP_PAGE_INFO_BUBBLE_VIEW_H_
