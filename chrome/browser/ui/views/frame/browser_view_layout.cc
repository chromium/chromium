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
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/hit_test_utils.h"

using views::View;
using web_modal::ModalDialogHostObserver;
using web_modal::WebContentsModalDialogHost;

namespace {

// The number of pixels the constrained window should overlap the bottom
// of the omnibox.
const int kConstrainedWindowOverlap = 3;

// The normal clipping created by `View::Paint()` may not cover the bottom of
// the TopContainerView at certain scale factor because both of the position and
// the height might be roudned down. This function sets the clip path that
// enlarges the height at 2 DPs to compensate this error (both origin and size)
// that the canvas can cover the entire TopContainerView.  See
// crbug.com/390669712 for more details.  TODO(crbug.com/41344902): Remove this
// hack once the pixel canvas is enabled on all aura platforms.  Note that macOS
// supports integer scale only, so this isn't necessary on macOS.
void SetClipPathWithBottomAllowance(views::View* view) {
  if (!features::IsPixelCanvasRecordingEnabled()) {
    constexpr int kBottomPaintAllowance = 2;
    const gfx::Rect local_bounds = view->GetLocalBounds();
    const int extended_height = local_bounds.height() + kBottomPaintAllowance;
    view->SetClipPath(
        SkPath::Rect(SkRect::MakeWH(local_bounds.width(), extended_height)));
  }
}

}  // namespace

constexpr int BrowserViewLayout::kMainBrowserContentsMinimumWidth;

