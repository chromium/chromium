// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_layout.h"

#include <algorithm>

#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/trace_event/common/trace_event_common.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/contents_layout_manager.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"

using views::View;
using web_modal::WebContentsModalDialogHost;
using web_modal::ModalDialogHostObserver;

namespace {

// The visible height of the shadow above the tabs. Clicks in this area are
// treated as clicks to the frame, rather than clicks to the tab.
const int kTabShadowSize = 2;
// The number of pixels the constrained window should overlap the bottom
// of the omnibox.
const int kConstrainedWindowOverlap = 3;

// Combines View::ConvertPointToTarget and View::HitTest for a given |point|.
// Converts |point| from |src| to |dst| and hit tests it against |dst|. The
// converted |point| can then be retrieved and used for additional tests.
bool ConvertedHitTest(views::View* src, views::View* dst, gfx::Point* point) {
  DCHECK(src);
  DCHECK(dst);
  DCHECK(point);
  views::View::ConvertPointToTarget(src, dst, point);
  return dst->HitTestPoint(*point);
}

}  // namespace

constexpr int BrowserViewLayout::kMainBrowserContentsMinimumWidth;

class BrowserViewLayout::WebContentsModalDialogHostViews
    : public WebContentsModalDialogHost {
 public:
  explicit WebContentsModalDialogHostViews(
      BrowserViewLayout* browser_view_layout)
          : browser_view_layout_(browser_view_layout) {
  }

  ~WebContentsModalDialogHostViews() override {
    for (ModalDialogHostObserver& observer : observer_list_)
      observer.OnHostDestroying();
  }

  void NotifyPositionRequiresUpdate() {
    for (ModalDialogHostObserver& observer : observer_list_)
      observer.OnPositionRequiresUpdate();
  }

  gfx::Point GetDialogPosition(const gfx::Size& size) override {
    views::View* view = browser_view_layout_->contents_container_;
    gfx::Rect content_area = view->ConvertRectToWidget(view->GetLocalBounds());
    const int middle_x = content_area.x() + content_area.width() / 2;
    const int top = browser_view_layout_->web_contents_modal_dialog_top_y_;
    return gfx::Point(middle_x - size.width() / 2, top);
  }

  bool ShouldActivateDialog() const override {
    // The browser Widget may be inactive if showing a bubble so instead check
    // against the last active browser window when determining whether to
    // activate the dialog.
    return chrome::FindLastActive() ==
           browser_view_layout_->browser_view_->browser();
  }

  gfx::Size GetMaximumDialogSize() override {
    views::View* view = browser_view_layout_->contents_container_;
    gfx::Rect content_area = view->ConvertRectToWidget(view->GetLocalBounds());
    const int top = browser_view_layout_->web_contents_modal_dialog_top_y_;
    return gfx::Size(content_area.width(), content_area.bottom() - top);
  }

 private:
  gfx::NativeView GetHostView() const override {
    return browser_view_layout_->host_view_;
  }

  // Add/remove observer.
  void AddObserver(ModalDialogHostObserver* observer) override {
    observer_list_.AddObserver(observer);
  }
  void RemoveObserver(ModalDialogHostObserver* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  BrowserViewLayout* const browser_view_layout_;

  base::ObserverList<ModalDialogHostObserver>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsModalDialogHostViews);
};

////////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, public:

BrowserViewLayout::BrowserViewLayout(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    gfx::NativeView host_view,
    BrowserView* browser_view,
    views::View* top_container,
    TabStripRegionView* tab_strip_region_view,
    TabStrip* tab_strip,
    views::View* toolbar,
    InfoBarContainerView* infobar_container,
    views::View* contents_container,
    ImmersiveModeController* immersive_mode_controller,
    views::View* web_footer_experiment,
    views::View* contents_separator)
    : delegate_(std::move(delegate)),
      host_view_(host_view),
      browser_view_(browser_view),
      top_container_(top_container),
      tab_strip_region_view_(tab_strip_region_view),
      toolbar_(toolbar),
      infobar_container_(infobar_container),
      contents_container_(contents_container),
      immersive_mode_controller_(immersive_mode_controller),
      web_footer_experiment_(web_footer_experiment),
      contents_separator_(contents_separator),
      tab_strip_(tab_strip),
      dialog_host_(std::make_unique<WebContentsModalDialogHostViews>(this)) {}

BrowserViewLayout::~BrowserViewLayout() = default;

