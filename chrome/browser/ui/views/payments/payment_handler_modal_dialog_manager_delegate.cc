// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_handler_modal_dialog_manager_delegate.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/payments/core/features.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace payments {

// TODO(crbug.com/558687749): Convert host_view_ to raw_ref<views::View> and use
// CHECK_DEREF when kPaymentHandlerModalDialogHost is removed.
PaymentHandlerModalDialogManagerDelegate::
    PaymentHandlerModalDialogManagerDelegate(
        views::View* host_view,
        content::WebContents* tab_web_contents)
    : host_view_(host_view), tab_web_contents_(tab_web_contents->GetWeakPtr()) {
  if (base::FeatureList::IsEnabled(features::kPaymentHandlerModalDialogHost)) {
    CHECK(host_view_);
    host_view_observation_.Observe(host_view_);
    if (host_view_->GetWidget()) {
      widget_observation_.Observe(host_view_->GetWidget());
    }
  }
}

PaymentHandlerModalDialogManagerDelegate::
    ~PaymentHandlerModalDialogManagerDelegate() {
  if (base::FeatureList::IsEnabled(features::kPaymentHandlerModalDialogHost)) {
    observer_list_.Notify(
        &web_modal::ModalDialogHostObserver::OnHostDestroying);
  }
}

void PaymentHandlerModalDialogManagerDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  DCHECK(web_contents);
  DCHECK_EQ(web_contents_, web_contents);
  if (!blocked) {
    web_contents->Focus();
  }
}

web_modal::WebContentsModalDialogHost*
PaymentHandlerModalDialogManagerDelegate::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  if (base::FeatureList::IsEnabled(features::kPaymentHandlerModalDialogHost)) {
    // Acts as the WebContentsModalDialogHost to display modal dialogs triggered
    // by the payment handler's web view (e.g. WebAuthn and Secure Payment
    // Confirmation dialogs) parented directly to the payment dialog widget.
    return this;
  }

  if (!tab_web_contents_) {
    return nullptr;
  }

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(tab_web_contents_.get());
  if (!tab) {
    return nullptr;
  }

  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();

  // Borrow the browser's WebContentModalDialogHost to display modal dialogs
  // triggered by the payment handler's web view (e.g. WebAuthn and Secure
  // Payment Confirmation dialogs).
  return browser->GetWebContentsModalDialogHostForTab(tab);
}

bool PaymentHandlerModalDialogManagerDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  DCHECK_EQ(web_contents_, web_contents);
  return platform_util::IsVisible(web_contents->GetNativeView());
}

void PaymentHandlerModalDialogManagerDelegate::SetWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  web_contents_ = web_contents;
}

gfx::NativeView PaymentHandlerModalDialogManagerDelegate::GetHostView() const {
  if (!host_view_->GetWidget()) {
    return gfx::NativeView();
  }
  return host_view_->GetWidget()->GetNativeView();
}

// Gets the position for the dialog in coordinates relative to the host view.
// Centers the dialog horizontally and vertically over the host view, clamping
// y to 0 so the top of the child dialog stays on-screen if dialog height
// exceeds the host view.
gfx::Point PaymentHandlerModalDialogManagerDelegate::GetDialogPosition(
    const gfx::Size& size) {
  if (!host_view_->GetWidget()) {
    return gfx::Point();
  }

  const gfx::Size host_size = host_view_->size();
  const int x = (host_size.width() - size.width()) / 2;
  const int y = std::max(0, (host_size.height() - size.height()) / 2);
  gfx::Point position(x, y);
  views::View::ConvertPointToWidget(host_view_, &position);
  return position;
}

void PaymentHandlerModalDialogManagerDelegate::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PaymentHandlerModalDialogManagerDelegate::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

gfx::Size PaymentHandlerModalDialogManagerDelegate::GetMaximumDialogSize() {
  if (!host_view_->GetWidget()) {
    return gfx::Size();
  }
  return host_view_->GetWidget()->GetClientAreaBoundsInScreen().size();
}

void PaymentHandlerModalDialogManagerDelegate::OnViewAddedToWidget(
    views::View* observed_view) {
  if (observed_view == host_view_ && host_view_->GetWidget()) {
    widget_observation_.Reset();
    widget_observation_.Observe(host_view_->GetWidget());
    NotifyPositionRequiresUpdate();
  }
}

void PaymentHandlerModalDialogManagerDelegate::OnViewRemovedFromWidget(
    views::View* observed_view) {
  if (observed_view == host_view_) {
    widget_observation_.Reset();
    NotifyPositionRequiresUpdate();
  }
}

void PaymentHandlerModalDialogManagerDelegate::OnViewBoundsChanged(
    views::View* observed_view) {
  if (observed_view == host_view_) {
    NotifyPositionRequiresUpdate();
  }
}

void PaymentHandlerModalDialogManagerDelegate::OnViewIsDeleting(
    views::View* observed_view) {
  if (observed_view == host_view_) {
    widget_observation_.Reset();
    host_view_observation_.Reset();
    host_view_ = nullptr;
  }
}

void PaymentHandlerModalDialogManagerDelegate::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  NotifyPositionRequiresUpdate();
}

void PaymentHandlerModalDialogManagerDelegate::OnWidgetDestroying(
    views::Widget* widget) {
  widget_observation_.Reset();
}

void PaymentHandlerModalDialogManagerDelegate::NotifyPositionRequiresUpdate() {
  observer_list_.Notify(
      &web_modal::ModalDialogHostObserver::OnPositionRequiresUpdate);
}

}  // namespace payments
