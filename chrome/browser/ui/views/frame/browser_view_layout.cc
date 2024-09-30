// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_layout.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_math.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
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
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/pref_names.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/hit_test_utils.h"

using views::View;
using web_modal::ModalDialogHostObserver;
using web_modal::WebContentsModalDialogHost;

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
    : public WebContentsModalDialogHost,
      public views::WidgetObserver {
 public:
  explicit WebContentsModalDialogHostViews(
      BrowserViewLayout* browser_view_layout)
      : browser_view_layout_(browser_view_layout) {
    // browser_view might be nullptr in unit tests.
    if (browser_view_layout->browser_view_) {
      browser_widget_observation_.Observe(
          browser_view_layout->browser_view_->GetWidget());
    }
  }

  WebContentsModalDialogHostViews(const WebContentsModalDialogHostViews&) =
      delete;
  WebContentsModalDialogHostViews& operator=(
      const WebContentsModalDialogHostViews&) = delete;

  ~WebContentsModalDialogHostViews() override {
    observer_list_.Notify(&ModalDialogHostObserver::OnHostDestroying);
  }

  void NotifyPositionRequiresUpdate() {
    observer_list_.Notify(&ModalDialogHostObserver::OnPositionRequiresUpdate);
  }

  gfx::Point GetDialogPosition(const gfx::Size& size) override {
    // Horizontally places the dialog at the center of the content.
    views::View* view = browser_view_layout_->contents_container_;
    gfx::Rect rect = view->ConvertRectToWidget(view->GetLocalBounds());
    const int middle_x = rect.x() + rect.width() / 2;
    const int top = browser_view_layout_->dialog_top_y_;
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
    // Modals use NativeWidget and cannot be rendered beyond the browser
    // window boundaries. Restricting them to the browser window bottom
    // boundary and let the dialog to figure out a good layout.
    // WARNING: previous attempts to allow dialog to extend beyond the browser
    // boundaries have caused regressions in a number of dialogs. See
    // crbug.com/364463378, crbug.com/369739216, crbug.com/363205507.
    // TODO(crbug.com/334413759, crbug.com/346974105): use desktop widgets
    // universally.
    views::View* view = browser_view_layout_->contents_container_;
    gfx::Rect content_area = view->ConvertRectToWidget(view->GetLocalBounds());
    const int top = browser_view_layout_->dialog_top_y_;
    return gfx::Size(content_area.width(), content_area.bottom() - top);
  }

  views::Widget* GetHostWidget() const {
    return views::Widget::GetWidgetForNativeView(
        browser_view_layout_->delegate_->GetHostViewForAnchoring());
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* browser_widget) override {
    browser_widget_observation_.Reset();
  }
  void OnWidgetBoundsChanged(views::Widget* browser_widget,
                             const gfx::Rect& new_bounds) override {
    // Update the modal dialogs' position when the browser window bounds change.
    // This is used to adjust the modal dialog's position when the browser
    // window is being dragged across screen boundaries. We avoid having the
    // modal dialog partially visible as it may display security-sensitive
    // information.
    NotifyPositionRequiresUpdate();
  }

 private:
  gfx::NativeView GetHostView() const override {
    views::Widget* const host_widget = GetHostWidget();
    return host_widget ? host_widget->GetNativeView() : nullptr;
  }

  // Add/remove observer.
  void AddObserver(ModalDialogHostObserver* observer) override {
    observer_list_.AddObserver(observer);
  }
  void RemoveObserver(ModalDialogHostObserver* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  const raw_ptr<BrowserViewLayout> browser_view_layout_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_widget_observation_{this};

  base::ObserverList<ModalDialogHostObserver>::Unchecked observer_list_;
};

////////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, public:

BrowserViewLayout::BrowserViewLayout(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
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
    views::View* contents_separator)
    : delegate_(std::move(delegate)),
      browser_view_(browser_view),
      top_container_(top_container),
      web_app_frame_toolbar_(web_app_frame_toolbar),
      web_app_window_title_(web_app_window_title),
      tab_strip_region_view_(tab_strip_region_view),
      toolbar_(toolbar),
      infobar_container_(infobar_container),
      contents_container_(contents_container),
      left_aligned_side_panel_separator_(left_aligned_side_panel_separator),
      unified_side_panel_(unified_side_panel),
      right_aligned_side_panel_separator_(right_aligned_side_panel_separator),
      side_panel_rounded_corner_(side_panel_rounded_corner),
      immersive_mode_controller_(immersive_mode_controller),
      contents_separator_(contents_separator),
      tab_strip_(tab_strip),
      dialog_host_(std::make_unique<WebContentsModalDialogHostViews>(this)) {
}

BrowserViewLayout::~BrowserViewLayout() = default;

WebContentsModalDialogHost* BrowserViewLayout::GetWebContentsModalDialogHost() {
  return dialog_host_.get();
}

gfx::Size BrowserViewLayout::GetMinimumSize(const views::View* host) const {
  // Prevent having a 0x0 sized-contents as this can allow the window to be
  // resized down such that it's invisible and can no longer accept events.
  // Use a very small 1x1 size to allow the user and the web contents to be able
  // to resize the window as small as possible without introducing bugs.
  // https://crbug.com/847179.
  constexpr gfx::Size kContentsMinimumSize(1, 1);
  if (delegate_->GetBorderlessModeEnabled()) {
    // The minimum size of a window is unrestricted for a borderless mode app.
    return kContentsMinimumSize;
  }

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
  contents_size.SetToMax(
      (delegate_->BrowserIsTypeNormal() ||
       (delegate_->BrowserIsTypeApp() && delegate_->BrowserIsWebApp() &&
        !delegate_->BrowserIsSystemWebApp()))
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

void BrowserViewLayout::SetContentBorderBounds(
    const std::optional<gfx::Rect>& region_capture_rect) {
  dynamic_content_border_bounds_ = region_capture_rect;
  LayoutContentBorder();
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
  views::View::ConvertPointToTarget(parent, browser_view_,
                                    &point_in_browser_view_coords);

  // Check if the point is in the web_app_frame_toolbar_. Because this toolbar
  // can entirely be within the window controls overlay area, this check needs
  // to be done before the window controls overlay area check below.
  if (web_app_frame_toolbar_) {
    int web_app_component =
        views::GetHitTestComponent(web_app_frame_toolbar_, point);
    if (web_app_component != HTNOWHERE) {
      return web_app_component;
    }
  }

  // Let the frame handle any events that fall within the bounds of the window
  // controls overlay.
  if (browser_view_->IsWindowControlsOverlayEnabled() &&
      browser_view_->GetActiveWebContents()) {
    // The window controls overlays are to the left and/or right of the
    // |titlebar_area_rect|.
    gfx::Rect titlebar_area_rect =
        browser_view_->GetActiveWebContents()->GetWindowsControlsOverlayRect();

    // The top area rect is the same height as the |titlebar_area_rect| but
    // fills the full width of the browser view.
    gfx::Rect top_area_rect(0, titlebar_area_rect.y(), browser_view_->width(),
                            titlebar_area_rect.height());

    // If the point is within the top_area_rect but not the titlebar_area_rect,
    // then it must be in the window controls overlay.
    if (top_area_rect.Contains(point_in_browser_view_coords) &&
        !titlebar_area_rect.Contains(point_in_browser_view_coords))
      return HTNOWHERE;
  }

  // Determine if the TabStrip exists and is capable of being clicked on. We
  // might be a popup window without a TabStrip.
  if (delegate_->ShouldDrawTabStrip()) {
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

  // For PWAs with window-controls-overlay or borderless display override, see
  // if we're in an app defined draggable region so we can return htcaption.
  web_app::AppBrowserController* controller =
      browser_view_->browser()->app_controller();

  if (browser_view_->AreDraggableRegionsEnabled() && controller &&
      controller->draggable_region().has_value()) {
    // Draggable regions are defined relative to the web contents.
    gfx::Point point_in_contents_web_view_coords(point_in_browser_view_coords);
    views::View::ConvertPointToTarget(browser_view_,
                                      browser_view_->contents_web_view(),
                                      &point_in_contents_web_view_coords);

    if (controller->draggable_region()->contains(
            point_in_contents_web_view_coords.x(),
            point_in_contents_web_view_coords.y())) {
      // Draggable regions should be ignored for clicks into any browser view's
      // owned widgets, for example alerts, permission prompts or find bar.
      return browser_view_->WidgetOwnedByAnchorContainsPoint(
                 point_in_browser_view_coords)
                 ? HTCLIENT
                 : HTCAPTION;
    }
  }

  // If the point's y coordinate is below the top of the topmost view and
  // otherwise within the bounds of this view, the point is considered to be
  // within the client area.
  gfx::Rect bounds_from_toolbar_top = browser_view_->bounds();
  bounds_from_toolbar_top.Inset(gfx::Insets::TLBR(GetClientAreaTop(), 0, 0, 0));
  if (bounds_from_toolbar_top.Contains(point)) {
    return HTCLIENT;
  }

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
  if (tabstrip_background_bounds.Contains(point)) {
    return HTNOWHERE;
  }

  // If the point is somewhere else, delegate to the default implementation.
  return browser_view_->views::ClientView::NonClientHitTest(point);
}

//////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, views::LayoutManager implementation:

void BrowserViewLayout::Layout(views::View* browser_view) {
  TRACE_EVENT0("ui", "BrowserViewLayout::Layout");
  vertical_layout_rect_ = browser_view->GetLocalBounds();
  int top_inset = delegate_->GetTopInsetInBrowserView();
  int top = LayoutTitleBarForWebApp(top_inset);
  if (delegate_->ShouldLayoutTabStrip()) {
    top = LayoutTabStripRegion(top);
    if (delegate_->ShouldDrawTabStrip()) {
      tab_strip_->SetBackgroundOffset(tab_strip_region_view_->GetMirroredX() +
                                      browser_view_->GetMirroredX());
    }
    top = LayoutWebUITabStrip(top);
  }
  top = LayoutToolbar(top);

  top = LayoutBookmarkAndInfoBars(top, browser_view->y());

  // Top container requires updated toolbar and bookmark bar to compute bounds.
  UpdateTopContainerBounds();

  // Layout items at the bottom of the view.
  const int bottom = LayoutDownloadShelf(browser_view->height());

  // Layout the contents container in the remaining space.
  LayoutContentsContainerView(top, bottom);

  LayoutContentBorder();

  // This must be done _after_ we lay out the WebContents since this
  // code calls back into us to find the bounding box the find bar
  // must be laid out within, and that code depends on the
  // TabContentsContainer's bounds being up to date.
  //
  // Because Find Bar can be repositioned to keep from hiding find results, we
  // don't want to reset its position on every layout, however - only if the
  // geometry of the contents pane actually changes in a way that could affect
  // the positioning of the bar.
  const gfx::Rect new_contents_bounds =
      contents_container_->GetBoundsInScreen();
  if (delegate_->HasFindBarController() &&
      (new_contents_bounds.width() != latest_contents_bounds_.width() ||
       (new_contents_bounds.y() != latest_contents_bounds_.y() &&
        new_contents_bounds.height() != latest_contents_bounds_.height()))) {
    delegate_->MoveWindowForFindBarIfNecessary();
  }
  latest_contents_bounds_ = new_contents_bounds;

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
  const gfx::Rect host_widget_bounds =
      dialog_host_->GetHostWidget()
          ? dialog_host_->GetHostWidget()->GetClientAreaBoundsInScreen()
          : gfx::Rect();
  const gfx::Rect dialog_bounds_in_screen =
      dialog_bounds + host_widget_bounds.OffsetFromOrigin();
  if (latest_dialog_bounds_in_screen_ != dialog_bounds_in_screen) {
    latest_dialog_bounds_in_screen_ = dialog_bounds_in_screen;
    dialog_host_->NotifyPositionRequiresUpdate();
  }
}

gfx::Size BrowserViewLayout::GetPreferredSize(
    const views::View* host,
    const views::SizeBounds& available_size) const {
  return gfx::Size();
}

// Return the preferred size which is the size required to give each
// children their respective preferred size.
gfx::Size BrowserViewLayout::GetPreferredSize(const views::View* host) const {
  return GetPreferredSize(host, {});
}

std::vector<raw_ptr<views::View, VectorExperimental>>
BrowserViewLayout::GetChildViewsInPaintOrder(const views::View* host) const {
  std::vector<raw_ptr<views::View, VectorExperimental>> result =
      views::LayoutManager::GetChildViewsInPaintOrder(host);
  // Make sure `top_container_` is after `contents_container_` in paint order
  // when this is a window using WindowControlsOverlay, to make sure the window
  // controls are in fact drawn on top of the web contents.
  if (delegate_->IsWindowControlsOverlayEnabled()) {
    auto top_container_iter = base::ranges::find(result, top_container_);
    auto contents_container_iter =
        base::ranges::find(result, contents_container_);
    CHECK(contents_container_iter != result.end());
    // When in Immersive Fullscreen `top_container_` might not be one of our
    // children at all. While Window Controls Overlay shouldn't be enabled in
    // fullscreen either, during the transition there is a moment where both
    // could be true at the same time.
    if (top_container_iter != result.end()) {
      std::rotate(top_container_iter, top_container_iter + 1,
                  contents_container_iter + 1);
    }
  }
  return result;
}

int BrowserViewLayout::GetMinWebContentsWidthForTesting() const {
  return GetMinWebContentsWidth();
}

//////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, private:

int BrowserViewLayout::LayoutTitleBarForWebApp(int top) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutTitleBarForWebApp");
  if (!web_app_frame_toolbar_) {
    return top;
  }

  if (delegate_->GetBorderlessModeEnabled()) {
    web_app_frame_toolbar_->SetVisible(false);
    if (web_app_window_title_) {
      web_app_window_title_->SetVisible(false);
    }
    return top;
  }

  gfx::Rect toolbar_bounds(
      delegate_->GetBoundsForWebAppFrameToolbarInBrowserView());

  web_app_frame_toolbar_->SetVisible(!toolbar_bounds.IsEmpty());
  if (web_app_window_title_) {
    web_app_window_title_->SetVisible(!toolbar_bounds.IsEmpty());
  }
  if (toolbar_bounds.IsEmpty()) {
    return top;
  }

  if (delegate_->IsWindowControlsOverlayEnabled()) {
    web_app_frame_toolbar_->LayoutForWindowControlsOverlay(toolbar_bounds);
    toolbar_bounds.Subtract(web_app_frame_toolbar_->bounds());
    delegate_->UpdateWindowControlsOverlay(toolbar_bounds);
    if (web_app_window_title_) {
      web_app_window_title_->SetVisible(false);
    }
    return top;
  }

  gfx::Rect window_title_bounds =
      web_app_frame_toolbar_->LayoutInContainer(toolbar_bounds);

  if (web_app_window_title_) {
    if (delegate_->ShouldDrawTabStrip()) {
      web_app_window_title_->SetVisible(false);
    } else {
      delegate_->LayoutWebAppWindowTitle(window_title_bounds,
                                         *web_app_window_title_);
    }
  }

  return toolbar_bounds.bottom();
}

int BrowserViewLayout::LayoutTabStripRegion(int top) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutTabStripRegion");
  if (!delegate_->ShouldDrawTabStrip()) {
    SetViewVisibility(tab_strip_region_view_, false);
    tab_strip_region_view_->SetBounds(0, 0, 0, 0);
    return top;
  }
  // This retrieves the bounds for the tab strip based on whether or not we show
  // anything to the left of it, like the incognito avatar.
  gfx::Rect tab_strip_region_bounds(
      delegate_->GetBoundsForTabStripRegionInBrowserView());
  if (is_compact_mode_) {
    constexpr int retain_some_padding = 2;
    int height = GetLayoutConstant(TAB_STRIP_HEIGHT) -
                 GetLayoutConstant(TAB_STRIP_PADDING) + retain_some_padding;
    tab_strip_region_bounds.set_height(height);
  }

  if (web_app_frame_toolbar_) {
    tab_strip_region_bounds.Inset(gfx::Insets::TLBR(
        0, 0, 0, web_app_frame_toolbar_->GetPreferredSize().width()));
  }

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
  dialog_top_y_ = top + browser_view_y - kConstrainedWindowOverlap;

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
      top_container_->ReorderChildView(loading_bar_,
                                       top_container_->children().size());
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
    top = (browser_view_ ? browser_view_->y() : 0) +
          immersive_mode_controller_->GetMinimumContentOffset();
  }
  // The content usually starts at the bottom of the infobar. When there is an
  // extra infobar offset the infobar is shifted down while the content stays.
  int infobar_top = top;
  int content_top = infobar_top + infobar_container_->height();
  infobar_top += delegate_->GetExtraInfobarOffset();
  SetViewVisibility(infobar_container_, IsInfobarVisible());
  infobar_container_->SetBounds(
      vertical_layout_rect_.x(), infobar_top, vertical_layout_rect_.width(),
      infobar_container_->GetPreferredSize().height());
  return content_top;
}

void BrowserViewLayout::LayoutContentsContainerView(int top, int bottom) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutContentsContainerView");
  // |contents_container_| contains web page contents and devtools.
  // See browser_view.h for details.
  gfx::Rect contents_container_bounds(vertical_layout_rect_.x(), top,
                                      vertical_layout_rect_.width(),
                                      std::max(0, bottom - top));
  if (webui_tab_strip_ && webui_tab_strip_->GetVisible()) {
    // The WebUI tab strip container should "push" the tab contents down without
    // resizing it.
    contents_container_bounds.Inset(
        gfx::Insets().set_bottom(-webui_tab_strip_->size().height()));
  }

  LayoutSidePanelView(unified_side_panel_, contents_container_bounds);

  contents_container_->SetBoundsRect(contents_container_bounds);
}

