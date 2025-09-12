// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_handler_modal_dialog_manager_delegate.h"

#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"

namespace payments {

PaymentHandlerModalDialogManagerDelegate::
    PaymentHandlerModalDialogManagerDelegate(
        content::WebContents* host_web_contents)
    : host_web_contents_(host_web_contents->GetWeakPtr()) {}

PaymentHandlerModalDialogManagerDelegate::
    ~PaymentHandlerModalDialogManagerDelegate() = default;

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
  if (!host_web_contents_) {
    return nullptr;
  }

  auto* dialog_manager =
      static_cast<web_modal::WebContentsModalDialogManagerDelegate*>(
          chrome::FindBrowserWithTab(host_web_contents_.get()));
  if (!dialog_manager) {
    return nullptr;
  }

  // Borrow the browser's WebContentModalDialogHost to display modal dialogs
  // triggered by the payment handler's web view (e.g. WebAuthn and Secure
  // Payment Confirmation dialogs).
  return dialog_manager->GetWebContentsModalDialogHost(
      host_web_contents_.get());
}

bool PaymentHandlerModalDialogManagerDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  DCHECK_EQ(web_contents_, web_contents);
  return platform_util::IsVisible(web_contents->GetNativeView());
}

void PaymentHandlerModalDialogManagerDelegate::OnWebContentsModalDialogShown(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(features::kSideBySide)) {
    return;
  }
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return;
  }

  TabStripModel* tab_strip_model =
      tab->GetBrowserWindowInterface()->GetTabStripModel();
  const int tab_index = tab_strip_model->GetIndexOfWebContents(web_contents);
  if (tab_strip_model->GetActiveWebContents() != web_contents &&
      tab_strip_model->IsTabInForeground(tab_index)) {
    tab_strip_model->ActivateTabAt(
        tab_strip_model->GetIndexOfWebContents(web_contents));
  }
}

void PaymentHandlerModalDialogManagerDelegate::SetWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  web_contents_ = web_contents;
}

}  // namespace payments
