// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

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

  return gfx::Rect(position, size);
}

void UpdateModalDialogPosition(views::Widget* widget,
                               BrowserWindowInterface* host_browser_window,
                               const gfx::Size& size) {
  // Do not forcibly update the dialog widget position if it is being dragged.
  if (widget->HasCapture()) {
    return;
  }

  // If the host view is not backed by a Views::Widget, just update the widget
  // size. This can happen on MacViews under the Cocoa browser where the window
  // modal dialogs are displayed as sheets, and their position is managed by a
  // ConstrainedWindowSheetController instance.
  if (!host_browser_window->TopContainer()->GetWidget()) {
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

}  // namespace

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
      delegate, nullptr, host->GetNativeView()));
}

void TabDialogManager::ShowDialogAndBlockTabInteraction(views::Widget* widget) {
  CHECK(tab_interface_->CanShowModalUI());
  widget_ = widget->GetWeakPtr();
  ConfigureDesiredBoundsDelegate(widget_.get(),
                                 tab_interface_->GetBrowserWindowInterface());
  UpdateModalDialogPosition(widget_.get(),
                            tab_interface_->GetBrowserWindowInterface(),
                            widget_->GetRootView()->GetPreferredSize({}));
  widget_->SetNativeWindowProperty(
      views::kWidgetIdentifierKey,
      const_cast<void*>(
          constrained_window::kConstrainedWindowWidgetIdentifier));
  scoped_ignore_input_events_ =
      tab_interface_->GetContents()->IgnoreInputEvents(std::nullopt);
  tab_dialog_widget_observer_ =
      std::make_unique<TabDialogWidgetObserver>(this, widget_.get());
  if (tab_interface_->IsActivated()) {
    widget_->Show();
  }
}

std::unique_ptr<views::Widget>
TabDialogManager::CreateShowDialogAndBlockTabInteraction(
    views::DialogDelegate* delegate) {
  auto widget = CreateTabScopedDialog(delegate);
  ShowDialogAndBlockTabInteraction(widget.get());
  return widget;
}

void TabDialogManager::CloseDialog() {
  if (widget_) {
    widget_->Close();
    widget_.reset();
  }
}

void TabDialogManager::WidgetDestroyed(views::Widget* widget) {
  CHECK_EQ(widget, widget_.get());
  tab_dialog_widget_observer_.reset();
  scoped_ignore_input_events_.reset();
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

  // Close modal dialogs if necessary.
  if (!net::registry_controlled_domains::SameDomainOrHost(
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
    widget_->SetVisible(true);
  }
}

void TabDialogManager::TabWillEnterBackground(TabInterface* tab_interface) {
  if (widget_) {
    widget_->SetVisible(false);
  }
}

void TabDialogManager::TabWillDetach(TabInterface* tab_interface,
                                     TabInterface::DetachReason reason) {
  CloseDialog();
}

}  // namespace tabs
