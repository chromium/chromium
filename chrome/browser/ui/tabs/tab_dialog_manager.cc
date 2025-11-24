// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/base_window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace constrained_window {
extern const void* kConstrainedWindowWidgetIdentifier;
}  // namespace constrained_window

namespace tabs {

class TabDialogWidgetObserver : public views::WidgetObserver {
 public:
  TabDialogWidgetObserver(TabDialogManager* tab_dialog_manager,
                          views::Widget* widget);
  TabDialogWidgetObserver(const TabDialogWidgetObserver&) = delete;
  TabDialogWidgetObserver& operator=(const TabDialogWidgetObserver&) = delete;
  ~TabDialogWidgetObserver() override = default;

 private:
  // Overridden from WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

  raw_ptr<TabDialogManager> tab_dialog_manager_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      tab_dialog_scoped_observation_{this};
};

TabDialogWidgetObserver::TabDialogWidgetObserver(
    TabDialogManager* tab_dialog_manager,
    views::Widget* widget)
    : tab_dialog_manager_(tab_dialog_manager) {
  tab_dialog_scoped_observation_.Observe(widget);
}

void TabDialogWidgetObserver::OnWidgetDestroyed(views::Widget* widget) {
  tab_dialog_scoped_observation_.Reset();
  tab_dialog_manager_->WidgetDestroyed(widget);
}

namespace {

bool SupportsGlobalScreenCoordinates() {
#if !BUILDFLAG(IS_OZONE)
  return true;
#else
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .supports_global_screen_coordinates;
#endif
}

bool PlatformClipsChildrenToViewport() {
#if BUILDFLAG(IS_LINUX)
  return true;
#else
  return false;
#endif
}

gfx::Rect GetModalDialogBounds(views::Widget* widget,
                               TabInterface* tab_interface,
                               const gfx::Size& size) {
  BrowserWindowInterface* const host_browser_window =
      tab_interface->GetBrowserWindowInterface();
  gfx::Point position =
      host_browser_window->GetWebContentsModalDialogHostForTab(tab_interface)
          ->GetDialogPosition(size);

  if (widget->non_client_view()) {
    // Align the first row of pixels inside the border. This is the apparent top
    // of the dialog.
    position.set_y(position.y() -
                   widget->non_client_view()->frame_view()->GetInsets().top());
  }

  gfx::Rect dialog_bounds(position, size);

  if (widget->is_top_level() && SupportsGlobalScreenCoordinates()) {
    views::Widget* const host_widget =
        BrowserElementsViews::From(host_browser_window)
            ->GetPrimaryWindowWidget();
    gfx::Rect dialog_screen_bounds =
        dialog_bounds +
        host_widget->GetClientAreaBoundsInScreen().OffsetFromOrigin();
    const gfx::Rect host_screen_bounds = host_widget->GetWindowBoundsInScreen();

    // The requested dialog bounds should never fall outside the bounds of the
    // transient parent.
    DCHECK(dialog_screen_bounds.Intersects(host_screen_bounds));

    // Adjust the dialog bound to ensure it remains visible on the display.
    const gfx::Rect display_work_area =
        host_widget->GetNearestDisplay().value().work_area();
    if (!display_work_area.Contains(dialog_screen_bounds)) {
      dialog_screen_bounds.AdjustToFit(display_work_area);
    }

    // For platforms that clip transient children to the viewport we must
    // maximize its bounds on the display whilst keeping it within the host
    // bounds to avoid viewport clipping.
    // In the case that the host window bounds do not have sufficient overlap
    // with the display, and the dialog cannot be shown in its entirety, this is
    // a recoverable state as users are still able to reposition the host window
    // back onto the display.
    if (PlatformClipsChildrenToViewport() &&
        !host_screen_bounds.Contains(dialog_screen_bounds)) {
      dialog_screen_bounds.AdjustToFit(host_screen_bounds);
    }

    // Readjust the position of the dialog.
    dialog_bounds.set_origin(dialog_screen_bounds.origin());
  }
  return dialog_bounds;
}

void ConfigureDesiredBoundsDelegate(views::Widget* widget,
                                    TabInterface* tab_interface) {
  views::WidgetDelegate* delegate = widget->widget_delegate();
  // This callback is invoked in two cases:
  // 1. by auto-resizing (Widget::is_autosized()) widgets when the layout is
  // invalidated.
  // 2. by BubbleDialogDelegate::SizeToContents().
  delegate->set_desired_bounds_delegate(base::BindRepeating(
      [](views::Widget* widget, TabInterface* tab_interface) -> gfx::Rect {
        return GetModalDialogBounds(
            widget, tab_interface, widget->GetRootView()->GetPreferredSize({}));
      },
      widget, tab_interface));
}

// The dialog widget should be visible if and only if the tab is in the
// foreground and activated, the host window is not minimized and the client
// also indicates visibility.
bool GetWidgetVisibility(
    bool activated,
    bool minimized,
    TabDialogManager::ShouldShowCallback& should_show_callback) {
  bool should_show = true;
  if (activated && !minimized && should_show_callback) {
    should_show_callback.Run(should_show);
  }
  return activated && !minimized && should_show;
}

}  // namespace

// Applies positioning changes from the browser window widget to the tracked
// Widget. This class relies on the assumption that it is scoped to the lifetime
// of a single tab, in a single browser, and that it will be destroyed
// before the tab moves between browser windows.
class TabDialogManager::BrowserWindowWidgetObserver
    : public views::WidgetObserver {
 public:
  BrowserWindowWidgetObserver(TabDialogManager* tab_dialog_manager,
                              TabInterface* tab_interface,
                              views::Widget* dialog_widget)
      : tab_dialog_manager_(tab_dialog_manager),
        tab_(tab_interface),
        dialog_widget_(dialog_widget) {
    CHECK(dialog_widget_);
    browser_window_widget_observation_.Observe(
        tab_dialog_manager_->GetHostWidget());
  }
  BrowserWindowWidgetObserver(const BrowserWindowWidgetObserver&) = delete;
  BrowserWindowWidgetObserver& operator=(const BrowserWindowWidgetObserver&) =
      delete;
  ~BrowserWindowWidgetObserver() override = default;

  // WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    if (dialog_widget_->IsVisible()) {
      tab_dialog_manager_->UpdateModalDialogBounds();
    }
  }

  void OnWidgetShowStateChanged(views::Widget* widget) override {
    bool minimized = widget->IsMinimized();
    bool activated = tab_->IsActivated();
    auto* tab_dialog_manager = tab_->GetTabFeatures()->tab_dialog_manager();
    dialog_widget_->SetVisible(
        GetWidgetVisibility(activated, minimized,
                            tab_dialog_manager->params_->should_show_callback));
  }

 private:
  const raw_ptr<TabDialogManager> tab_dialog_manager_;

  // The tab that owns this dialog manager.
  raw_ptr<TabInterface> tab_;

  // The widget being tracked.
  raw_ptr<views::Widget> dialog_widget_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_window_widget_observation_{this};
};

