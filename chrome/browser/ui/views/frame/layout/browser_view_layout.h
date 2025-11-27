// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/layout/layout_manager.h"

class BookmarkBarView;
class Browser;
class BrowserViewLayoutDelegate;
class InfoBarContainerView;
class MultiContentsView;
class SidePanel;
class TabStrip;
class TabStripRegionView;
class WebAppFrameToolbarView;

namespace views {
class View;
class Label;
}  // namespace views

namespace web_modal {
class WebContentsModalDialogHost;
}

// Views associated with a browser view.
struct BrowserViewLayoutViews {
  BrowserViewLayoutViews();
  BrowserViewLayoutViews(BrowserViewLayoutViews&&) noexcept;
  BrowserViewLayoutViews& operator=(BrowserViewLayoutViews&&) noexcept;
  ~BrowserViewLayoutViews();

  // LINT.IfChange(BrowserViewLayoutViews)

  // The Browser View, but only as a view.
  raw_ptr<views::View> browser_view = nullptr;

  // Child views that the layout manager manages.
  // NOTE: If you add a view, try to add it as a views::View, which makes
  // testing much easier.
  raw_ptr<views::View> window_scrim = nullptr;
  raw_ptr<views::View> main_background_region = nullptr;
  raw_ptr<views::View> main_shadow_overlay = nullptr;
  raw_ptr<views::View> top_container = nullptr;
  raw_ptr<WebAppFrameToolbarView> web_app_frame_toolbar = nullptr;
  raw_ptr<views::Label> web_app_window_title = nullptr;
  raw_ptr<TabStripRegionView> tab_strip_region_view = nullptr;
  raw_ptr<views::View> vertical_tab_strip_container = nullptr;
  raw_ptr<views::View> toolbar = nullptr;
  raw_ptr<InfoBarContainerView> infobar_container = nullptr;
  raw_ptr<views::View> contents_container = nullptr;
  raw_ptr<MultiContentsView> multi_contents_view = nullptr;
  raw_ptr<SidePanel> toolbar_height_side_panel = nullptr;
  raw_ptr<SidePanel> contents_height_side_panel = nullptr;
  raw_ptr<views::View> side_panel_animation_content = nullptr;

  // TODO(crbug.com/424236535): These can be removed once `SideBySide` is
  // launched.
  raw_ptr<views::View> left_aligned_side_panel_separator = nullptr;
  raw_ptr<views::View> right_aligned_side_panel_separator = nullptr;
  raw_ptr<views::View> side_panel_rounded_corner = nullptr;

  // The contents separator used for when the top container is overlaid.
  // Note: when `SideBySide` feature is disabled, this separator is also
  // used when not overlaid. Once the feature is fully rolled out, we can
  // rely on `MultiContentsView` to manage the contents separator when not
  // overlaid (i.e. no immersive fullscreen).
  raw_ptr<views::View> top_container_separator = nullptr;

  // LINT.ThenChange(//chrome/browser/ui/views/frame/browser_view.cc:BrowserViewLayoutViews)

  // These views are dynamically set.
  raw_ptr<views::View> webui_tab_strip = nullptr;
  raw_ptr<views::View> loading_bar = nullptr;
  raw_ptr<BookmarkBarView> bookmark_bar = nullptr;
};

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

  // The width of the vertical tab strip.
  //
  // TODO(https://crbug.com/439961053): This shouldn't be hard-coded and should
  // be reported by the vertical tabstrip itself.
  static constexpr int kMinVerticalTabStripWidth = 240;

  BrowserViewLayout(const BrowserViewLayout&) = delete;
  BrowserViewLayout& operator=(const BrowserViewLayout&) = delete;

  ~BrowserViewLayout() override;

  // Creates the appropriate layout object.
  //
  // Can be used to switch between layout experiments, layouts by browser type,
  // etc.
  static std::unique_ptr<BrowserViewLayout> CreateLayout(
      std::unique_ptr<BrowserViewLayoutDelegate> delegate,
      Browser* browser,
      BrowserViewLayoutViews views);

  // Sets or updates views that are not available when |this| is initialized.
  void set_webui_tab_strip(views::View* webui_tab_strip) {
    views_.webui_tab_strip = webui_tab_strip;
  }
  void set_loading_bar(views::View* loading_bar) {
    views_.loading_bar = loading_bar;
  }
  void set_bookmark_bar(BookmarkBarView* bookmark_bar) {
    views_.bookmark_bar = bookmark_bar;
  }
  void set_side_panel_animation_content(views::View* contents_to_animate) {
    views_.side_panel_animation_content = contents_to_animate;
  }
  views::View* side_panel_animation_content() {
    return views_.side_panel_animation_content;
  }

  // views::LayoutManager overrides:
  gfx::Size GetPreferredSize(
      const views::View* host,
      const views::SizeBounds& available_size) const override;
  gfx::Size GetPreferredSize(const views::View* host) const override;

  // Used by BrowserView.
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost();

  // Test-only methods.

  // Returns the minimum acceptable width for the browser web contents.
  bool IsInfobarVisibleForTesting() const;
  void SetDelegateForTesting(
      std::unique_ptr<BrowserViewLayoutDelegate> delegate);

  // DEPRECATED - do not call.
  //
  // TODO(https://crbug.com/454583671): Eliminate this in favor of something
  // that actually returns the specific width needed by the test, or else find
  // some other way to calculate this in the test itself.
  virtual int GetMinWebContentsWidthForTesting() const = 0;

 protected:
  // |browser| may be null in tests.
  BrowserViewLayout(std::unique_ptr<BrowserViewLayoutDelegate> delegate,
                    Browser* browser,
                    BrowserViewLayoutViews views);

  const BrowserViewLayoutViews& views() const { return views_; }

  Browser* browser() { return browser_; }
  const Browser* browser() const { return browser_; }
  BrowserViewLayoutDelegate& delegate() { return *delegate_; }
  const BrowserViewLayoutDelegate& delegate() const { return *delegate_; }

  virtual gfx::Point GetDialogPosition(const gfx::Size& dialog_size) const = 0;
  virtual gfx::Size GetMaximumDialogSize() const = 0;

  // Returns the current pref for vertical tabs by accessing the vertical
  // tab strip state controller
  bool ShouldDisplayVerticalTabs() const;

  // Returns true if an infobar is showing.
  bool IsInfobarVisible() const;

  // Updates bubbles, dialogs, and infobars.
  // Must be called *after* contents pane is laid out.
  void UpdateBubbles();

 private:
  class BrowserModalDialogHostViews;

  // The delegate interface. May be a mock or replaced in tests.
  std::unique_ptr<BrowserViewLayoutDelegate> delegate_;

  // The owning browser view.
  const raw_ptr<Browser> browser_;

  // The collection of Views associated with the browser.
  BrowserViewLayoutViews views_;

  // The host for use in positioning the web contents browser modal dialog.
  std::unique_ptr<BrowserModalDialogHostViews> dialog_host_;

  // The latest dialog bounds applied during a layout pass.
  gfx::Rect latest_dialog_bounds_in_screen_;

  // The latest contents bounds applied during a layout pass, in screen
  // coordinates.
  gfx::Rect latest_contents_bounds_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_H_
