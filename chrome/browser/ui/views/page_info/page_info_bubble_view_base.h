// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_BASE_H_

#include "chrome/browser/ui/page_info/page_info_ui.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

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

// Common class to |PageInfoBubbleView| and |InternalPageInfoBubbleView|.
class PageInfoBubbleViewBase : public views::BubbleDialogDelegateView,
                               public content::WebContentsObserver {
 public:
  // Type of the bubble being displayed.
  enum BubbleType {
    BUBBLE_NONE,
    // Usual page info bubble for websites.
    BUBBLE_PAGE_INFO,
    // Custom bubble for internal pages like chrome:// and chrome-extensions://.
    BUBBLE_INTERNAL_PAGE,
    // Custom bubble for displaying safety tips.
    BUBBLE_SAFETY_TIP
  };

  // Returns the type of the bubble being shown. For testing only.
  static BubbleType GetShownBubbleType();

  // Returns the page info bubble being shown.
  static views::BubbleDialogDelegateView* GetPageInfoBubbleForTesting();

 protected:
  PageInfoBubbleViewBase(views::View* anchor_view,
                         const gfx::Rect& anchor_rect,
                         gfx::NativeView parent_window,
                         BubbleType type,
                         content::WebContents* web_contents);

  // views::BubbleDialogDelegateView:
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void OnWidgetDestroying(views::Widget* widget) override;

  PageInfoUI::SecurityDescriptionType GetSecurityDescriptionType() const;
  void set_window_title(const base::string16& title) { window_title_ = title; }
  void set_security_description_type(
      const PageInfoUI::SecurityDescriptionType& type) {
    security_description_type_ = type;
  }

 private:
  friend class SafetyTipPageInfoBubbleViewBrowserTest;

  // WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void DidChangeVisibleSecurityState() override;

  base::string16 window_title_;
  PageInfoUI::SecurityDescriptionType security_description_type_ =
      PageInfoUI::SecurityDescriptionType::CONNECTION;

  DISALLOW_COPY_AND_ASSIGN(PageInfoBubbleViewBase);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_BASE_H_