class TabDialogManager::WebContentsModalDialogHostObserver
    : public web_modal::ModalDialogHostObserver {
 public:
  WebContentsModalDialogHostObserver(TabDialogManager* tab_dialog_manager,
                                     TabInterface* tab_interface)
      : tab_dialog_manager_(tab_dialog_manager), tab_(tab_interface) {
    UpdateModalDialogHost();
  }

  WebContentsModalDialogHostObserver(
      const WebContentsModalDialogHostObserver&) = delete;
  WebContentsModalDialogHostObserver& operator=(
      const WebContentsModalDialogHostObserver&) = delete;
  ~WebContentsModalDialogHostObserver() override = default;

  void UpdateModalDialogHost() {
    model_dialog_host_observation_.Reset();
    BrowserWindowInterface* const host_browser_window =
        tab_->GetBrowserWindowInterface();
    web_modal::WebContentsModalDialogHost* dialog_host =
        host_browser_window->GetWebContentsModalDialogHostForTab(tab_);
    model_dialog_host_observation_.Observe(dialog_host);
  }

  // web_modal::ModalDialogHostObserver:
  void OnPositionRequiresUpdate() override {
    tab_dialog_manager_->UpdateModalDialogBounds();
  }

  void OnHostDestroying() override { model_dialog_host_observation_.Reset(); }

 private:
  const raw_ptr<TabDialogManager> tab_dialog_manager_;

  // The tab that owns this dialog manager.
  raw_ptr<TabInterface> tab_;

  base::ScopedObservation<web_modal::WebContentsModalDialogHost,
                          web_modal::ModalDialogHostObserver>
      model_dialog_host_observation_{this};
};

