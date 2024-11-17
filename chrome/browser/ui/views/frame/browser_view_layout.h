// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
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
class TabStripRegionView;
class WebAppFrameToolbarView;

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class View;
class Label;
class Widget;
}  // namespace views

namespace web_modal {
class WebContentsModalDialogHost;
}

// The layout manager used in chrome browser.
class BrowserViewLayout : public views::LayoutManager {
 public:
  // The minimum width for the normal (tabbed or web app) browser window's
  // contents area. This should be wide enough that WebUI pages (e.g.
  // chrome://settings) and the various associated WebUI dialogs (e.g. Import
  // Bookmarks) can still be functional. This value provides a trade-off between
  // browser usability and privacy - specifically, the ability to browse in a
  // very small window, even on large monitors (which is why a minimum height is
  // not specified). This value is used for the main browser window only, not
  // for popups.
  static constexpr int kMainBrowserContentsMinimumWidth = 500;

  // |browser_view| may be null in tests.
  BrowserViewLayout(std::unique_ptr<BrowserViewLayoutDelegate> delegate,
                    BrowserView* browser_view,
                    views::View* top_container,
                    WebAppFrameToolbarView* web_app_frame_toolbar,
                    views::Label* web_app_window_title,
                    TabStripRegionView* tab_strip_region_view,
                    TabStrip* tab_strip,
                    views::View* toolbar,
                    InfoBarContainerView* infobar_container,
                    views::View* contents_container,
                    views::View* left_aligned_side_panel_separator,
                    views::View* unified_side_panel,
                    views::View* right_aligned_side_panel_separator,
                    views::View* side_panel_rounded_corner,
                    ImmersiveModeController* immersive_mode_controller,
                    views::View* contents_separator);

  BrowserViewLayout(const BrowserViewLayout&) = delete;
  BrowserViewLayout& operator=(const BrowserViewLayout&) = delete;

  ~BrowserViewLayout() override;

  // Sets or updates views that are not available when |this| is initialized.
  void set_tab_strip(TabStrip* tab_strip) { tab_strip_ = tab_strip; }
  void set_webui_tab_strip(views::View* webui_tab_strip) {
    webui_tab_strip_ = webui_tab_strip;
  }
  void set_loading_bar(views::View* loading_bar) { loading_bar_ = loading_bar; }
  void set_bookmark_bar(BookmarkBarView* bookmark_bar) {
    bookmark_bar_ = bookmark_bar;
  }
  void set_download_shelf(views::View* download_shelf) {
    download_shelf_ = download_shelf;
  }
  void set_contents_border_widget(views::Widget* contents_border_widget) {
    contents_border_widget_ = contents_border_widget;
  }
  void set_compact_mode(bool is_compact_mode) {
    is_compact_mode_ = is_compact_mode;
  }
  views::Widget* contents_border_widget() { return contents_border_widget_; }

  // Sets the bounds for the contents border.
  // * If nullopt, no specific bounds are set, and the border will be drawn
  //   around the entire contents area.
  // * Otherwise, the blue border will be drawn around the indicated Rect,
  //   which is in View coordinates.
  // Note that *whether* the border is drawn is an orthogonal issue;
  // this function only controls where it's drawn when it is in fact drawn.
  void SetContentBorderBounds(
      const std::optional<gfx::Rect>& region_capture_rect);

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
  gfx::Size GetPreferredSize(
      const views::View* host,
      const views::SizeBounds& available_size) const override;
  gfx::Size GetPreferredSize(const views::View* host) const override;
  std::vector<raw_ptr<views::View, VectorExperimental>>
  GetChildViewsInPaintOrder(const views::View* host) const override;

  // Returns the minimum acceptable width for the browser web contents.
  int GetMinWebContentsWidthForTesting() const;