WebContentsModalDialogHost*
    BrowserViewLayout::GetWebContentsModalDialogHost() {
  return dialog_host_.get();
}

gfx::Size BrowserViewLayout::GetMinimumSize(const views::View* host) const {
  // Prevent having a 0x0 sized-contents as this can allow the window to be
  // resized down such that it's invisible and can no longer accept events.
  // Use a very small 1x1 size to allow the user and the web contents to be able
  // to resize the window as small as possible without introducing bugs.
  // https://crbug.com/847179.
  constexpr gfx::Size kContentsMinimumSize(1, 1);

  // The minimum height for the normal (tabbed) browser window's contents area.
  constexpr int kMainBrowserContentsMinimumHeight = 1;

  const bool has_tabstrip =
      delegate_->SupportsWindowFeature(Browser::FEATURE_TABSTRIP);
  const bool has_toolbar =
      delegate_->SupportsWindowFeature(Browser::FEATURE_TOOLBAR);
  const bool has_location_bar =
      delegate_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
  const bool has_bookmarks_bar =
      bookmark_bar_ && bookmark_bar_->GetVisible() &&
      delegate_->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR);

  gfx::Size tabstrip_size(
      has_tabstrip ? tab_strip_region_view_->GetMinimumSize() : gfx::Size());
  gfx::Size toolbar_size((has_toolbar || has_location_bar)
                             ? toolbar_->GetMinimumSize()
                             : gfx::Size());
  gfx::Size bookmark_bar_size;
  if (has_bookmarks_bar)
    bookmark_bar_size = bookmark_bar_->GetMinimumSize();
  gfx::Size infobar_container_size(infobar_container_->GetMinimumSize());
  // TODO(pkotwicz): Adjust the minimum height for the find bar.

  gfx::Size contents_size(contents_container_->GetMinimumSize());
  contents_size.SetToMax(delegate_->BrowserIsTypeNormal()
                             ? gfx::Size(kMainBrowserContentsMinimumWidth,
                                         kMainBrowserContentsMinimumHeight)
                             : kContentsMinimumSize);

  const int min_height =
      delegate_->GetTopInsetInBrowserView() + tabstrip_size.height() +
      toolbar_size.height() + bookmark_bar_size.height() +
      infobar_container_size.height() + contents_size.height();

  const int min_width = std::max(
      {tabstrip_size.width(), toolbar_size.width(), bookmark_bar_size.width(),
       infobar_container_size.width(), contents_size.width()});

  return gfx::Size(min_width, min_height);
}

gfx::NativeView BrowserViewLayout::GetHostView() {
  return delegate_->GetHostView();
}

int BrowserViewLayout::NonClientHitTest(const gfx::Point& point) {
  // Since the TabStrip only renders in some parts of the top of the window,
  // the un-obscured area is considered to be part of the non-client caption
  // area of the window. So we need to treat hit-tests in these regions as
  // hit-tests of the titlebar.

  views::View* parent = browser_view_->parent();

  gfx::Point point_in_browser_view_coords(point);
  views::View::ConvertPointToTarget(
      parent, browser_view_, &point_in_browser_view_coords);

  // Determine if the TabStrip exists and is capable of being clicked on. We
  // might be a popup window without a TabStrip.
  if (delegate_->IsTabStripVisible()) {
    // See if the mouse pointer is within the bounds of the TabStripRegionView.
    gfx::Point test_point(point);
    if (ConvertedHitTest(parent, tab_strip_region_view_, &test_point)) {
      if (tab_strip_region_view_->IsPositionInWindowCaption(test_point))
        return HTCAPTION;
      return HTCLIENT;
    }

    // The top few pixels of the TabStrip are a drop-shadow - as we're pretty
    // starved of dragable area, let's give it to window dragging (this also
    // makes sense visually).
    // TODO(tluk): Investigate the impact removing this has on draggable area
    // given the tab strip no longer uses shadows.
    views::Widget* widget = browser_view_->GetWidget();
    if (!(widget->IsMaximized() || widget->IsFullscreen()) &&
        (point_in_browser_view_coords.y() <
         (tab_strip_region_view_->y() + kTabShadowSize))) {
      // We return HTNOWHERE as this is a signal to our containing
      // NonClientView that it should figure out what the correct hit-test
      // code is given the mouse position...
      return HTNOWHERE;
    }
  }

  // If the point's y coordinate is below the top of the topmost view and
  // otherwise within the bounds of this view, the point is considered to be
  // within the client area.
  gfx::Rect bounds_from_toolbar_top = browser_view_->bounds();
  bounds_from_toolbar_top.Inset(0, GetClientAreaTop(), 0, 0);
  if (bounds_from_toolbar_top.Contains(point))
    return HTCLIENT;

  // If the point's y coordinate is above the top of the toolbar, but not
  // over the tabstrip (per previous checking in this function), then we
  // consider it in the window caption (e.g. the area to the right of the
  // tabstrip underneath the window controls). However, note that we DO NOT
  // return HTCAPTION here, because when the window is maximized the window
  // controls will fall into this space (since the BrowserView is sized to
  // entire size of the window at that point), and the HTCAPTION value will
  // cause the window controls not to work. So we return HTNOWHERE so that the
  // caller will hit-test the window controls before finally falling back to
  // HTCAPTION.
  gfx::Rect tabstrip_background_bounds = browser_view_->bounds();
  gfx::Point toolbar_origin = toolbar_->origin();
  views::View::ConvertPointToTarget(top_container_, browser_view_,
                                    &toolbar_origin);
  tabstrip_background_bounds.set_height(toolbar_origin.y());
  if (tabstrip_background_bounds.Contains(point))
    return HTNOWHERE;

  // If the point is somewhere else, delegate to the default implementation.
  return browser_view_->views::ClientView::NonClientHitTest(point);
}