TabDialogManager::Params::Params() = default;

TabDialogManager::Params::~Params() = default;

TabDialogManager::TabDialogManager(TabInterface* tab_interface)
    : content::WebContentsObserver(tab_interface->GetContents()),
      tab_interface_(tab_interface) {
  tab_subscriptions_.push_back(
      tab_interface_->RegisterDidBecomeVisible(base::BindRepeating(
          &TabDialogManager::TabDidEnterForeground, base::Unretained(this))));
  tab_subscriptions_.push_back(
      tab_interface_->RegisterWillBecomeHidden(base::BindRepeating(
          &TabDialogManager::TabWillEnterBackground, base::Unretained(this))));
  tab_subscriptions_.push_back(
      tab_interface_->RegisterWillDetach(base::BindRepeating(
          &TabDialogManager::TabWillDetach, base::Unretained(this))));
}

TabDialogManager::~TabDialogManager() = default;

std::unique_ptr<views::Widget> TabDialogManager::CreateTabScopedDialog(
    views::DialogDelegate* delegate) {
  DCHECK_EQ(ui::mojom::ModalType::kChild, delegate->GetModalType());
  views::Widget* host = GetHostWidget();
  CHECK(host);

  if (base::FeatureList::IsEnabled(features::kTabModalUsesDesktopWidget)) {
    delegate->set_use_desktop_widget_override(true);
  }

  return base::WrapUnique(views::DialogDelegate::CreateDialogWidget(
      delegate, gfx::NativeWindow(), host->GetNativeView()));
}

void TabDialogManager::ShowDialog(views::Widget* widget,
                                  std::unique_ptr<Params> params) {
  // An autosized widget handles its own bounds, while `animated` bounds changes
  // are handled by `TabDialogManager` and not the widget. They are not
  // compatible.
  // TODO(crbug.com/427759111): allow animated autosizing widgets.
  CHECK(!(params->animated && widget->is_autosized()))
      << "Animated widgets are not compatible with autosized.";

  if (params_ && !params_->block_new_modal && widget_) {
    CloseDialog();
  }
  widget_ = widget;
  params_ = std::move(params);
  if (!params_->get_dialog_bounds) {
    ConfigureDesiredBoundsDelegate(widget_.get(), tab_interface_);
  }
  UpdateModalDialogBounds();
  widget_->SetNativeWindowProperty(
      views::kWidgetIdentifierKey,
      const_cast<void*>(
          constrained_window::kConstrainedWindowWidgetIdentifier));
  if (params_->disable_input) {
    scoped_ignore_input_events_ =
        tab_interface_->GetContents()->IgnoreInputEvents(std::nullopt);
    tab_interface_->GetBrowserWindowInterface()
        ->capabilities()
        ->SetWebContentsBlocked(tab_interface_->GetContents(),
                                /*blocked=*/true);
  }
  tab_dialog_widget_observer_ =
      std::make_unique<TabDialogWidgetObserver>(this, widget_.get());
  if (params_->block_new_modal) {
    showing_modal_ui_ = tab_interface_->ShowModalUI();
  }

  if (base::FeatureList::IsEnabled(features::kSideBySide)) {
    web_contents_modal_dialog_host_observer_ =
        std::make_unique<WebContentsModalDialogHostObserver>(this,
                                                             tab_interface_);
  } else {
    browser_window_widget_observer_ =
        std::make_unique<BrowserWindowWidgetObserver>(this, tab_interface_,
                                                      widget_.get());
  }

  if (params_->should_show_inactive) {
    widget_->ShowInactive();
  } else {
    widget_->Show();
  }
  UpdateDialogVisibility();
}

