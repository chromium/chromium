// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_mac.h"

#include <AppKit/AppKit.h>

#include <vector>

#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/common/chrome_features.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace {

// The width of the traffic lights. Used to layout the tab strip leaving a hole
// for the traffic lights.
// TODO(https://crbug.com/1414521): Get this dynamically. Unfortunately the
// values in BrowserNonClientFrameViewMac::GetCaptionButtonInsets don't account
// for a window with an NSToolbar.
const int kTrafficLightsWidth = 70;

class ImmersiveModeControllerMac : public ImmersiveModeController,
                                   public views::FocusChangeListener,
                                   public views::ViewObserver,
                                   public views::WidgetObserver,
                                   public views::FocusTraversable {
 public:
  class RevealedLock : public ImmersiveRevealedLock {
   public:
    explicit RevealedLock(base::WeakPtr<ImmersiveModeControllerMac> controller);

    RevealedLock(const RevealedLock&) = delete;
    RevealedLock& operator=(const RevealedLock&) = delete;

    ~RevealedLock() override;

   private:
    base::WeakPtr<ImmersiveModeControllerMac> controller_;
  };

  ImmersiveModeControllerMac();

  ImmersiveModeControllerMac(const ImmersiveModeControllerMac&) = delete;
  ImmersiveModeControllerMac& operator=(const ImmersiveModeControllerMac&) =
      delete;

  ~ImmersiveModeControllerMac() override;

  // ImmersiveModeController overrides:
  void Init(BrowserView* browser_view) override;
  void SetEnabled(bool enabled) override;
  bool IsEnabled() const override;
  bool ShouldHideTopViews() const override;
  bool IsRevealed() const override;
  int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const override;
  std::unique_ptr<ImmersiveRevealedLock> GetRevealedLock(
      AnimateReveal animate_reveal) override;
  void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds_in_screen) override;
  bool ShouldStayImmersiveAfterExitingFullscreen() override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // Set the widget id of the tab hosting widget. Set before calling SetEnabled.
  void SetTabNativeWidgetID(uint64_t widget_id);

  // views::FocusChangeListener implementation.
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // views::ViewObserver implementation
  void OnViewBoundsChanged(views::View* observed_view) override;

  // views::WidgetObserver implementation
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::Traversable:
  views::FocusSearch* GetFocusSearch() override;
  views::FocusTraversable* GetFocusTraversableParent() override;
  views::View* GetFocusTraversableParentView() override;

  BrowserView* browser_view() { return browser_view_; }

 private:
  friend class RevealedLock;

  void LockDestroyed();

  // Move children from `from_widget` to `to_widget`. Certain child widgets will
  // be held back from the move, see `ShouldMoveChild` for details.
  void MoveChildren(views::Widget* from_widget, views::Widget* to_widget);

  // Returns true if the child should be moved.
  bool ShouldMoveChild(views::Widget* child);

  raw_ptr<BrowserView> browser_view_ = nullptr;  // weak
  std::unique_ptr<ImmersiveRevealedLock> focus_lock_;
  bool enabled_ = false;
  base::ScopedObservation<views::View, views::ViewObserver>
      top_container_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_frame_observation_{this};

  std::unique_ptr<views::FocusSearch> focus_search_;

  // Used as a convenience to access
  // NativeWidgetMacNSWindowHost::GetNSWindowMojo().
  // Dangling in ImmersiveModeControllerMacBrowserTest.ToggleFullscreen.
  // Dangling when executing DownloadBubbleInteractiveUiTest.* (with Mac
  // immersive mode enabled).
  raw_ptr<remote_cocoa::mojom::NativeWidgetNSWindow, DanglingUntriaged>
      ns_window_mojo_ = nullptr;  // weak

  // Used to hold the widget id for the tab hosting widget. This will be passed
  // to the remote_cocoa immersive mode controller where the tab strip will be
  // placed in the titlebar.
  uint64_t tab_native_widget_id_ = 0;

  base::WeakPtrFactory<ImmersiveModeControllerMac> weak_ptr_factory_;
};

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

}  // namespace

ImmersiveModeControllerMac::RevealedLock::RevealedLock(
    base::WeakPtr<ImmersiveModeControllerMac> controller)
    : controller_(std::move(controller)) {}

ImmersiveModeControllerMac::RevealedLock::~RevealedLock() {
  if (auto* controller = controller_.get())
    controller->LockDestroyed();
}

