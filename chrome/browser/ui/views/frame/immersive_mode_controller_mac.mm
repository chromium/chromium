// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_mac.h"

#include <AppKit/AppKit.h>

#include <vector>

#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/fullscreen_util_mac.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/common/chrome_features.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/remote_cocoa/app_shim/features.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/widget/native_widget.h"

namespace {

// The width of the traffic lights. Used to animate the tab strip leaving a hole
// for the traffic lights.
// TODO(crbug.com/40892148): Get this dynamically. Unfortunately the
// values in BrowserNonClientFrameViewMac::GetCaptionButtonInsets don't account
// for a window with an NSToolbar.
constexpr int kTrafficLightsWidth = 62;
constexpr int kTabAlignmentInset = 4;
constexpr base::TimeDelta kTabSlideAnimationDuration = base::Milliseconds(149);

class ImmersiveModeFocusSearchMac : public views::FocusSearch {
 public:
  explicit ImmersiveModeFocusSearchMac(BrowserView* browser_view);
  ImmersiveModeFocusSearchMac(const ImmersiveModeFocusSearchMac&) = delete;
  ImmersiveModeFocusSearchMac& operator=(const ImmersiveModeFocusSearchMac&) =
      delete;
  ~ImmersiveModeFocusSearchMac() override;

  // views::FocusSearch:
  views::View* FindNextFocusableView(
      views::View* starting_view,
      SearchDirection search_direction,
      TraversalDirection traversal_direction,
      StartingViewPolicy check_starting_view,
      AnchoredDialogPolicy can_go_into_anchored_dialog,
      views::FocusTraversable** focus_traversable,
      views::View** focus_traversable_view) override;

 private:
  raw_ptr<BrowserView> browser_view_;
};

bool ShouldAnimateTabs() {
  return base::FeatureList::IsEnabled(features::kFullscreenAnimateTabs) &&
         !base::FeatureList::IsEnabled(
             remote_cocoa::features::kFullscreenAlwaysShowTrafficLights);
}

}  // namespace

ImmersiveModeControllerMac::RevealedLock::RevealedLock(
    base::WeakPtr<ImmersiveModeControllerMac> controller)
    : controller_(std::move(controller)) {}

ImmersiveModeControllerMac::RevealedLock::~RevealedLock() {
  if (auto* controller = controller_.get())
    controller->LockDestroyed();
}

ImmersiveModeControllerMac::ImmersiveModeControllerMac(bool separate_tab_strip)
    : separate_tab_strip_(separate_tab_strip), weak_ptr_factory_(this) {}