std::unique_ptr<views::Widget> TabDialogManager::CreateAndShowDialog(
    views::DialogDelegate* delegate,
    std::unique_ptr<Params> params) {
  auto widget = CreateTabScopedDialog(delegate);
  ShowDialog(widget.get(), std::move(params));
  return widget;
}

void TabDialogManager::CloseDialog() {
  if (widget_) {
    views::Widget* widget = widget_;

    // First reset all state tracked by this class.
    WidgetDestroyed(widget_);

    // Now destroy the Widget. We don't know whether destruction will be
    // synchronous or asynchronous, but we no longer hold any state at this
    // point so it doesn't matter.
    widget->Close();
  }
}

bool TabDialogManager::MaybeActivateDialog() {
  // Also test whether the widget is in the closed state and in the middle of
  // being torn down (Widget::CloseNow() or Widget::Close() called)
  if (!widget_ || widget_->IsClosed()) {
    return false;
  }

  if (UpdateDialogVisibility()) {
    widget_->Activate();
    return true;
  }

  return false;
}

void TabDialogManager::WidgetDestroyed(views::Widget* widget) {
  CHECK_EQ(widget, widget_.get());
  widget_ = nullptr;
  params_.reset();
  tab_dialog_widget_observer_.reset();
  scoped_ignore_input_events_.reset();
  browser_window_widget_observer_.reset();
  web_contents_modal_dialog_host_observer_.reset();
  bounds_animation_.reset();
  tab_interface_->GetBrowserWindowInterface()
      ->capabilities()
      ->SetWebContentsBlocked(tab_interface_->GetContents(), /*blocked=*/false);
  // Resetting ScopedTabModalUI may cause the showing of a new dialog.
  // Leaving it at the end of the function to prevent its side effects
  // from being overridden.
  showing_modal_ui_.reset();
}

views::Widget* TabDialogManager::GetHostWidget() const {
  return BrowserElementsViews::From(tab_interface_->GetBrowserWindowInterface())
      ->GetPrimaryWindowWidget();
}

void TabDialogManager::UpdateModalDialogBounds() {
  if (bounds_animation_) {
    bounds_animation_->Stop();
  }

  if (!widget_) {
    return;
  }

  // Do not forcibly update the dialog widget position if it is being dragged.
  if (widget_->HasCapture()) {
    return;
  }

  auto* host_widget = GetHostWidget();
  const gfx::Size size = widget_->GetRootView()->GetPreferredSize({});

  if (!host_widget) {
    widget_->SetSize(size);
    return;
  }

  // If the host view's widget is minimized, don't update any positions.
  if (host_widget->IsMinimized()) {
    return;
  }

  gfx::Rect target_bounds;
  if (params_->get_dialog_bounds) {
    target_bounds = params_->get_dialog_bounds.Run();
  } else {
    target_bounds = GetModalDialogBounds(widget_.get(), tab_interface_, size);
  }

  if (params_->animated && gfx::Animation::ShouldRenderRichAnimation() &&
      widget_->IsVisible()) {
    if (!bounds_animation_) {
      bounds_animation_ = std::make_unique<gfx::LinearAnimation>(this);
      bounds_animation_->SetDuration(
          gfx::Animation::RichAnimationDuration(base::Milliseconds(120)));
    }
    animation_start_bounds_ = widget_->GetWindowBoundsInScreen();
    animation_target_bounds_ = target_bounds;
    bounds_animation_->Start();
  } else {
    widget_->SetBounds(target_bounds);
  }
}

void TabDialogManager::UpdateModalDialogHost() {
  if (web_contents_modal_dialog_host_observer_) {
    web_contents_modal_dialog_host_observer_->UpdateModalDialogHost();
    UpdateModalDialogBounds();
  }
}

