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
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
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
                               BrowserWindowInterface* host_browser_window,
                               const gfx::Size& size) {
  gfx::Point position =
      host_browser_window->GetWebContentsModalDialogHostForWindow()
          ->GetDialogPosition(size);
  // Align the first row of pixels inside the border. This is the apparent top
  // of the dialog.
  position.set_y(position.y() -
                 widget->non_client_view()->frame_view()->GetInsets().top());

  gfx::Rect dialog_bounds(position, size);

  if (widget->is_top_level() && SupportsGlobalScreenCoordinates()) {
    views::Widget* const host_widget =
        host_browser_window->TopContainer()->GetWidget();
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

void UpdateModalDialogPosition(views::Widget* widget,
                               BrowserWindowInterface* host_browser_window,
                               const gfx::Size& size) {
  // Do not forcibly update the dialog widget position if it is being dragged.
  if (widget->HasCapture()) {
    return;
  }

  auto* host_view_widget = host_browser_window->TopContainer()->GetWidget();

  // If the host view's widget is minimized, don't update any positions.
  if (host_view_widget && host_view_widget->IsMinimized()) {
    return;
  }

  // If the host view is not backed by a Views::Widget, just update the widget
  // size. This can happen on MacViews under the Cocoa browser where the window
  // modal dialogs are displayed as sheets, and their position is managed by a
  // ConstrainedWindowSheetController instance.
  if (!host_view_widget) {
    widget->SetSize(size);
    return;
  }

  widget->SetBounds(GetModalDialogBounds(widget, host_browser_window, size));
}

void ConfigureDesiredBoundsDelegate(
    views::Widget* widget,
    BrowserWindowInterface* host_browser_window) {
  views::WidgetDelegate* delegate = widget->widget_delegate();
  // TODO(kylixrd): Audit other usages of this API and determine whether to make
  // it exclusive for use here. Currently used in BubbleDialogDelegate and
  // shouldn't ever be used for a tab-modal dialog.
  delegate->set_desired_bounds_delegate(base::BindRepeating(
      [](views::Widget* widget,
         BrowserWindowInterface* host_browser_window) -> gfx::Rect {
        return GetModalDialogBounds(
            widget, host_browser_window,
            widget->GetRootView()->GetPreferredSize({}));
      },
      widget, host_browser_window));
}

// The dialog widget should be visible if and only if the tab is in the
// foreground and activated, and the host window is not minimized.
bool GetDialogWidgetVisibility(bool activated, bool minimized) {
  return activated && !minimized;
}

}  // namespace

// Applies positioning changes from the browser window widget to the tracked
// Widget. This class relies on the assumption that it is scoped to the lifetime
// of a single tab, in a single browser, and that it will be destroyed
// before the tab moves between browser windows.
class BrowserWindowWidgetObserver : public views::WidgetObserver {
 public:
  BrowserWindowWidgetObserver(TabInterface* tab_interface,
                              views::Widget* dialog_widget)
      : tab_(tab_interface), dialog_widget_(dialog_widget) {
    CHECK(dialog_widget_);
    browser_window_widget_observation_.Observe(
        tab_->GetBrowserWindowInterface()->TopContainer()->GetWidget());
  }
  BrowserWindowWidgetObserver(const BrowserWindowWidgetObserver&) = delete;
  BrowserWindowWidgetObserver& operator=(const BrowserWindowWidgetObserver&) =
      delete;
  ~BrowserWindowWidgetObserver() override = default;

  // WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    if (dialog_widget_->IsVisible()) {
      UpdateModalDialogPosition(
          dialog_widget_, tab_->GetBrowserWindowInterface(),
          dialog_widget_->GetRootView()->GetPreferredSize({}));
    }
  }

  void OnWidgetShowStateChanged(views::Widget* widget) override {
    bool minimized = widget->IsMinimized();
    bool activated = tab_->IsActivated();
    dialog_widget_->SetVisible(GetDialogWidgetVisibility(activated, minimized));
  }

 private:
  // The tab that owns this dialog manager.
  raw_ptr<TabInterface> tab_;

  // The widget being tracked.
  raw_ptr<views::Widget> dialog_widget_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_window_widget_observation_{this};
};

TabDialogManager::TabDialogManager(TabInterface* tab_interface)
    : content::WebContentsObserver(tab_interface->GetContents()),
      tab_interface_(tab_interface) {
  tab_did_enter_foreground_subscription_ =
      tab_interface_->RegisterDidActivate(base::BindRepeating(
          &TabDialogManager::TabDidEnterForeground, base::Unretained(this)));
  tab_will_enter_background_subscription_ =
      tab_interface_->RegisterWillDeactivate(base::BindRepeating(
          &TabDialogManager::TabWillEnterBackground, base::Unretained(this)));
  tab_will_detach_subscription_ =
      tab_interface_->RegisterWillDetach(base::BindRepeating(
          &TabDialogManager::TabWillDetach, base::Unretained(this)));
}

TabDialogManager::~TabDialogManager() = default;

