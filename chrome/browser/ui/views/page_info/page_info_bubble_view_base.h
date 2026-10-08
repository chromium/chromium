// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_BASE_H_

#include "components/page_info/page_info_ui.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

// Common class to |PageInfoBubbleView| and |InternalPageInfoBubbleView|.
class PageInfoBubbleViewBase : public views::BubbleDialogDelegateView,
                               public content::WebContentsObserver {
  METADATA_HEADER(PageInfoBubbleViewBase, views::BubbleDialogDelegateView)

 public:
  // Type of the bubble being displayed.
  enum BubbleType {
    BUBBLE_NONE,
    // Usual page info bubble for websites.
    BUBBLE_PAGE_INFO,
    // Custom bubble for internal pages like chrome:// and chrome-extensions://.
    BUBBLE_INTERNAL_PAGE,
    // Custom bubble for displaying safety tips.
    BUBBLE_SAFETY_TIP,
    // Custom bubble for displaying suspicious site warnings.
    BUBBLE_SUSPICIOUS_SITE,
  };

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPageInfoBubbleElementIdentifier);

  PageInfoBubbleViewBase(const PageInfoBubbleViewBase&) = delete;
  PageInfoBubbleViewBase& operator=(const PageInfoBubbleViewBase&) = delete;

  // Returns true if a page info bubble is showing for the given WebContents.
  static bool IsShowing(content::WebContents* web_contents);

  // Returns the type of the bubble being shown. For testing only.
  static BubbleType GetShownBubbleType();
  static BubbleType GetShownBubbleType(content::WebContents* web_contents);

  // Returns the page info bubble being shown.
  static views::BubbleDialogDelegateView* GetPageInfoBubbleForTesting();
  static views::BubbleDialogDelegateView* GetPageInfoBubbleForTesting(
      content::WebContents* web_contents);

  // Returns the specific bubble type for this instance.
  BubbleType type() const { return type_; }

 protected:
  PageInfoBubbleViewBase(views::BubbleAnchor anchor,
                         const gfx::Rect& anchor_rect,
                         gfx::NativeView parent_window,
                         BubbleType type,
                         content::WebContents* web_contents);

  // views::BubbleDialogDelegateView:
  void OnWidgetDestroying(views::Widget* widget) override;

  // WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  friend class SafetyTipPageInfoBubbleViewBrowserTest;

  // WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryPageChanged(content::Page& page) override;
  void DidChangeVisibleSecurityState() override;

  const BubbleType type_ = BUBBLE_NONE;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_BASE_H_
