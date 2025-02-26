// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/bnpl_tos_view_desktop.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/autofill/payments/bnpl_tos_dialog.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"
#include "content/public/browser/web_contents.h"

using tabs::TabInterface;

namespace autofill {

std::unique_ptr<BnplTosView> CreateAndShowBnplTos(
    base::WeakPtr<BnplTosController> controller,
    content::WebContents* web_contents) {
  return std::make_unique<BnplTosViewDesktop>(controller, web_contents);
}

BnplTosViewDesktop::BnplTosViewDesktop(
    base::WeakPtr<BnplTosController> controller,
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  std::unique_ptr<BnplTosDialog> dialog_view = std::make_unique<BnplTosDialog>(
      controller, base::BindRepeating(&BnplTosViewDesktop::OpenLink,
                                      weak_ptr_factory_.GetWeakPtr()));
  TabInterface* tab_interface = TabInterface::GetFromContents(web_contents_);
  CHECK(tab_interface);
  dialog_widget_ =
      tab_interface->GetTabFeatures()
          ->tab_dialog_manager()
          ->CreateShowDialogAndBlockTabInteraction(dialog_view.release());
  dialog_widget_->MakeCloseSynchronous(base::BindOnce(
      &BnplTosViewDesktop::CloseWidget, weak_ptr_factory_.GetWeakPtr()));
}

BnplTosViewDesktop::~BnplTosViewDesktop() = default;

void BnplTosViewDesktop::CloseWidget(
    views::Widget::ClosedReason closed_reason) {
  BnplTosDialog* dialog_view =
      AsViewClass<BnplTosDialog>(dialog_widget_->GetClientContentsView());
  CHECK(dialog_view);
  // The following call will result in the destruction of this and, indirectly,
  // the `dialog_widget_`. Do not access this after the following call.
  dialog_view->controller()->OnViewClosing(
      closed_reason == views::Widget::ClosedReason::kAcceptButtonClicked);
}

void BnplTosViewDesktop::OpenLink(const GURL& url) {
  web_contents_->OpenURL(
      content::OpenURLParams(
          url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
}

}  // namespace autofill