ImmersiveModeControllerMac::~ImmersiveModeControllerMac() {
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void ImmersiveModeControllerMac::Init(BrowserView* browser_view) {
  browser_view_ = browser_view;
  focus_search_ = std::make_unique<ImmersiveModeFocusSearchMac>(browser_view);
}

void ImmersiveModeControllerMac::SetEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }
  enabled_ = enabled;
  if (enabled) {
    if (separate_tab_strip_) {
      tab_widget_height_ = browser_view_->tab_strip_region_view()->height();
      tab_widget_height_ += static_cast<BrowserNonClientFrameViewMac*>(
                                browser_view_->frame()->GetFrameView())
                                ->GetTopInset(false);

      browser_view_->tab_overlay_widget()->SetSize(gfx::Size(
          browser_view_->top_container()->size().width(), tab_widget_height_));
      browser_view_->tab_overlay_widget()->Show();

      // Move the tab strip to the `tab_overlay_widget`, the host of the
      // `tab_overlay_view`.
      browser_view_->tab_overlay_view()->AddChildView(
          browser_view_->tab_strip_region_view());

      browser_view_->tab_strip_region_view()->SetBorder(
          views::CreateEmptyBorder(GetTabStripRegionViewInsets()));

      views::NativeWidgetMacNSWindowHost* tab_overlay_host =
          views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
              browser_view_->tab_overlay_widget()->GetNativeWindow());
      SetTabNativeWidgetID(tab_overlay_host->bridged_native_widget_id());
    }
    top_container_observation_.Observe(browser_view_->top_container());
    browser_frame_observation_.Observe(browser_view_->GetWidget());
    overlay_widget_observation_.Observe(browser_view_->overlay_widget());

    // Capture the overlay content view before enablement. Once enabled the view
    // is moved to an AppKit window leaving us otherwise without a reference.
    NSView* content_view = browser_view_->overlay_widget()
                               ->GetNativeWindow()
                               .GetNativeNSWindow()
                               .contentView;
    browser_view_->overlay_widget()->SetNativeWindowProperty(
        views::NativeWidgetMacNSWindowHost::kMovedContentNSView,
        (__bridge void*)content_view);

    views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
        browser_view_->GetWidget()->GetNativeWindow())
        ->set_immersive_mode_reveal_client(this);

    // Move the appropriate children from the browser widget to the overlay
    // widget, unless we are entering content fullscreen. Make sure to call
    // `Show()` on the overlay widget before enabling immersive fullscreen. The
    // call to `Show()` actually performs the underlying window reparenting.
    if (!fullscreen_utils::IsInContentFullscreen(browser_view_->browser())) {
      MoveChildren(browser_view_->GetWidget(), browser_view_->overlay_widget());
    }

    // `Show()` is needed because the overlay widget's compositor is still being
    // used, even though its content view has been moved to the AppKit
    // controlled fullscreen NSWindow.
    browser_view_->overlay_widget()->Show();

    // Set revealed to be true when entering immersive fullscreen so the toolbar
    // and bookmarks bar heights are accounted for during the fullscreen
    // transition.
    OnImmersiveModeToolbarRevealChanged(true);

    // Move top chrome to the overlay view.
    browser_view_->OnImmersiveRevealStarted();
    browser_view_->InvalidateLayout();

    views::NativeWidgetMacNSWindowHost* overlay_host =
        views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
            browser_view_->overlay_widget()->GetNativeWindow());
    if (auto* window = GetNSWindowMojo()) {
      window->EnableImmersiveFullscreen(
          overlay_host->bridged_native_widget_id(), tab_native_widget_id_);
    }
    browser_view_->GetWidget()->GetFocusManager()->AddFocusChangeListener(this);
    // Set up a root FocusTraversable that handles focus cycles between overlay
    // widgets and the browser widget.
    browser_view_->GetWidget()->SetFocusTraversableParent(this);
    browser_view_->GetWidget()->SetFocusTraversableParentView(browser_view_);
    browser_view_->overlay_widget()->SetFocusTraversableParent(this);
    browser_view_->overlay_widget()->SetFocusTraversableParentView(
        browser_view_->overlay_view());
    if (browser_view_->tab_overlay_widget()) {
      browser_view_->tab_overlay_widget()->SetFocusTraversableParent(this);
      browser_view_->tab_overlay_widget()->SetFocusTraversableParentView(
          browser_view_->tab_overlay_view());
    }

    // If the window is maximized OnViewBoundsChanged will not be called
    // when transitioning to full screen. Call it now.
    OnViewBoundsChanged(browser_view_->top_container());

    if (separate_tab_strip_) {
      tab_bounds_animator_ = std::make_unique<views::BoundsAnimator>(
          browser_view_->tab_overlay_view(), false);
      tab_bounds_animator_->SetAnimationDuration(kTabSlideAnimationDuration);
    }
  } else {
    if (separate_tab_strip_) {
      tab_bounds_animator_.reset();
      browser_view_->tab_overlay_widget()->Hide();
      browser_view_->tab_strip_region_view()->SetBorder(nullptr);
      browser_view_->top_container()->AddChildViewAt(
          browser_view_->tab_strip_region_view(), 0);
    }
    top_container_observation_.Reset();
    browser_frame_observation_.Reset();
    overlay_widget_observation_.Reset();

    // Notify BrowserView about the fullscreen exit so that the top container
    // can be reparented, otherwise it might be destroyed along with the
    // overlay widget.
    for (Observer& observer : observers_)
      observer.OnImmersiveFullscreenExited();

    // Rollback the view shuffling from enablement.
    MoveChildren(browser_view_->overlay_widget(), browser_view_->GetWidget());
    browser_view_->overlay_widget()->Hide();
    if (auto* window = GetNSWindowMojo()) {
      window->DisableImmersiveFullscreen();
    }
    browser_view_->overlay_widget()->SetNativeWindowProperty(
        views::NativeWidgetMacNSWindowHost::kMovedContentNSView, nullptr);

    browser_view_->GetWidget()->GetFocusManager()->RemoveFocusChangeListener(
        this);
    focus_lock_.reset();
    // Remove the root FocusTraversable.
    browser_view_->GetWidget()->SetFocusTraversableParent(nullptr);
    browser_view_->GetWidget()->SetFocusTraversableParentView(nullptr);
    browser_view_->overlay_widget()->SetFocusTraversableParent(nullptr);
    browser_view_->overlay_widget()->SetFocusTraversableParentView(nullptr);
    if (browser_view_->tab_overlay_widget()) {
      browser_view_->tab_overlay_widget()->SetFocusTraversableParent(nullptr);
      browser_view_->tab_overlay_widget()->SetFocusTraversableParentView(
          nullptr);
    }
  }
}