ImmersiveModeControllerMac::ImmersiveModeControllerMac()
    : weak_ptr_factory_(this) {}

ImmersiveModeControllerMac::~ImmersiveModeControllerMac() {
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void ImmersiveModeControllerMac::Init(BrowserView* browser_view) {
  browser_view_ = browser_view;
  ns_window_mojo_ = views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
                        browser_view_->GetWidget()->GetNativeWindow())
                        ->GetNSWindowMojo();
  focus_search_ = std::make_unique<ImmersiveModeFocusSearchMac>(browser_view);
}

void ImmersiveModeControllerMac::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;
  if (enabled) {
    browser_view_->GetWidget()->GetFocusManager()->AddFocusChangeListener(this);
    top_container_observation_.Observe(browser_view_->top_container());
    browser_frame_observation_.Observe(browser_view_->GetWidget());

    // Capture the overlay content view before enablement. Once enabled the view
    // is moved to an AppKit window leaving us otherwise without a reference.
    NSView* content_view = browser_view_->overlay_widget()
                               ->GetNativeWindow()
                               .GetNativeNSWindow()
                               .contentView;
    browser_view_->overlay_widget()->SetNativeWindowProperty(
        views::NativeWidgetMacNSWindowHost::kMovedContentNSView, content_view);

    // Move the appropriate children from the browser widget to the overlay
    // widget. Make sure to call `Show()` on the overlay widget before enabling
    // immersive fullscreen. The call to `Show()` actually performs the
    // underlying window reparenting.
    MoveChildren(browser_view_->GetWidget(), browser_view_->overlay_widget());

    // `Show()` is needed because the overlay widget's compositor is still being
    // used, even though its content view has been moved to the AppKit
    // controlled fullscreen NSWindow.
    browser_view_->overlay_widget()->Show();

    // Move top chrome to the overlay view.
    browser_view_->OnImmersiveRevealStarted();
    browser_view_->InvalidateLayout();

    views::NativeWidgetMacNSWindowHost* overlay_host =
        views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
            browser_view_->overlay_widget()->GetNativeWindow());
    ns_window_mojo_->EnableImmersiveFullscreen(
        overlay_host->bridged_native_widget_id(), tab_native_widget_id_);

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
  } else {
    browser_view_->GetWidget()->GetFocusManager()->RemoveFocusChangeListener(
        this);
    top_container_observation_.Reset();
    browser_frame_observation_.Reset();
    focus_lock_.reset();

    // Notify BrowserView about the fullscreen exit so that the top container
    // can be reparented, otherwise it might be destroyed along with the
    // overlay widget.
    for (Observer& observer : observers_)
      observer.OnImmersiveFullscreenExited();

    // Rollback the view shuffling from enablement.
    MoveChildren(browser_view_->overlay_widget(), browser_view_->GetWidget());
    browser_view_->overlay_widget()->Hide();
    ns_window_mojo_->DisableImmersiveFullscreen();
    browser_view_->overlay_widget()->SetNativeWindowProperty(
        views::NativeWidgetMacNSWindowHost::kMovedContentNSView, nullptr);

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

bool ImmersiveModeControllerMac::IsEnabled() const {
  return enabled_;
}

bool ImmersiveModeControllerMac::ShouldHideTopViews() const {
  return enabled_ && !IsRevealed();
}

bool ImmersiveModeControllerMac::IsRevealed() const {
  return enabled_;
}

int ImmersiveModeControllerMac::GetTopContainerVerticalOffset(
    const gfx::Size& top_container_size) const {
  return 0;
}

std::unique_ptr<ImmersiveRevealedLock>
ImmersiveModeControllerMac::GetRevealedLock(AnimateReveal animate_reveal) {
  ns_window_mojo_->ImmersiveFullscreenRevealLock();
  return std::make_unique<RevealedLock>(weak_ptr_factory_.GetWeakPtr());
}

void ImmersiveModeControllerMac::OnFindBarVisibleBoundsChanged(
    const gfx::Rect& new_visible_bounds_in_screen) {}

bool ImmersiveModeControllerMac::ShouldStayImmersiveAfterExitingFullscreen() {
  return false;
}

void ImmersiveModeControllerMac::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {}

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
  if (!observed_view->bounds().IsEmpty()) {
    browser_view_->overlay_widget()->SetBounds(observed_view->bounds());
    ns_window_mojo_->OnTopContainerViewBoundsChanged(observed_view->bounds());
  }
}