void BrowserViewLayout::LayoutSidePanelView(
    views::View* side_panel,
    gfx::Rect& contents_container_bounds) {
  const bool side_panel_visible = side_panel && side_panel->GetVisible();
  // Update side panel rounded corner visibility to match side panel visibility.
  SetViewVisibility(side_panel_rounded_corner_, side_panel_visible);

  if (left_aligned_side_panel_separator_) {
    const bool side_panel_visible_on_left =
        side_panel_visible &&
        !views::AsViewClass<SidePanel>(unified_side_panel_)->IsRightAligned();
    SetViewVisibility(left_aligned_side_panel_separator_,
                      side_panel_visible_on_left);
  }

  if (right_aligned_side_panel_separator_) {
    const bool side_panel_visible_on_right =
        side_panel_visible &&
        views::AsViewClass<SidePanel>(unified_side_panel_)->IsRightAligned();
    SetViewVisibility(right_aligned_side_panel_separator_,
                      side_panel_visible_on_right);
  }

  if (!side_panel || !side_panel->GetVisible())
    return;

  DCHECK(side_panel == unified_side_panel_);
  bool is_right_aligned =
      views::AsViewClass<SidePanel>(side_panel)->IsRightAligned();

  views::View* side_panel_separator =
      is_right_aligned ? right_aligned_side_panel_separator_.get()
                       : left_aligned_side_panel_separator_.get();

  DCHECK(side_panel_separator);

  // Side panel occupies some of the container's space. The side panel should
  // never occupy more space than is available in the content window, and
  // should never force the web contents to be smaller than its intended
  // minimum.
  gfx::Rect side_panel_bounds = contents_container_bounds;

  side_panel_bounds.set_width(
      std::min(side_panel->GetPreferredSize().width(),
               contents_container_bounds.width() - GetMinWebContentsWidth() -
                   side_panel_separator->GetPreferredSize().width()));

  double side_panel_visible_width =
      side_panel_bounds.width() *
      views::AsViewClass<SidePanel>(unified_side_panel_)->GetAnimationValue();

  // Shrink container bounds to fit the side panel.
  contents_container_bounds.set_width(
      contents_container_bounds.width() - side_panel_visible_width -
      side_panel_separator->GetPreferredSize().width());

  // In LTR, the point (0,0) represents the top left of the browser.
  // In RTL, the point (0,0) represents the top right of the browser.
  const bool is_container_after_side_panel =
      (base::i18n::IsRTL() && is_right_aligned) ||
      (!base::i18n::IsRTL() && !is_right_aligned);

  if (is_container_after_side_panel) {
    // When the side panel should appear before the main content area relative
    // to the ui direction, move `contents_container_bounds` after the side
    // panel. Also leave space for the separator.
    contents_container_bounds.set_x(
        side_panel_visible_width +
        side_panel_separator->GetPreferredSize().width());
    side_panel_bounds.set_x(side_panel_bounds.x() - (side_panel_bounds.width() -
                                                     side_panel_visible_width));
  } else {
    // When the side panel should appear after the main content area relative to
    // the ui direction, move `side_panel_bounds` after the main content area.
    // Also leave space for the separator.
    side_panel_bounds.set_x(contents_container_bounds.right() +
                            side_panel_separator->GetPreferredSize().width());
  }

  side_panel->SetBoundsRect(side_panel_bounds);

  // Adjust the side panel separator bounds based on the side panel bounds
  // calculated above.
  gfx::Rect side_panel_separator_bounds = side_panel_bounds;
  side_panel_separator_bounds.set_width(
      side_panel_separator->GetPreferredSize().width());

  // If the side panel appears before `contents_container_bounds`, place the
  // separator immediately after the side panel but before the container bounds.
  // If the side panel appears after `contents_container_bounds`, place the
  // separator immediately after the contents bounds but before the side panel.
  side_panel_separator_bounds.set_x(is_container_after_side_panel
                                        ? side_panel_bounds.right()
                                        : contents_container_bounds.right());

  side_panel_separator->SetBoundsRect(side_panel_separator_bounds);

  // Adjust the side panel rounded corner bounds based on the side panel bounds
  // calculated above.
  const float corner_radius =
      side_panel_rounded_corner_->GetLayoutProvider()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kSidePanelPageContentRadius);
  if (is_container_after_side_panel) {
    side_panel_rounded_corner_->SetBounds(
        side_panel_bounds.right(),
        side_panel_bounds.y() - views::Separator::kThickness,
        corner_radius + views::Separator::kThickness,
        corner_radius + views::Separator::kThickness);
  } else {
    side_panel_rounded_corner_->SetBounds(
        side_panel_bounds.x() - corner_radius - views::Separator::kThickness,
        side_panel_bounds.y() - views::Separator::kThickness,
        corner_radius + views::Separator::kThickness,
        corner_radius + views::Separator::kThickness);
  }
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