gfx::Insets ImmersiveModeControllerMac::GetTabStripRegionViewInsets() {
  // Inset the start of `tab_strip_region_view` by `kTabAlignmentInset` +
  // `kTrafficLightsWidth`. This leaves a hole for the traffic lights to appear.
  // When tab animation is enabled, only inset by `kTabAlignmentInset`, this
  // keeps the tab strip aligned with the toolbar. The tab strip will slide out
  // of the way when the traffic lights appear.
  int right_left_inset = ShouldAnimateTabs()
                             ? kTabAlignmentInset
                             : kTabAlignmentInset + kTrafficLightsWidth;

  // Without this +1 top inset the tabs sit 1px too high. I assume this is
  // because in fullscreen there is no resize handle.
  return browser_view_->frame()->GetFrameView()->CaptionButtonsOnLeadingEdge()
             ? gfx::Insets::TLBR(1, right_left_inset, 0, 0)
             : gfx::Insets::TLBR(1, 0, 0, right_left_inset);
}

bool ImmersiveModeControllerMac::IsEnabled() const {
  return enabled_;
}

bool ImmersiveModeControllerMac::ShouldHideTopViews() const {
  // Always return false to ensure the top UI is pre-rendered and ready for
  // display. We don't have full control over the visibility of the top UI. For
  // instance, in auto-hide mode, the top UI is revealed when the user hovers
  // over the screen's upper border. Notifications about this visibility change
  // arrive only after the UI is already displayed, so it's crucial to have the
  // top UI fully rendered by then.
  return false;
}

bool ImmersiveModeControllerMac::IsRevealed() const {
  return enabled_ && is_revealed_;
}

int ImmersiveModeControllerMac::GetTopContainerVerticalOffset(
    const gfx::Size& top_container_size) const {
  return 0;
}

std::unique_ptr<ImmersiveRevealedLock>
ImmersiveModeControllerMac::GetRevealedLock(AnimateReveal animate_reveal) {
  if (auto* window = GetNSWindowMojo()) {
    window->ImmersiveFullscreenRevealLock();
  }
  return std::make_unique<RevealedLock>(weak_ptr_factory_.GetWeakPtr());
}

void ImmersiveModeControllerMac::OnFindBarVisibleBoundsChanged(
    const gfx::Rect& new_visible_bounds_in_screen) {
  bool was_visible =
      std::exchange(find_bar_visible_, !new_visible_bounds_in_screen.IsEmpty());
  if (enabled_ && was_visible != find_bar_visible_) {
    // Ensure web content is fully visible if find bar is showing.
    browser_view_->InvalidateLayout();
  }
}