//////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, views::LayoutManager implementation:

void BrowserViewLayout::Layout(views::View* browser_view) {
  TRACE_EVENT0("ui", "BrowserViewLayout::Layout");
  vertical_layout_rect_ = browser_view->GetLocalBounds();
  int top_inset = delegate_->GetTopInsetInBrowserView();
  int top = LayoutTabStripRegion(top_inset);
  if (delegate_->IsTabStripVisible()) {
    tab_strip_->SetBackgroundOffset(tab_strip_region_view_->GetMirroredX() +
                                    browser_view_->GetMirroredX() +
                                    delegate_->GetThemeBackgroundXInset());
  }
  top = LayoutWebUITabStrip(top);
  top = LayoutToolbar(top);

  top = LayoutBookmarkAndInfoBars(top, browser_view->y());

  // Top container requires updated toolbar and bookmark bar to compute bounds.
  UpdateTopContainerBounds();

  // Layout items at the bottom of the view.
  int bottom = LayoutWebFooterExperiment(browser_view->height());
  bottom = LayoutDownloadShelf(bottom);

  // Layout the contents container in the remaining space.
  const gfx::Rect old_contents_bounds = contents_container_->bounds();
  LayoutContentsContainerView(top, bottom);

  if (contents_border_widget_ && contents_border_widget_->IsVisible()) {
    gfx::Point contents_top_left;
    views::View::ConvertPointToScreen(contents_container_, &contents_top_left);
    contents_border_widget_->SetBounds(
        gfx::Rect(contents_top_left.x(), contents_top_left.y(),
                  contents_container_->width(), contents_container_->height()));
  }

  // This must be done _after_ we lay out the WebContents since this
  // code calls back into us to find the bounding box the find bar
  // must be laid out within, and that code depends on the
  // TabContentsContainer's bounds being up to date.
  //
  // Because Find Bar can be repositioned to keep from hiding find results, we
  // don't want to reset its position on every layout, however - only if the
  // geometry of the contents pane actually changes in a way that could affect
  // the positioning of the bar.
  if (delegate_->HasFindBarController() &&
      (contents_container_->y() != old_contents_bounds.y() ||
       contents_container_->width() != old_contents_bounds.width())) {
    delegate_->MoveWindowForFindBarIfNecessary();
  }

  // Adjust the fullscreen exit bubble bounds for |top_container_|'s new bounds.
  // This makes the fullscreen exit bubble look like it animates with
  // |top_container_| in immersive fullscreen.
  ExclusiveAccessBubbleViews* exclusive_access_bubble =
      delegate_->GetExclusiveAccessBubble();
  if (exclusive_access_bubble)
    exclusive_access_bubble->RepositionIfVisible();

  // Adjust any hosted dialogs if the browser's dialog hosting bounds changed.
  const gfx::Rect dialog_bounds(dialog_host_->GetDialogPosition(gfx::Size()),
                                dialog_host_->GetMaximumDialogSize());
  if (latest_dialog_bounds_ != dialog_bounds) {
    latest_dialog_bounds_ = dialog_bounds;
    dialog_host_->NotifyPositionRequiresUpdate();
  }
}