void BrowserViewLayout::LayoutContentBorder() {
  if (!contents_border_widget_ || !contents_border_widget_->IsVisible()) {
    return;
  }

  gfx::Point contents_top_left;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  views::View::ConvertPointToScreen(contents_container_, &contents_top_left);
#else
  // On Ash placing the border widget on top of the contents container
  // does not require an offset -- see crbug.com/1030925.
  contents_top_left =
      gfx::Point(contents_container_->x(), contents_container_->y());
#endif

  gfx::Rect rect;
  if (dynamic_content_border_bounds_) {
    rect =
        gfx::Rect(contents_top_left.x() + dynamic_content_border_bounds_->x(),
                  contents_top_left.y() + dynamic_content_border_bounds_->y(),
                  dynamic_content_border_bounds_->width(),
                  dynamic_content_border_bounds_->height());
  } else {
    rect =
        gfx::Rect(contents_top_left.x(), contents_top_left.y(),
                  contents_container_->width(), contents_container_->height());
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Immersive top container might overlap with the blue border in fullscreen
  // mode - see crbug.com/1392733. By insetting the bounds rectangle we ensure
  // that the blue border is always placed below the top container.
  if (immersive_mode_controller_->IsRevealed()) {
    int delta = top_container_->bounds().bottom() - rect.y();
    if (delta > 0) {
      rect.Inset(gfx::Insets().set_top(delta));
    }
  }
#endif

  contents_border_widget_->SetBounds(rect);
}

int BrowserViewLayout::GetClientAreaTop() {
  // If webui_tab_strip is displayed, the client area starts at its top,
  // otherwise at the top of the toolbar.
  return webui_tab_strip_ && webui_tab_strip_->GetVisible()
             ? webui_tab_strip_->y()
             : toolbar_->y();
}

int BrowserViewLayout::GetMinWebContentsWidth() const {
  int min_width =
      kMainBrowserContentsMinimumWidth -
      unified_side_panel_->GetMinimumSize().width() -
      right_aligned_side_panel_separator_->GetPreferredSize().width();
  DCHECK_GE(min_width, 0);
  return min_width;
}

bool BrowserViewLayout::IsInfobarVisible() const {
  return !infobar_container_->IsEmpty();
}