bool TabDialogManager::UpdateDialogVisibility(
    std::optional<bool> requested_visibility) {
  if (!widget_) {
    return false;
  }
  const bool should_be_visible =
      GetDialogWidgetVisibility() && requested_visibility.value_or(true);
  if (should_be_visible) {
    params_->should_show_inactive ? widget_->ShowInactive() : widget_->Show();
  } else {
    widget_->Hide();
  }
  return widget_->IsVisible();
}

bool TabDialogManager::IsDialogManaged(views::Widget* widget) {
  return widget_ && widget == widget_.get();
}

void TabDialogManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!widget_) {
    return;
  }

  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // Disable BFCache for the page which had any modal dialog open.
  // This prevents the page which has print, confirm form resubmission, http
  // password dialogs, etc. to go in to BFCache. We can't simply dismiss the
  // dialogs in the case, since they are requesting meaningful input from the
  // user that affects the loading or display of the content.
  content::BackForwardCache::DisableForRenderFrameHost(
      navigation_handle->GetPreviousRenderFrameHostId(),
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::kModalDialog));

  // Close modal dialogs if necessary.
  bool different_site_navigation =
      !net::registry_controlled_domains::SameDomainOrHost(
          navigation_handle->GetPreviousPrimaryMainFrameURL(),
          navigation_handle->GetURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (params_->close_on_navigate && different_site_navigation) {
    CloseDialog();
  }
}

void TabDialogManager::PrimaryMainFrameWasResized(bool width_changed) {
  if (base::FeatureList::IsEnabled(features::kSideBySide)) {
    UpdateModalDialogBounds();
  }
}

void TabDialogManager::TabDidEnterForeground(TabInterface* tab_interface) {
  if (widget_) {
    if (base::FeatureList::IsEnabled(features::kSideBySide)) {
      web_contents_modal_dialog_host_observer_ =
          std::make_unique<WebContentsModalDialogHostObserver>(this,
                                                               tab_interface_);
    } else {
      browser_window_widget_observer_ =
          std::make_unique<BrowserWindowWidgetObserver>(this, tab_interface_,
                                                        widget_.get());
    }
    // Check if the tab was detached and dragged to a new browser window. This
    // ensures the widget is properly reparented.
    auto* parent_widget = GetHostWidget();
    if (parent_widget != widget_->parent()) {
      widget_->Reparent(parent_widget);
    }
    UpdateDialogVisibility();
    UpdateModalDialogBounds();
  }
}

void TabDialogManager::TabWillEnterBackground(TabInterface* tab_interface) {
  if (widget_) {
    if (bounds_animation_ && bounds_animation_->is_animating()) {
      bounds_animation_->Stop();
    }
    widget_->SetVisible(false);
    browser_window_widget_observer_.reset();
    web_contents_modal_dialog_host_observer_.reset();
  }
}

void TabDialogManager::TabWillDetach(TabInterface* tab_interface,
                                     TabInterface::DetachReason reason) {
  if (widget_ && params_->close_on_detach) {
    CloseDialog();
  }
}

bool TabDialogManager::GetDialogWidgetVisibility() {
  // The dialog widget should be visible if and only if the tab is in the
  // foreground and the host window is not minimized. The inactive tab in a
  // split view can show a modal dialog.
  return GetWidgetVisibility(
      tab_interface_->IsVisible(),
      tab_interface_->GetBrowserWindowInterface()->GetWindow()->IsMinimized(),
      params_->should_show_callback);
}

void TabDialogManager::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == bounds_animation_.get()) {
    gfx::Rect new_bounds = animation->CurrentValueBetween(
        animation_start_bounds_, animation_target_bounds_);
    widget_->SetBounds(new_bounds);
  }
}

void TabDialogManager::AnimationEnded(const gfx::Animation* animation) {
  if (animation == bounds_animation_.get()) {
    widget_->SetBounds(animation_target_bounds_);
  }
}

}  // namespace tabs
