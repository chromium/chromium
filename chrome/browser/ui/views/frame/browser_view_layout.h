// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/layout/layout_manager.h"

class BookmarkBarView;
class BrowserView;
class BrowserViewLayoutDelegate;
class ImmersiveModeController;
class InfoBarContainerView;
class TabStrip;

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class View;
class Widget;
}  // namespace views

namespace web_modal {
class WebContentsModalDialogHost;
}

// The layout manager used in chrome browser.
class BrowserViewLayout : public views::LayoutManager {
 public:
  // The minimum width for the normal (tabbed) browser window's contents area.
  // This should be wide enough that WebUI pages (e.g. chrome://settings) and
  // the various associated WebUI dialogs (e.g. Import Bookmarks) can still be
  // functional. This value provides a trade-off between browser usability and
  // privacy - specifically, the ability to browse in a very small window, even
  // on large monitors (which is why a minimum height is not specified). This
  // value is used for the main browser window only, not for popups.
  static constexpr int kMainBrowserContentsMinimumWidth = 500;

  // |browser_view| may be null in tests.
  BrowserViewLayout(std::unique_ptr<BrowserViewLayoutDelegate> delegate,
                    gfx::NativeView host_view,
                    BrowserView* browser_view,
                    views::View* top_container,
                    views::View* tab_strip_region_view,
                    TabStrip* tab_strip,
                    views::View* toolbar,
                    InfoBarContainerView* infobar_container,
                    views::View* contents_container,
                    ImmersiveModeController* immersive_mode_controller,
                    views::View* web_footer_experiment,
                    views::View* contents_separator);
  ~BrowserViewLayout() override;

  // Sets or updates views that are not available when |this| is initialized.
  void set_tab_strip(TabStrip* tab_strip) { tab_strip_ = tab_strip; }
  void set_webui_tab_strip(views::View* webui_tab_strip) {
    webui_tab_strip_ = webui_tab_strip;
  }
  void set_bookmark_bar(BookmarkBarView* bookmark_bar) {
    bookmark_bar_ = bookmark_bar;
  }
  void set_download_shelf(views::View* download_shelf) {
    download_shelf_ = download_shelf;
  }
  void set_contents_border_widget(views::Widget* contents_border_widget) {
    contents_border_widget_ = contents_border_widget;
  }
  views::Widget* contents_border_widget() { return contents_border_widget_; }

  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost();

  // Returns the view against which the dialog is positioned and parented.
  gfx::NativeView GetHostView();

  // Tests to see if the specified |point| (in nonclient view's coordinates)
  // is within the views managed by the laymanager. Returns one of
  // HitTestCompat enum defined in ui/base/hit_test.h.
  // See also ClientView::NonClientHitTest.
  int NonClientHitTest(const gfx::Point& point);

  // views::LayoutManager overrides:
  void Layout(views::View* host) override;
  gfx::Size GetMinimumSize(const views::View* host) const override;
  gfx::Size GetPreferredSize(const views::View* host) const override;

  // Returns true if an infobar is showing.
  bool IsInfobarVisible() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserViewLayoutTest, BrowserViewLayout);
  FRIEND_TEST_ALL_PREFIXES(BrowserViewLayoutTest, Layout);
  FRIEND_TEST_ALL_PREFIXES(BrowserViewLayoutTest, LayoutDownloadShelf);
  class WebContentsModalDialogHostViews;

  // Layout the following controls, starting at |top|, returns the coordinate
  // of the bottom of the control, for laying out the next control.
  int LayoutTabStripRegion(int top);
  int LayoutWebUITabStrip(int top);
  int LayoutToolbar(int top);
  int LayoutBookmarkAndInfoBars(int top, int browser_view_y);
  int LayoutBookmarkBar(int top);
  int LayoutInfoBar(int top);

  // Layout the |contents_container_| view between the coordinates |top| and
  // |bottom|. See browser_view.h for details of the relationship between
  // |contents_container_| and other views.
  void LayoutContentsContainerView(int top, int bottom);

  // Updates |top_container_|'s bounds. The new bounds depend on the size of
  // the bookmark bar and the toolbar.
  void UpdateTopContainerBounds();

  // Layout the Download Shelf, returns the coordinate of the top of the
  // control, for laying out the previous control.
  int LayoutDownloadShelf(int bottom);

  // Returns the y coordinate of the client area.
  int GetClientAreaTop();

  // Layout the web-footer experiment if enabled, returns the top of the
  // control. See https://crbug.com/993502.
  int LayoutWebFooterExperiment(int bottom);

  // The delegate interface. May be a mock in tests.
  const std::unique_ptr<BrowserViewLayoutDelegate> delegate_;

  // The view against which the web dialog is positioned and parented.
  gfx::NativeView const host_view_;

  // The owning browser view.
  BrowserView* const browser_view_;

  // Child views that the layout manager manages.
  // NOTE: If you add a view, try to add it as a views::View, which makes
  // testing much easier.
  views::View* const top_container_;
  views::View* const tab_strip_region_view_;
  views::View* const toolbar_;
  InfoBarContainerView* const infobar_container_;
  views::View* const contents_container_;
  ImmersiveModeController* const immersive_mode_controller_;
  views::View* const web_footer_experiment_;
  views::View* const contents_separator_;

  views::View* webui_tab_strip_ = nullptr;
  TabStrip* tab_strip_ = nullptr;
  BookmarkBarView* bookmark_bar_ = nullptr;
  views::View* download_shelf_ = nullptr;

  // The widget displaying a border on top of contents container for
  // highlighting the content. Not created by default.
  views::Widget* contents_border_widget_ = nullptr;

  // The bounds within which the vertically-stacked contents of the BrowserView
  // should be laid out within. This is just the local bounds of the
  // BrowserView.
  // TODO(jamescook): Remove this and just use browser_view_->GetLocalBounds().
  gfx::Rect vertical_layout_rect_;

  // The host for use in positioning the web contents modal dialog.
  std::unique_ptr<WebContentsModalDialogHostViews> dialog_host_;

  // The latest dialog bounds applied during a layout pass.
  gfx::Rect latest_dialog_bounds_;

  // The distance the web contents modal dialog is from the top of the window,
  // in pixels.
  int web_contents_modal_dialog_top_y_ = -1;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayout);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_
