// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_and_fill_view_desktop.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/autofill/payments/save_and_fill_dialog.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

using tabs::TabInterface;

namespace autofill {

std::unique_ptr<SaveAndFillDialogView> CreateAndShowSaveAndFillDialog(
    base::WeakPtr<SaveAndFillDialogController> controller,
    content::WebContents* web_contents) {
  return std::make_unique<SaveAndFillViewDesktop>(controller, web_contents);
}

SaveAndFillViewDesktop::SaveAndFillViewDesktop(
    base::WeakPtr<SaveAndFillDialogController> controller,
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  auto dialog_view = std::make_unique<SaveAndFillDialog>(
      controller,
      base::BindRepeating(&SaveAndFillViewDesktop::OnLegalMessageLinkClicked,
                          weak_ptr_factory_.GetWeakPtr()));
  TabInterface* tab_interface = TabInterface::GetFromContents(web_contents);
  CHECK(tab_interface);
  dialog_widget_ = tab_interface->GetTabFeatures()
                       ->tab_dialog_manager()
                       ->CreateAndShowDialog(
                           dialog_view.release(),
                           std::make_unique<tabs::TabDialogManager::Params>());
}

SaveAndFillViewDesktop::~SaveAndFillViewDesktop() = default;

void SaveAndFillViewDesktop::OnLegalMessageLinkClicked(const GURL& url) {
  web_contents_->OpenURL(
      content::OpenURLParams(
          url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
}

}  // namespace autofill