// Return the preferred size which is the size required to give each
// children their respective preferred size.
gfx::Size BrowserViewLayout::GetPreferredSize(const views::View* host) const {
  return gfx::Size();
}

//////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, private:

int BrowserViewLayout::LayoutTabStripRegion(int top) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutTabStripRegion");
  if (!delegate_->IsTabStripVisible()) {
    SetViewVisibility(tab_strip_region_view_, false);
    tab_strip_region_view_->SetBounds(0, 0, 0, 0);
    return top;
  }
  // This retrieves the bounds for the tab strip based on whether or not we show
  // anything to the left of it, like the incognito avatar.
  gfx::Rect tab_strip_region_bounds(
      delegate_->GetBoundsForTabStripRegionInBrowserView());

  SetViewVisibility(tab_strip_region_view_, true);
  tab_strip_region_view_->SetBoundsRect(tab_strip_region_bounds);

  return tab_strip_region_bounds.bottom() -
         GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
}

int BrowserViewLayout::LayoutWebUITabStrip(int top) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutWebUITabStrip");
  if (!webui_tab_strip_)
    return top;
  if (!webui_tab_strip_->GetVisible()) {
    webui_tab_strip_->SetBoundsRect(gfx::Rect());
    return top;
  }
  webui_tab_strip_->SetBounds(
      vertical_layout_rect_.x(), top, vertical_layout_rect_.width(),
      webui_tab_strip_->GetHeightForWidth(vertical_layout_rect_.width()));
  return webui_tab_strip_->bounds().bottom();
}

int BrowserViewLayout::LayoutToolbar(int top) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutToolbar");
  int browser_view_width = vertical_layout_rect_.width();
  bool toolbar_visible = delegate_->IsToolbarVisible();
  int height = toolbar_visible ? toolbar_->GetPreferredSize().height() : 0;
  SetViewVisibility(toolbar_, toolbar_visible);
  toolbar_->SetBounds(vertical_layout_rect_.x(), top, browser_view_width,
                      height);
  return toolbar_->bounds().bottom();
}

int BrowserViewLayout::LayoutBookmarkAndInfoBars(int top, int browser_view_y) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutBookmarkAndInfoBars");
  web_contents_modal_dialog_top_y_ =
      top + browser_view_y - kConstrainedWindowOverlap;

  if (bookmark_bar_) {
    top = std::max(toolbar_->bounds().bottom(), LayoutBookmarkBar(top));
  }

  if (delegate_->IsContentsSeparatorEnabled() &&
      (toolbar_->GetVisible() || bookmark_bar_) && top > 0) {
    SetViewVisibility(contents_separator_, true);
    const int separator_height =
        contents_separator_->GetPreferredSize().height();
    contents_separator_->SetBounds(vertical_layout_rect_.x(), top,
                                   vertical_layout_rect_.width(),
                                   separator_height);
    if (loading_bar_) {
      SetViewVisibility(loading_bar_, true);
      loading_bar_->SetBounds(vertical_layout_rect_.x(), top - 2,
                              vertical_layout_rect_.width(),
                              separator_height + 2);
      top_container_->ReorderChildView(loading_bar_, -1);
    }
    top += separator_height;
  } else {
    SetViewVisibility(contents_separator_, false);
    if (loading_bar_)
      SetViewVisibility(loading_bar_, false);
  }

  return LayoutInfoBar(top);
}

int BrowserViewLayout::LayoutBookmarkBar(int top) {
  if (!delegate_->IsBookmarkBarVisible()) {
    SetViewVisibility(bookmark_bar_, false);
    // TODO(jamescook): Don't change the bookmark bar height when it is
    // invisible, so we can use its height for layout even in that state.
    bookmark_bar_->SetBounds(0, top, browser_view_->width(), 0);
    return top;
  }

  bookmark_bar_->SetInfoBarVisible(IsInfobarVisible());
  int bookmark_bar_height = bookmark_bar_->GetPreferredSize().height();
  bookmark_bar_->SetBounds(vertical_layout_rect_.x(), top,
                           vertical_layout_rect_.width(), bookmark_bar_height);
  // Set visibility after setting bounds, as the visibility update uses the
  // bounds to determine if the mouse is hovering over a button.
  SetViewVisibility(bookmark_bar_, true);
  return top + bookmark_bar_height;
}

