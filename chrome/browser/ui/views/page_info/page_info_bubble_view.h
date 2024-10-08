// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/page_info/page_info_history_controller.h"
#include "chrome/browser/ui/views/page_info/page_info_navigation_handler.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/base/metadata/metadata_header_macros.h"

class ChromePageInfoUiDelegate;
class PageSwitcherView;
class PageInfoViewFactory;

// The views implementation of the page info UI.
class PageInfoBubbleView : public PageInfoBubbleViewBase,
                           public PageInfoNavigationHandler {
  METADATA_HEADER(PageInfoBubbleView, PageInfoBubbleViewBase)

 public:
  // The column set id of the permissions table for |permissions_view_|.
  static constexpr int kPermissionColumnSetId = 0;

  ~PageInfoBubbleView() override;

  // Creates the appropriate page info bubble for the given |url|.
  // |anchor_view| will be used to place the bubble.  If |anchor_view| is null,
  // |anchor_rect| will be used instead.  |parent_window| will become the
  // parent of the widget hosting the bubble view.
  static views::BubbleDialogDelegateView* CreatePageInfoBubble(
      views::View* anchor_view,
      const gfx::Rect& anchor_rect,
      gfx::NativeWindow parent_window,
      content::WebContents* web_contents,
      const GURL& url,
      base::OnceClosure initialized_callback,
      PageInfoClosingCallback closing_callback,
      bool allow_about_this_site);

  // PageInfoNavigationHandler:
  void OpenMainPage(base::OnceClosure initialized_callback) override;
  void OpenSecurityPage() override;
  void OpenPermissionPage(ContentSettingsType type) override;
  void OpenAdPersonalizationPage() override;
  void OpenCookiesPage() override;
  void CloseBubble() override;

  // WebContentsObserver:
  void DidChangeVisibleSecurityState() override;

  PageInfo* presenter_for_testing() { return presenter_.get(); }

 private:
  PageInfoBubbleView(views::View* anchor_view,
                     const gfx::Rect& anchor_rect,
                     gfx::NativeView parent_window,
                     content::WebContents* web_contents,
                     const GURL& url,
                     base::OnceClosure initialized_callback,
                     PageInfoClosingCallback closing_callback,
                     bool allow_about_this_site);

  // PageInfoBubbleViewBase:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void WebContentsDestroyed() override;
  void ChildPreferredSizeChanged(views::View* child) override;

  void AnnouncePageOpened(std::u16string announcement);

  // The presenter that controls the Page Info UI.
  std::unique_ptr<PageInfo> presenter_;

  std::unique_ptr<ChromePageInfoUiDelegate> ui_delegate_;

  std::unique_ptr<PageInfoViewFactory> view_factory_;

  std::unique_ptr<PageInfoHistoryController> history_controller_;

  raw_ptr<PageSwitcherView> page_container_ = nullptr;

  PageInfoClosingCallback closing_callback_;

  base::WeakPtrFactory<PageInfoBubbleView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_H_