bool ImmersiveModeControllerMac::ShouldStayImmersiveAfterExitingFullscreen() {
  return false;
}

void ImmersiveModeControllerMac::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {}

int ImmersiveModeControllerMac::GetMinimumContentOffset() const {
  if (find_bar_visible_ &&
      !fullscreen_utils::IsAlwaysShowToolbarEnabled(browser_view_->browser()) &&
      !fullscreen_utils::IsInContentFullscreen(browser_view_->browser())) {
    return overlay_height_;
  }
  return 0;
}

int ImmersiveModeControllerMac::GetExtraInfobarOffset() const {
  if (fullscreen_utils::IsInContentFullscreen(browser_view_->browser())) {
    return 0;
  }
  if (fullscreen_utils::IsAlwaysShowToolbarEnabled(browser_view_->browser())) {
    return reveal_amount_ * menu_bar_height_;
  }
  return reveal_amount_ * (menu_bar_height_ + overlay_height_);
}

void ImmersiveModeControllerMac::OnContentFullscreenChanged(
    bool is_content_fullscreen) {
  // Ignore this call if we are not in browser fullscreen.
  if (!IsEnabled()) {
    return;
  }

  if (is_content_fullscreen) {
    // When in content fullscreen the overlay widget is not displayed. Move all
    // the child widgets from the overlay widget to the browser widget. This is
    // particularly important for sticky children like the find bar or
    // permission dialogs.
    MoveChildren(browser_view_->overlay_widget(), browser_view_->GetWidget());
  } else {
    // Put the children back when transitioning from content fullscreen back to
    // browser fullscreen.
    MoveChildren(browser_view_->GetWidget(), browser_view_->overlay_widget());
  }
}

void ImmersiveModeControllerMac::OnWillChangeFocus(views::View* focused_before,
                                                   views::View* focused_now) {}

void ImmersiveModeControllerMac::OnDidChangeFocus(views::View* focused_before,
                                                  views::View* focused_now) {
  if (browser_view_->top_container()->Contains(focused_now) ||
      browser_view_->tab_overlay_view()->Contains(focused_now)) {
    if (!focus_lock_)
      focus_lock_ = GetRevealedLock(ANIMATE_REVEAL_NO);
  } else {
    focus_lock_.reset();
  }
}

void ImmersiveModeControllerMac::OnViewBoundsChanged(
    views::View* observed_view) {
  gfx::Rect bounds = observed_view->bounds();
  if (bounds.IsEmpty()) {
    return;
  }
  overlay_height_ = bounds.height();
  if (separate_tab_strip_) {
    gfx::Size new_size(bounds.width(), tab_widget_height_);
    browser_view_->tab_overlay_widget()->SetSize(new_size);
    browser_view_->tab_overlay_view()->SetSize(new_size);
    browser_view_->tab_strip_region_view()->SetSize(gfx::Size(
        new_size.width(), browser_view_->tab_strip_region_view()->height()));
    overlay_height_ += tab_widget_height_;
  }
  browser_view_->overlay_widget()->SetSize(bounds.size());
  if (auto* window = GetNSWindowMojo()) {
    window->OnTopContainerViewBoundsChanged(bounds);
  }
}

void ImmersiveModeControllerMac::OnWidgetDestroying(views::Widget* widget) {
  SetEnabled(false);
}

void ImmersiveModeControllerMac::LockDestroyed() {
  if (auto* window = GetNSWindowMojo()) {
    window->ImmersiveFullscreenRevealUnlock();
  }
}

void ImmersiveModeControllerMac::SetTabNativeWidgetID(uint64_t widget_id) {
  tab_native_widget_id_ = widget_id;
}