int BrowserViewLayout::LayoutInfoBar(int top) {
  // In immersive fullscreen or when top-chrome is fully hidden due to the page
  // gesture scroll slide behavior, the infobar always starts near the top of
  // the screen.
  if (immersive_mode_controller_->IsEnabled() ||
      (delegate_->IsTopControlsSlideBehaviorEnabled() &&
       delegate_->GetTopControlsSlideBehaviorShownRatio() == 0.f)) {
    // Can be null in tests.
    top = browser_view_ ? browser_view_->y() : 0;
  }

  SetViewVisibility(infobar_container_, IsInfobarVisible());
  infobar_container_->SetBounds(
      vertical_layout_rect_.x(), top, vertical_layout_rect_.width(),
      infobar_container_->GetPreferredSize().height());
  return top + infobar_container_->height();
}

void BrowserViewLayout::LayoutContentsContainerView(int top, int bottom) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutContentsContainerView");
  // |contents_container_| contains web page contents and devtools.
  // See browser_view.h for details.
  gfx::Rect contents_container_bounds(vertical_layout_rect_.x(),
                                      top,
                                      vertical_layout_rect_.width(),
                                      std::max(0, bottom - top));
  if (webui_tab_strip_ && webui_tab_strip_->GetVisible()) {
    // The WebUI tab strip container should "push" the tab contents down without
    // resizing it.
    contents_container_bounds.Inset(0, 0, 0,
                                    -webui_tab_strip_->size().height());
  }

  contents_container_->SetBoundsRect(contents_container_bounds);
}

void BrowserViewLayout::UpdateTopContainerBounds() {
  // Set the bounds of the top container view such that it is tall enough to
  // fully show all of its children. In particular, the bottom of the bookmark
  // bar can be above the bottom of the toolbar while the bookmark bar is
  // animating. The top container view is positioned relative to the top of the
  // client view instead of relative to GetTopInsetInBrowserView() because the
  // top container view paints parts of the frame (title, window controls)
  // during an immersive fullscreen reveal.
  int height = 0;
  for (views::View* child : top_container_->children()) {
    if (child->GetVisible())
      height = std::max(height, child->bounds().bottom());
  }

  // Ensure that the top container view reaches the topmost view in the
  // ClientView because the bounds of the top container view are used in
  // layout and we assume that this is the case.
  height = std::max(height, delegate_->GetTopInsetInBrowserView());

  gfx::Rect top_container_bounds(vertical_layout_rect_.width(), height);

  if (delegate_->IsTopControlsSlideBehaviorEnabled()) {
    // If the top controls are fully hidden, then it's positioned outside the
    // views' bounds.
    const float ratio = delegate_->GetTopControlsSlideBehaviorShownRatio();
    top_container_bounds.set_y(ratio == 0 ? -height : 0);
  } else {
    // If the immersive mode controller is animating the top container, it may
    // be partly offscreen.
    top_container_bounds.set_y(
        immersive_mode_controller_->GetTopContainerVerticalOffset(
            top_container_bounds.size()));
  }
  top_container_->SetBoundsRect(top_container_bounds);
}

int BrowserViewLayout::LayoutDownloadShelf(int bottom) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutDownloadShelf");
  if (download_shelf_ && download_shelf_->GetVisible()) {
    const int height = download_shelf_->GetPreferredSize().height();
    download_shelf_->SetBounds(vertical_layout_rect_.x(), bottom - height,
                               vertical_layout_rect_.width(), height);
    bottom -= height;
  }
  return bottom;
}

int BrowserViewLayout::GetClientAreaTop() {
  // If webui_tab_strip is displayed, the client area starts at its top,
  // otherwise at the top of the toolbar.
  return webui_tab_strip_ && webui_tab_strip_->GetVisible()
             ? webui_tab_strip_->y()
             : toolbar_->y();
}

int BrowserViewLayout::LayoutWebFooterExperiment(int bottom) {
  if (!web_footer_experiment_)
    return bottom;
  bottom -= 1;
  web_footer_experiment_->SetBounds(vertical_layout_rect_.x(), bottom,
                                    vertical_layout_rect_.width(), 1);
  return bottom;
}

bool BrowserViewLayout::IsInfobarVisible() const {
  // NOTE: Can't check if the size IsEmpty() since it's always 0-width.
  return infobar_container_->GetPreferredSize().height() != 0;
}