std::unique_ptr<views::Widget> TabDialogManager::CreateTabScopedDialog(
    views::DialogDelegate* delegate) {
  DCHECK_EQ(ui::mojom::ModalType::kChild, delegate->GetModalType());
  views::Widget* host =
      tab_interface_->GetBrowserWindowInterface()->TopContainer()->GetWidget();
  CHECK(host);
  return base::WrapUnique(views::DialogDelegate::CreateDialogWidget(
      delegate, gfx::NativeWindow(), host->GetNativeView()));
}

void TabDialogManager::ShowDialogAndBlockTabInteraction(
    views::Widget* widget,
    bool close_on_navigation) {
  close_on_navigation_ = close_on_navigation;
  widget_ = widget;
  auto* browser_window_interface = tab_interface_->GetBrowserWindowInterface();
  ConfigureDesiredBoundsDelegate(widget_.get(), browser_window_interface);
  UpdateModalDialogPosition(widget_.get(), browser_window_interface,
                            widget_->GetRootView()->GetPreferredSize({}));
  widget_->SetNativeWindowProperty(
      views::kWidgetIdentifierKey,
      const_cast<void*>(
          constrained_window::kConstrainedWindowWidgetIdentifier));
  scoped_ignore_input_events_ =
      tab_interface_->GetContents()->IgnoreInputEvents(std::nullopt);
  tab_interface_->GetBrowserWindowInterface()->SetWebContentsBlocked(
      tab_interface_->GetContents(), /*blocked=*/true);
  tab_dialog_widget_observer_ =
      std::make_unique<TabDialogWidgetObserver>(this, widget_.get());
  showing_modal_ui_ = tab_interface_->ShowModalUI();
  browser_window_widget_observer_ =
      std::make_unique<BrowserWindowWidgetObserver>(tab_interface_,
                                                    widget_.get());
  bool minimized = browser_window_interface->IsMinimized();
  bool activated = tab_interface_->IsActivated();
  widget_->Show();
  widget_->SetVisible(GetDialogWidgetVisibility(activated, minimized));
}

std::unique_ptr<views::Widget>
TabDialogManager::CreateShowDialogAndBlockTabInteraction(
    views::DialogDelegate* delegate,
    bool close_on_navigation) {
  auto widget = CreateTabScopedDialog(delegate);
  ShowDialogAndBlockTabInteraction(widget.get(), close_on_navigation);
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

void TabDialogManager::WidgetDestroyed(views::Widget* widget) {
  CHECK_EQ(widget, widget_.get());
  widget_ = nullptr;
  showing_modal_ui_.reset();
  tab_dialog_widget_observer_.reset();
  scoped_ignore_input_events_.reset();
  browser_window_widget_observer_.reset();
  tab_interface_->GetBrowserWindowInterface()->SetWebContentsBlocked(
      tab_interface_->GetContents(), /*blocked=*/false);
}

void TabDialogManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (widget_) {
    // Disable BFCache for the page which had any modal dialog open.
    // This prevents the page which has print, confirm form resubmission, http
    // password dialogs, etc. to go in to BFCache. We can't simply dismiss the
    // dialogs in the case, since they are requesting meaningful input from the
    // user that affects the loading or display of the content.
    content::BackForwardCache::DisableForRenderFrameHost(
        navigation_handle->GetPreviousRenderFrameHostId(),
        back_forward_cache::DisabledReason(
            back_forward_cache::DisabledReasonId::kModalDialog));
  }

  // Close modal dialogs if navigation is to a new domain/host.
  if (close_on_navigation_ &&
      !net::registry_controlled_domains::SameDomainOrHost(
          navigation_handle->GetPreviousPrimaryMainFrameURL(),
          navigation_handle->GetURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    CloseDialog();
  }
}

void TabDialogManager::TabDidEnterForeground(TabInterface* tab_interface) {
  if (widget_) {
    UpdateModalDialogPosition(widget_.get(),
                              tab_interface_->GetBrowserWindowInterface(),
                              widget_->GetRootView()->GetPreferredSize({}));
    browser_window_widget_observer_ =
        std::make_unique<BrowserWindowWidgetObserver>(tab_interface_,
                                                      widget_.get());
    // Check if the tab was detached and dragged to a new browser window. This
    // ensures the widget is properly reparented.
    auto* parent_widget =
        tab_interface->GetBrowserWindowInterface()->TopContainer()->GetWidget();
    if (parent_widget != widget_->parent()) {
      widget_->Reparent(parent_widget);
    }
    widget_->SetVisible(
        GetDialogWidgetVisibility(/*activated=*/true, widget_->IsMinimized()));
  }
}

void TabDialogManager::TabWillEnterBackground(TabInterface* tab_interface) {
  if (widget_) {
    widget_->SetVisible(false);
    browser_window_widget_observer_.reset();
  }
}

void TabDialogManager::TabWillDetach(TabInterface* tab_interface,
                                     TabInterface::DetachReason reason) {
  CloseDialog();
}

}  // namespace tabs