struct BrowserViewLayout::ContentsContainerLayoutResult {
  gfx::Rect contents_container_bounds;
  gfx::Rect side_panel_bounds;
  bool side_panel_visible;
  bool side_panel_right_aligned;
  bool contents_container_after_side_panel;
  gfx::Rect separator_bounds;
};

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

  gfx::Point GetDialogPosition(const gfx::Size& dialog_size) override {
    // Horizontally places the dialog at the center of the content.

    views::View* view = browser_view_layout_->contents_container_;
    // Recalculate bounds of `contents_container_`. It may be stale due to
    // pending layouts (from switching tabs, for example). The `top` and
    // `bottom` parameters should not be relevant to the result, since we only
    // care about the resulting width here.
    BrowserViewLayout::ContentsContainerLayoutResult layout_result =
        browser_view_layout_->CalculateContentsContainerLayout(
            view->bounds().y(), view->bounds().bottom());

    int leading_x;
    if (base::i18n::IsRTL()) {
      // Dialog coordinates are not flipped for RTL, but the View's coordinates
      // are. Calculate the left edge of `contents_container_bounds`.
      if (layout_result.contents_container_after_side_panel) {
        leading_x = 0;
      } else {
        leading_x = browser_view_layout_->vertical_layout_rect_.width() -
                    layout_result.contents_container_bounds.width();
      }
    } else {
      leading_x = layout_result.contents_container_bounds.x();
    }
    const int middle_x =
        leading_x + layout_result.contents_container_bounds.width() / 2;
    return gfx::Point(middle_x - dialog_size.width() / 2,
                      browser_view_layout_->dialog_top_y_);
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
    return host_widget ? host_widget->GetNativeView() : gfx::NativeView();
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
    views::View* window_scrim,
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
      window_scrim_(window_scrim),
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
      dialog_host_(std::make_unique<WebContentsModalDialogHostViews>(this)) {}

BrowserViewLayout::~BrowserViewLayout() = default;

void BrowserViewLayout::SetUseBrowserContentMinimumSize(
    bool use_browser_content_minimum_size) {
  use_browser_content_minimum_size_ = use_browser_content_minimum_size;
  InvalidateLayout();
}

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
  if (has_bookmarks_bar) {
    bookmark_bar_size = bookmark_bar_->GetMinimumSize();
  }
  gfx::Size infobar_container_size(infobar_container_->GetMinimumSize());
  // TODO(pkotwicz): Adjust the minimum height for the find bar.

  gfx::Size contents_size(contents_container_->GetMinimumSize());
  contents_size.SetToMax(use_browser_content_minimum_size_
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

//////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, views::LayoutManager implementation:

void BrowserViewLayout::Layout(views::View* browser_view) {
  TRACE_EVENT0("ui", "BrowserViewLayout::Layout");
  vertical_layout_rect_ = browser_view->GetLocalBounds();
  // The window scrim covers the entire browser view.
  if (window_scrim_) {
    window_scrim_->SetBoundsRect(vertical_layout_rect_);
  }

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
  if (exclusive_access_bubble) {
    exclusive_access_bubble->RepositionIfVisible();
  }

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
    auto top_container_iter = std::ranges::find(result, top_container_);
    auto contents_container_iter =
        std::ranges::find(result, contents_container_);
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
  if (!webui_tab_strip_) {
    return top;
  }
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
  SetClipPathWithBottomAllowance(toolbar_);
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
    if (loading_bar_) {
      SetViewVisibility(loading_bar_, false);
    }
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
  SetClipPathWithBottomAllowance(bookmark_bar_);
  if (!features::IsPixelCanvasRecordingEnabled()) {
    // Make sure the contents separator is painted last as the background for
    // BookmarkVieBar/ToolbarView may paint over it otherwise.
    // TODO(crbug.com/41344902): Remove once the pixel canvas is enabled on
    // all aura platforms.
    if (top_container_ == bookmark_bar_->parent()) {
      top_container_->ReorderChildView(contents_separator_,
                                       top_container_->children().size());
    }
  }

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
  if (infobar_container_->GetVisible()) {
    infobar_container_->SetBounds(
        vertical_layout_rect_.x(), infobar_top, vertical_layout_rect_.width(),
        infobar_container_->GetPreferredSize().height());
  } else {
    infobar_container_->SetBounds(vertical_layout_rect_.x(), infobar_top, 0, 0);
  }
  return content_top;
}

BrowserViewLayout::ContentsContainerLayoutResult
BrowserViewLayout::CalculateContentsContainerLayout(int top, int bottom) const {
  gfx::Rect contents_container_bounds(vertical_layout_rect_.x(), top,
                                      vertical_layout_rect_.width(),
                                      std::max(0, bottom - top));
  if (webui_tab_strip_ && webui_tab_strip_->GetVisible()) {
    // The WebUI tab strip container should "push" the tab contents down without
    // resizing it.
    contents_container_bounds.Inset(
        gfx::Insets().set_bottom(-webui_tab_strip_->size().height()));
  }

  const bool side_panel_visible =
      unified_side_panel_ && unified_side_panel_->GetVisible();
  if (!side_panel_visible) {
    // The contents container takes all available space, and we're done.
    return ContentsContainerLayoutResult{contents_container_bounds,
                                         gfx::Rect(),
                                         false,
                                         false,
                                         false,
                                         gfx::Rect()};
  }

  SidePanel* side_panel = views::AsViewClass<SidePanel>(unified_side_panel_);

  const bool side_panel_right_aligned = side_panel->IsRightAligned();
  const bool is_in_split_view = delegate_->IsInSplitView();
  views::View* side_panel_separator =
      side_panel_right_aligned ? right_aligned_side_panel_separator_.get()
                               : left_aligned_side_panel_separator_.get();
  CHECK(side_panel_separator);
  const int separator_width =
      is_in_split_view ? 0 : side_panel_separator->GetPreferredSize().width();

  // Side panel occupies some of the container's space. The side panel should
  // never occupy more space than is available in the content window, and
  // should never force the web contents to be smaller than its intended
  // minimum.
  gfx::Rect side_panel_bounds = contents_container_bounds;

  // If necessary, cap the side panel width at 2/3rds of the contents container
  // width as long as the side panel remains at or above its minimum width.
  if (side_panel->ShouldRestrictMaxWidth()) {
    side_panel_bounds.set_width(
        std::max(std::min(side_panel->GetPreferredSize().width(),
                          contents_container_bounds.width() * 2 / 3),
                 side_panel->GetMinimumSize().width()));
  } else {
    side_panel_bounds.set_width(std::min(side_panel->GetPreferredSize().width(),
                                         contents_container_bounds.width() -
                                             GetMinWebContentsWidth() -
                                             separator_width));
  }

  double side_panel_visible_width =
      side_panel_bounds.width() *
      views::AsViewClass<SidePanel>(unified_side_panel_)->GetAnimationValue();

  // Shrink container bounds to fit the side panel.
  contents_container_bounds.set_width(contents_container_bounds.width() -
                                      side_panel_visible_width -
                                      separator_width);

  // In LTR, the point (0,0) represents the top left of the browser.
  // In RTL, the point (0,0) represents the top right of the browser.
  const bool contents_container_after_side_panel =
      (base::i18n::IsRTL() && side_panel_right_aligned) ||
      (!base::i18n::IsRTL() && !side_panel_right_aligned);

  if (contents_container_after_side_panel) {
    // When the side panel should appear before the main content area relative
    // to the ui direction, move `contents_container_bounds` after the side
    // panel. Also leave space for the separator.
    contents_container_bounds.set_x(side_panel_visible_width + separator_width);
    side_panel_bounds.set_x(side_panel_bounds.x() - (side_panel_bounds.width() -
                                                     side_panel_visible_width));
  } else {
    // When the side panel should appear after the main content area relative to
    // the ui direction, move `side_panel_bounds` after the main content area.
    // Also leave space for the separator.
    side_panel_bounds.set_x(contents_container_bounds.right() +
                            separator_width);
  }

  // Adjust the side panel separator bounds based on the side panel bounds
  // calculated above.
  gfx::Rect separator_bounds = side_panel_bounds;
  // TODO (https://crbug.com/389972209): Adding 1px to the width as a bandaid
  // fix. This covers a case with subpixeling where a thin line of the
  // background finds its way to the front.
  separator_bounds.set_width(separator_width + 1);
  // If the side panel appears before `contents_container_bounds`, place the
  // separator immediately after the side panel but before the container
  // bounds. If the side panel appears after `contents_container_bounds`,
  // place the separator immediately after the contents bounds but before the
  // side panel.
  separator_bounds.set_x(contents_container_after_side_panel
                             ? side_panel_bounds.right()
                             : contents_container_bounds.right());

  return BrowserViewLayout::ContentsContainerLayoutResult{
      contents_container_bounds,
      side_panel_bounds,
      side_panel_visible,
      side_panel_right_aligned,
      contents_container_after_side_panel,
      separator_bounds};
}

void BrowserViewLayout::LayoutContentsContainerView(int top, int bottom) {
  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutContentsContainerView");
  // |contents_container_| contains web page contents and devtools.
  // See browser_view.h for details.

  BrowserViewLayout::ContentsContainerLayoutResult layout_result =
      CalculateContentsContainerLayout(top, bottom);
  const bool is_in_split_view = delegate_->IsInSplitView();

  contents_container_->SetBoundsRect(layout_result.contents_container_bounds);

  if (unified_side_panel_) {
    unified_side_panel_->SetBoundsRect(layout_result.side_panel_bounds);
  }
  if (right_aligned_side_panel_separator_) {
    SetViewVisibility(right_aligned_side_panel_separator_,
                      layout_result.side_panel_visible &&
                          layout_result.side_panel_right_aligned &&
                          !is_in_split_view);
    right_aligned_side_panel_separator_->SetBoundsRect(
        layout_result.separator_bounds);
  }
  if (left_aligned_side_panel_separator_) {
    SetViewVisibility(left_aligned_side_panel_separator_,
                      layout_result.side_panel_visible &&
                          !layout_result.side_panel_right_aligned &&
                          !is_in_split_view);
    left_aligned_side_panel_separator_->SetBoundsRect(
        layout_result.separator_bounds);
  }

  if (side_panel_rounded_corner_) {
    SetViewVisibility(side_panel_rounded_corner_,
                      layout_result.side_panel_visible && !is_in_split_view);
    if (layout_result.side_panel_visible) {
      // This can return nullptr when there is no Widget (for context, see
      // http://crbug.com/40178332). The nullptr dereference does not always
      // crash due to compiler optimizations, so CHECKing here ensures we crash.
      CHECK(side_panel_rounded_corner_->GetLayoutProvider());
      // Adjust the rounded corner bounds based on the side panel bounds.
      const float corner_radius =
          side_panel_rounded_corner_->GetLayoutProvider()
              ->GetCornerRadiusMetric(
                  views::ShapeContextTokens::kSidePanelPageContentRadius);
      const float corner_size = corner_radius + views::Separator::kThickness;
      if (layout_result.contents_container_after_side_panel) {
        side_panel_rounded_corner_->SetBounds(
            layout_result.side_panel_bounds.right(),
            layout_result.side_panel_bounds.y() - views::Separator::kThickness,
            corner_size, corner_size);
      } else {
        side_panel_rounded_corner_->SetBounds(
            layout_result.side_panel_bounds.x() - corner_radius -
                views::Separator::kThickness,
            layout_result.side_panel_bounds.y() - views::Separator::kThickness,
            corner_size, corner_size);
      }
    }
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
    if (child->GetVisible()) {
      height = std::max(height, child->bounds().bottom());
    }
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
  SetClipPathWithBottomAllowance(top_container_);
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
#if BUILDFLAG(IS_CHROMEOS)
  // On Ash placing the border widget on top of the contents container
  // does not require an offset -- see crbug.com/1030925.
  contents_top_left =
      gfx::Point(contents_container_->x(), contents_container_->y());
#else
  views::View::ConvertPointToScreen(contents_container_, &contents_top_left);
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
  return !infobar_container_->IsEmpty() &&
         (!browser_view_->IsFullscreen() ||
          !infobar_container_->ShouldHideInFullscreen());
}
