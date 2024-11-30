// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
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

namespace {

gfx::Rect GetModalDialogBounds(views::Widget* widget,
                               views::Widget* host_widget,
                               const gfx::Size& size) {
  gfx::Size host_widget_size = host_widget->GetWindowBoundsInScreen().size();
  gfx::Point position =
      gfx::Point((host_widget_size.width() - size.width()) / 2,
                 (host_widget_size.height() - size.height()) / 2);
  // Align the first row of pixels inside the border. This is the apparent top
  // of the dialog.
  position.set_y(position.y() -
                 widget->non_client_view()->frame_view()->GetInsets().top());

  return gfx::Rect(position, size);
}

void UpdateModalDialogPosition(views::Widget* widget,
                               views::Widget* host_widget,
                               const gfx::Size& size) {
  // Do not forcibly update the dialog widget position if it is being dragged.
  if (widget->HasCapture()) {
    return;
  }

  // If the host view is not backed by a Views::Widget, just update the widget
  // size. This can happen on MacViews under the Cocoa browser where the window
  // modal dialogs are displayed as sheets, and their position is managed by a
  // ConstrainedWindowSheetController instance.
  if (!host_widget) {
    widget->SetSize(size);
    return;
  }

  widget->SetBounds(GetModalDialogBounds(widget, host_widget, size));
}

void ConfigureDesiredBoundsDelegate(views::Widget* widget,
                                    views::Widget* host_widget) {
  views::WidgetDelegate* delegate = widget->widget_delegate();
  // TODO(kylixrd): Audit other usages of this API and determine whether to make
  // it exclusive for use here. Currently used in BubbleDialogDelegate and
  // shouldn't ever be used for a tab-modal dialog.
  delegate->set_desired_bounds_delegate(base::BindRepeating(
      [](views::Widget* widget, views::Widget* host_widget) -> gfx::Rect {
        return GetModalDialogBounds(
            widget, host_widget, widget->GetRootView()->GetPreferredSize({}));
      },
      widget, host_widget));
}

}  // namespace

TabDialogManager::TabDialogManager(TabInterface* tab_interface)
    : content::WebContentsObserver(tab_interface->GetContents()),
      tab_interface_(tab_interface) {
  tab_did_enter_foreground_subscription_ =
      tab_interface_->RegisterDidEnterForeground(base::BindRepeating(
          &TabDialogManager::TabDidEnterForeground, base::Unretained(this)));
  tab_will_enter_background_subscription_ =
      tab_interface_->RegisterWillEnterBackground(base::BindRepeating(
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
  ConfigureDesiredBoundsDelegate(
      widget_.get(),
      tab_interface_->GetBrowserWindowInterface()->TopContainer()->GetWidget());
  widget_->SetNativeWindowProperty(
      views::kWidgetIdentifierKey,
      const_cast<void*>(
          constrained_window::kConstrainedWindowWidgetIdentifier));
  scoped_ignore_input_events_ =
      tab_interface_->GetContents()->IgnoreInputEvents(std::nullopt);
  if (tab_interface_->IsInForeground()) {
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
    scoped_ignore_input_events_.reset();
    widget_->Close();
    widget_.reset();
  }
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
                              tab_interface_->GetBrowserWindowInterface()
                                  ->TopContainer()
                                  ->GetWidget(),
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
