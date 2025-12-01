// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_app_layout_impl.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl_old.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_popup_layout_impl.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_tabbed_layout_impl.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using web_modal::ModalDialogHostObserver;
using web_modal::WebContentsModalDialogHost;

BrowserViewLayoutViews::BrowserViewLayoutViews() = default;
BrowserViewLayoutViews::BrowserViewLayoutViews(
    BrowserViewLayoutViews&&) noexcept = default;
BrowserViewLayoutViews& BrowserViewLayoutViews::operator=(
    BrowserViewLayoutViews&&) noexcept = default;
BrowserViewLayoutViews::~BrowserViewLayoutViews() noexcept = default;

constexpr int BrowserViewLayout::kMainBrowserContentsMinimumWidth;

class BrowserViewLayout::BrowserModalDialogHostViews
    : public WebContentsModalDialogHost,
      public views::WidgetObserver {
 public:
  explicit BrowserModalDialogHostViews(BrowserViewLayout* browser_view_layout)
      : browser_view_layout_(browser_view_layout) {
    // browser_view might be nullptr in unit tests.
    if (browser_view_layout->views().browser_view) {
      browser_widget_observation_.Observe(
          browser_view_layout->views().browser_view->GetWidget());
    }
  }

  BrowserModalDialogHostViews(const BrowserModalDialogHostViews&) = delete;
  BrowserModalDialogHostViews& operator=(const BrowserModalDialogHostViews&) =
      delete;

  ~BrowserModalDialogHostViews() override {
    observer_list_.Notify(&ModalDialogHostObserver::OnHostDestroying);
  }

  void NotifyPositionRequiresUpdate() {
    observer_list_.Notify(&ModalDialogHostObserver::OnPositionRequiresUpdate);
  }

  gfx::Point GetDialogPosition(const gfx::Size& dialog_size) override {
    return browser_view_layout_->GetDialogPosition(dialog_size);
  }

  bool ShouldActivateDialog() const override {
    // The browser Widget may be inactive when showing a bubble, in which case
    // the bubble should be shown active.
    return !browser_view_layout_->views()
                .browser_view->GetWidget()
                ->ShouldPaintAsActive();
  }

  bool ShouldConstrainDialogBoundsByHost() override {
    return !base::FeatureList::IsEnabled(features::kTabModalUsesDesktopWidget);
  }

  gfx::Size GetMaximumDialogSize() override {
    return browser_view_layout_->GetMaximumDialogSize();
  }

  views::Widget* GetHostWidget() const {
    return views::Widget::GetWidgetForNativeView(
        browser_view_layout_->delegate().GetHostViewForAnchoring());
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

// static
std::unique_ptr<BrowserViewLayout> BrowserViewLayout::CreateLayout(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    Browser* browser,
    BrowserViewLayoutViews views) {
  // Browser can be null in unit tests.
  if (browser) {
    if (browser->is_type_normal() &&
        base::FeatureList::IsEnabled(features::kTabbedBrowserUseNewLayout)) {
      return std::make_unique<BrowserViewTabbedLayoutImpl>(
          std::move(delegate), browser, std::move(views));
    } else if (
        // TODO(crbug.com/40639933): have to check both `is_type_app()` and
        // `app_controller()` because "legacy" apps with no controllers lay out
        // like app popups.
        browser->is_type_app() && browser->app_controller() &&
        base::FeatureList::IsEnabled(features::kAppBrowserUseNewLayout)) {
      return std::make_unique<BrowserViewAppLayoutImpl>(
          std::move(delegate), browser, std::move(views));
    } else if ((browser->is_type_popup() || browser->is_type_devtools()) &&
               base::FeatureList::IsEnabled(
                   features::kPopupBrowserUseNewLayout)) {
      return std::make_unique<BrowserViewPopupLayoutImpl>(
          std::move(delegate), browser, std::move(views));
    }
  }
  return std::make_unique<BrowserViewLayoutImplOld>(std::move(delegate),
                                                    browser, std::move(views));
}

BrowserViewLayout::BrowserViewLayout(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    Browser* browser,
    BrowserViewLayoutViews views)
    : delegate_(std::move(delegate)),
      browser_(browser),
      views_(std::move(views)),
      dialog_host_(std::make_unique<BrowserModalDialogHostViews>(this)) {}

BrowserViewLayout::~BrowserViewLayout() = default;

WebContentsModalDialogHost* BrowserViewLayout::GetWebContentsModalDialogHost() {
  return dialog_host_.get();
}

//////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, views::LayoutManager implementation:

void BrowserViewLayout::UpdateBubbles() {
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
      views().contents_container->GetBoundsInScreen();
  if (delegate().HasFindBarController() &&
      (new_contents_bounds.width() != latest_contents_bounds_.width() ||
       (new_contents_bounds.y() != latest_contents_bounds_.y() &&
        new_contents_bounds.height() != latest_contents_bounds_.height()))) {
    delegate().MoveWindowForFindBarIfNecessary();
  }
  latest_contents_bounds_ = new_contents_bounds;

  // Adjust the fullscreen exit bubble bounds for |views().top_container|'s new
  // bounds. This makes the fullscreen exit bubble look like it animates with
  // |views().top_container| in immersive fullscreen.
  ExclusiveAccessBubbleViews* exclusive_access_bubble =
      delegate().GetExclusiveAccessBubble();
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