void ImmersiveModeControllerMac::OnWidgetDestroying(views::Widget* widget) {
  SetEnabled(false);
}

void ImmersiveModeControllerMac::LockDestroyed() {
  ns_window_mojo_->ImmersiveFullscreenRevealUnlock();
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
  for (auto* widget : widgets) {
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

// A derived class of ImmersiveModeControllerMac that peels off the tab strip
// from the top container.
class ImmersiveModeTabbedControllerMac : public ImmersiveModeControllerMac {
 public:
  ImmersiveModeTabbedControllerMac() = default;

  ImmersiveModeTabbedControllerMac(const ImmersiveModeTabbedControllerMac&) =
      delete;
  ImmersiveModeTabbedControllerMac& operator=(
      const ImmersiveModeTabbedControllerMac&) = delete;

  // ImmersiveModeControllerMac overrides:
  void SetEnabled(bool enabled) override;
  void OnViewBoundsChanged(views::View* observed_view) override;

 private:
  int tab_widget_height_ = 0;

  base::ScopedObservation<views::View, views::ViewObserver>
      tab_container_observation_{this};
};

void ImmersiveModeTabbedControllerMac::SetEnabled(bool enabled) {
  if (enabled == IsEnabled()) {
    return;
  }
  BrowserView* browser_view = ImmersiveModeControllerMac::browser_view();
  if (enabled) {
    tab_container_observation_.Observe(browser_view->tab_overlay_view());

    tab_widget_height_ = browser_view->tab_strip_region_view()->height();
    tab_widget_height_ += static_cast<BrowserNonClientFrameViewMac*>(
                              browser_view->frame()->GetFrameView())
                              ->GetTopInset(false);

    browser_view->tab_overlay_widget()->SetBounds(
        gfx::Rect(0, 0, browser_view->top_container()->size().width(),
                  tab_widget_height_));
    browser_view->tab_overlay_widget()->Show();

    // Move the tab strip to the `tab_overlay_widget`, the host of the
    // `tab_overlay_view`.
    browser_view->tab_overlay_view()->AddChildView(
        browser_view->tab_strip_region_view());

    // Inset the start of |tab_strip_region_view()| by |kTrafficLightsWidth|.
    // This will leave a hole for the traffic light to appear.
    // Without this +1 top inset the tabs sit 1px too high. I assume this is
    // because in fullscreen there is no resize handle.
    gfx::Insets insets = gfx::Insets::TLBR(1, kTrafficLightsWidth, 0, 0);
    browser_view->tab_strip_region_view()->SetBorder(
        views::CreateEmptyBorder(insets));

    views::NativeWidgetMacNSWindowHost* tab_overlay_host =
        views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
            browser_view->tab_overlay_widget()->GetNativeWindow());
    SetTabNativeWidgetID(tab_overlay_host->bridged_native_widget_id());
    ImmersiveModeControllerMac::SetEnabled(enabled);
  } else {
    tab_container_observation_.Reset();
    browser_view->tab_overlay_widget()->Hide();
    browser_view->tab_strip_region_view()->SetBorder(nullptr);
    browser_view->top_container()->AddChildViewAt(
        browser_view->tab_strip_region_view(), 0);
    ImmersiveModeControllerMac::SetEnabled(enabled);
  }
}

void ImmersiveModeTabbedControllerMac::OnViewBoundsChanged(
    views::View* observed_view) {
  // Resize the width of |tab_overlay_view()| and |tab_overlay_widget()|.
  BrowserView* browser_view = ImmersiveModeControllerMac::browser_view();
  gfx::Size new_size(observed_view->size().width(), tab_widget_height_);
  browser_view->tab_overlay_widget()->SetSize(new_size);
  browser_view->tab_overlay_view()->SetSize(new_size);
  browser_view->tab_strip_region_view()->SetSize(gfx::Size(
      new_size.width(), browser_view->tab_strip_region_view()->height()));
  ImmersiveModeControllerMac::OnViewBoundsChanged(observed_view);
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
  if (browser_view->UsesImmersiveFullscreenTabbedMode()) {
    return std::make_unique<ImmersiveModeTabbedControllerMac>();
  }
  return std::make_unique<ImmersiveModeControllerMac>();
}