  // Returns true if an infobar is showing.
  bool IsInfobarVisible() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserViewLayoutTest, BrowserViewLayout);
  FRIEND_TEST_ALL_PREFIXES(BrowserViewLayoutTest, Layout);
  FRIEND_TEST_ALL_PREFIXES(BrowserViewLayoutTest, LayoutDownloadShelf);
  class WebContentsModalDialogHostViews;

  // Layout the following controls, starting at |top|, returns the coordinate
  // of the bottom of the control, for laying out the next control.
  int LayoutTitleBarForWebApp(int top);
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

  // Layout the `side_panel`. This updates the passed in
  // `contents_container_bounds` to accommodate the side panel.
  void LayoutSidePanelView(views::View* side_panel,
                           gfx::Rect& contents_container_bounds);

  // Updates |top_container_|'s bounds. The new bounds depend on the size of
  // the bookmark bar and the toolbar.
  void UpdateTopContainerBounds();

  // Layout the Download Shelf, returns the coordinate of the top of the
  // control, for laying out the previous control.
  int LayoutDownloadShelf(int bottom);

  // Layout the contents border, which indicates the tab is being captured.
  void LayoutContentBorder();

  // Returns the y coordinate of the client area.
  int GetClientAreaTop();

  // Returns the minimum acceptable width for the browser web contents.
  int GetMinWebContentsWidth() const;

  // The delegate interface. May be a mock in tests.
  const std::unique_ptr<BrowserViewLayoutDelegate> delegate_;

  // The owning browser view.
  const raw_ptr<BrowserView, DanglingUntriaged> browser_view_;

  // Child views that the layout manager manages.
  // NOTE: If you add a view, try to add it as a views::View, which makes
  // testing much easier.
  const raw_ptr<views::View, AcrossTasksDanglingUntriaged> top_container_;
  const raw_ptr<WebAppFrameToolbarView, DanglingUntriaged>
      web_app_frame_toolbar_;
  const raw_ptr<views::Label, DanglingUntriaged> web_app_window_title_;
  const raw_ptr<TabStripRegionView, AcrossTasksDanglingUntriaged>
      tab_strip_region_view_;
  const raw_ptr<views::View, AcrossTasksDanglingUntriaged> toolbar_;
  const raw_ptr<InfoBarContainerView, AcrossTasksDanglingUntriaged>
      infobar_container_;
  const raw_ptr<views::View, AcrossTasksDanglingUntriaged> contents_container_;
  const raw_ptr<views::View, AcrossTasksDanglingUntriaged>
      left_aligned_side_panel_separator_;
  const raw_ptr<views::View, AcrossTasksDanglingUntriaged> unified_side_panel_;
  const raw_ptr<views::View, AcrossTasksDanglingUntriaged>
      right_aligned_side_panel_separator_;
  const raw_ptr<views::View, AcrossTasksDanglingUntriaged>
      side_panel_rounded_corner_;
  const raw_ptr<ImmersiveModeController, AcrossTasksDanglingUntriaged>
      immersive_mode_controller_;
  const raw_ptr<views::View, AcrossTasksDanglingUntriaged> contents_separator_;

  raw_ptr<views::View, DanglingUntriaged> webui_tab_strip_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> loading_bar_ = nullptr;
  raw_ptr<TabStrip, AcrossTasksDanglingUntriaged> tab_strip_ = nullptr;
  raw_ptr<BookmarkBarView, AcrossTasksDanglingUntriaged> bookmark_bar_ =
      nullptr;
  raw_ptr<views::View, DanglingUntriaged> download_shelf_ = nullptr;

  // The widget displaying a border on top of contents container for
  // highlighting the content. Not created by default.
  raw_ptr<views::Widget, DanglingUntriaged> contents_border_widget_ = nullptr;

  bool is_compact_mode_ = false;

  // The bounds within which the vertically-stacked contents of the BrowserView
  // should be laid out within. This is just the local bounds of the
  // BrowserView.
  // TODO(jamescook): Remove this and just use browser_view_->GetLocalBounds().
  gfx::Rect vertical_layout_rect_;

  // The host for use in positioning the web contents modal dialog.
  std::unique_ptr<WebContentsModalDialogHostViews> dialog_host_;

  // The latest dialog bounds applied during a layout pass.
  gfx::Rect latest_dialog_bounds_in_screen_;

  // The latest contents bounds applied during a layout pass, in screen
  // coordinates.
  gfx::Rect latest_contents_bounds_;

  // Directly tied to SetContentBorderBounds() - more details there.
  std::optional<gfx::Rect> dynamic_content_border_bounds_;

  // The distance the web contents modal dialog is from the top of the dialog
  // host widget.
  int dialog_top_y_ = -1;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_
