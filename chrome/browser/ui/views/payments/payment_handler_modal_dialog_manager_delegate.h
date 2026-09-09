// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_MODAL_DIALOG_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_MODAL_DIALOG_MANAGER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebContents;
}

namespace gfx {
class Point;
class Rect;
class Size;
}  // namespace gfx

namespace views {
class View;
class Widget;
}  // namespace views

namespace web_modal {
class ModalDialogHostObserver;
}

namespace payments {

// A delegate and host for presenting modal dialogs that are triggered from a
// web-based payment handler. When kPaymentHandlerModalDialogHost is enabled,
// this class implements WebContentsModalDialogHost directly so that child modal
// dialogs (e.g. WebAuthn, PageInfo Certificate Viewer) are parented to the
// PaymentRequestDialogView widget. When disabled, it falls back to borrowing
// the WebContentsModalDialogHost from the browser tab that spawned the payment
// sheet.
//
// ┌────────────────────────────────────────────────────────────┐
// │ Browser Tab (example-merchant.com)                         │
// │ (tab_web_contents_)                                        │
// │                                                            │
// │     ┌────────────────────────────────────────────────┐     │
// │     │ Payment Request Dialog (host_view_)            │     │
// │     │                                                │     │
// │     │   ┌────────────────────────────────────────┐   │     │
// │     │   │ views::WebView (web_contents_) hosting │   │     │
// │     │   │ Payment Handler App (example-pay.com)  │   │     │
// │     │   │                                        │   │     │
// │     │   │      ┌──────────────────────────┐      │   │     │
// │     │   │      │ Child Modal Dialog       │      │   │     │
// │     │   │      │ (e.g. WebAuthn, PageInfo)│      │   │     │
// │     │   │      │                          │      │   │     │
// │     │   │      └──────────────────────────┘      │   │     │
// │     │   └────────────────────────────────────────┘   │     │
// │     └────────────────────────────────────────────────┘     │
// └────────────────────────────────────────────────────────────┘
class PaymentHandlerModalDialogManagerDelegate
    : public web_modal::WebContentsModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost,
      public views::ViewObserver,
      public views::WidgetObserver {
 public:
  // |host_view| is the view representing the Payment Request dialog. Must not
  // be null when kPaymentHandlerModalDialogHost is enabled.
  // |tab_web_contents| is the browser tab that spawned the payment sheet.
  explicit PaymentHandlerModalDialogManagerDelegate(
      views::View* host_view,
      content::WebContents* tab_web_contents);

  PaymentHandlerModalDialogManagerDelegate(
      const PaymentHandlerModalDialogManagerDelegate&) = delete;
  PaymentHandlerModalDialogManagerDelegate& operator=(
      const PaymentHandlerModalDialogManagerDelegate&) = delete;

  ~PaymentHandlerModalDialogManagerDelegate() override;

  // Sets the payment handler's WebContents (the page displaying the payment
  // app) whose modal dialogs are managed by this delegate. |web_contents| must
  // not be null.
  void SetWebContents(content::WebContents* web_contents);

  // WebContentsModalDialogManagerDelegate:
  // |web_contents| must not be null and is expected to be the same as the one
  // provided to SetWebContents().
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost(
      content::WebContents* web_contents) override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;

  // web_modal::ModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // web_modal::WebContentsModalDialogHost:
  gfx::Size GetMaximumDialogSize() override;

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override;
  void OnViewRemovedFromWidget(views::View* observed_view) override;
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  // web_modal::ModalDialogHost:
  void NotifyPositionRequiresUpdate() override;

  // The view against which modal dialogs are positioned and parented.
  raw_ptr<views::View> host_view_;

  base::ScopedObservation<views::View, views::ViewObserver>
      host_view_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  // The WebContents of the browser tab that spawned the payment sheet (used as
  // fallback host when kPaymentHandlerModalDialogHost is disabled).
  base::WeakPtr<content::WebContents> tab_web_contents_;

  // A not-owned pointer to the payment handler's WebContents (the web page
  // displaying the payment app) whose modal dialogs are managed by this
  // delegate.
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_ = nullptr;

  base::ObserverList<web_modal::ModalDialogHostObserver> observer_list_;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_MODAL_DIALOG_MANAGER_DELEGATE_H_