void ImmersiveModeControllerMac::MoveChildren(views::Widget* from_widget,
                                              views::Widget* to_widget) {
  CHECK(from_widget && to_widget);

  // If the browser window is closing the native view is removed. Don't attempt
  // to move children.
  if (!from_widget->GetNativeView() || !to_widget->GetNativeView()) {
    return;
  }

  views::Widget::Widgets widgets;
  views::Widget::GetAllChildWidgets(from_widget->GetNativeView(), &widgets);
  for (views::Widget* widget : widgets) {
    if (ShouldMoveChild(widget)) {
      views::Widget::ReparentNativeView(widget->GetNativeView(),
                                        to_widget->GetNativeView());
    }
  }
}

bool ImmersiveModeControllerMac::ShouldMoveChild(views::Widget* child) {
  // Filter out widgets that should not be reparented.
  // The browser, overlay and tab overlay widgets all stay put.
  if (child == browser_view_->GetWidget() ||
      child == browser_view_->overlay_widget() ||
      child == browser_view_->tab_overlay_widget()) {
    return false;
  }

  // The find bar should be reparented if it exists.
  if (browser_view_->browser()->HasFindBarController()) {
    FindBarController* find_bar_controller =
        browser_view_->browser()->GetFindBarController();
    if (child == find_bar_controller->find_bar()->GetHostWidget()) {
      return true;
    }
  }

  const void* widget_identifier =
      child->GetNativeWindowProperty(views::kWidgetIdentifierKey);
  if (widget_identifier ==
          constrained_window::kConstrainedWindowWidgetIdentifier ||
      widget_identifier == kLensOverlayPreselectionWidgetIdentifier) {
    return true;
  }

  // Widgets that have an anchor view contained within top chrome should be
  // reparented.
  views::WidgetDelegate* widget_delegate = child->widget_delegate();
  if (!widget_delegate) {
    return false;
  }
  views::BubbleDialogDelegate* bubble_dialog =
      widget_delegate->AsBubbleDialogDelegate();
  if (!bubble_dialog) {
    return false;
  }
  // Both `top_container` and `tab_strip_region_view` are checked individually
  // because `tab_strip_region_view` is pulled out of `top_container` to be
  // displayed in the titlebar.
  views::View* anchor_view = bubble_dialog->GetAnchorView();
  if (anchor_view &&
      (browser_view_->top_container()->Contains(anchor_view) ||
       browser_view_->tab_strip_region_view()->Contains(anchor_view))) {
    return true;
  }

  // All other widgets will stay put.
  return false;
}

void ImmersiveModeControllerMac::OnImmersiveModeToolbarRevealChanged(
    bool is_revealed) {
  is_revealed_ = is_revealed;
}

void ImmersiveModeControllerMac::OnImmersiveModeMenuBarRevealChanged(
    double reveal_amount) {
  bool should_shift_tabs = reveal_amount == 1 || reveal_amount > reveal_amount_;
  reveal_amount_ = reveal_amount;
  if (!browser_view_->infobar_container()->IsEmpty()) {
    browser_view_->InvalidateLayout();
  }

  if (!ShouldAnimateTabs() || !tab_bounds_animator_.get()) {
    return;
  }

  if (should_shift_tabs) {
    tab_bounds_animator_->AnimateViewTo(
        browser_view_->tab_strip_region_view(),
        gfx::Rect(kTrafficLightsWidth, 0,
                  browser_view_->tab_overlay_view()->size().width() -
                      kTrafficLightsWidth,
                  browser_view_->tab_strip_region_view()->height()));
  } else {
    tab_bounds_animator_->AnimateViewTo(
        browser_view_->tab_strip_region_view(),
        gfx::Rect(0, 0, browser_view_->tab_overlay_view()->size().width(),
                  browser_view_->tab_strip_region_view()->height()));
  }
}

void ImmersiveModeControllerMac::OnAutohidingMenuBarHeightChanged(
    int menu_bar_height) {
  menu_bar_height_ = menu_bar_height;
  if (!browser_view_->infobar_container()->IsEmpty()) {
    browser_view_->InvalidateLayout();
  }
}

views::FocusSearch* ImmersiveModeControllerMac::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable*
ImmersiveModeControllerMac::GetFocusTraversableParent() {
  return nullptr;
}

views::View* ImmersiveModeControllerMac::GetFocusTraversableParentView() {
  return nullptr;
}

remote_cocoa::mojom::NativeWidgetNSWindow*
ImmersiveModeControllerMac::GetNSWindowMojo() {
  return views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
             browser_view_->GetWidget()->GetNativeWindow())
      ->GetNSWindowMojo();
}

ImmersiveModeFocusSearchMac::ImmersiveModeFocusSearchMac(
    BrowserView* browser_view)
    : views::FocusSearch(browser_view, true, true),
      browser_view_(browser_view) {}

ImmersiveModeFocusSearchMac::~ImmersiveModeFocusSearchMac() = default;

views::View* ImmersiveModeFocusSearchMac::FindNextFocusableView(
    views::View* starting_view,
    SearchDirection search_direction,
    TraversalDirection traversal_direction,
    StartingViewPolicy check_starting_view,
    AnchoredDialogPolicy can_go_into_anchored_dialog,
    views::FocusTraversable** focus_traversable,
    views::View** focus_traversable_view) {
  // Search in the `starting_view` traversable tree.
  views::FocusTraversable* starting_focus_traversable =
      starting_view->GetFocusTraversable();
  if (!starting_focus_traversable) {
    starting_focus_traversable =
        starting_view->GetWidget()->GetFocusTraversable();
  }

  views::View* v =
      starting_focus_traversable->GetFocusSearch()->FindNextFocusableView(
          starting_view, search_direction, traversal_direction,
          check_starting_view, can_go_into_anchored_dialog, focus_traversable,
          focus_traversable_view);

  if (v) {
    return v;
  }

  // If no next focusable view in the `starting_view` traversable tree,
  // jumps to the next widget.
  views::FocusManager* focus_manager =
      browser_view_->GetWidget()->GetFocusManager();

  // The focus cycles between overlay widget(s) and the browser widget.
  std::vector<views::Widget*> traverse_order = {browser_view_->overlay_widget(),
                                                browser_view_->GetWidget()};
  if (browser_view_->tab_overlay_widget()) {
    traverse_order.push_back(browser_view_->tab_overlay_widget());
  }

  auto current_widget_it = base::ranges::find_if(
      traverse_order, [starting_view](const views::Widget* widget) {
        return widget->GetRootView()->Contains(starting_view);
      });
  CHECK(current_widget_it != traverse_order.end());
  int current_widget_ind = current_widget_it - traverse_order.begin();

  bool reverse = search_direction == SearchDirection::kBackwards;
  int next_widget_ind =
      (current_widget_ind + (reverse ? -1 : 1) + traverse_order.size()) %
      traverse_order.size();
  return focus_manager->GetNextFocusableView(
      nullptr, traverse_order[next_widget_ind], reverse, true);
}

std::unique_ptr<ImmersiveModeController> CreateImmersiveModeControllerMac(
    const BrowserView* browser_view) {
  return std::make_unique<ImmersiveModeControllerMac>(
      /*separate_tab_strip=*/browser_view->UsesImmersiveFullscreenTabbedMode());
}

ImmersiveModeOverlayWidgetObserver::ImmersiveModeOverlayWidgetObserver(
    ImmersiveModeControllerMac* controller)
    : controller_(controller) {}

ImmersiveModeOverlayWidgetObserver::~ImmersiveModeOverlayWidgetObserver() =
    default;

void ImmersiveModeOverlayWidgetObserver::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  // Update web dialog position when the overlay widget moves by invalidating
  // the browse view layout.
  controller_->browser_view()->InvalidateLayout();
}
