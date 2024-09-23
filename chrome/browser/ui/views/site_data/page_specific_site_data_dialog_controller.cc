// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/widget/widget.h"

void RecordPageSpecificSiteDataDialogOpenedAction() {
  base::RecordAction(base::UserMetricsAction("CookiesInUseDialog.Opened"));
}

void RecordPageSpecificSiteDataDialogRemoveButtonClickedAction() {
  base::RecordAction(
      base::UserMetricsAction("CookiesInUseDialog.RemoveButtonClicked"));
}

// static
views::View* PageSpecificSiteDataDialogController::GetDialogView(
    content::WebContents* web_contents) {
  PageSpecificSiteDataDialogController* handle =
      static_cast<PageSpecificSiteDataDialogController*>(
          web_contents->GetUserData(
              PageSpecificSiteDataDialogController::UserDataKey()));
  if (!handle)
    return nullptr;
  return handle->GetDialogView();
}

// static
void PageSpecificSiteDataDialogController::CreateAndShowForWebContents(
    content::WebContents* web_contents) {
  views::View* const instance =
      PageSpecificSiteDataDialogController::GetDialogView(web_contents);
  if (!instance) {
    PageSpecificSiteDataDialogController::CreateForWebContents(web_contents);
    return;
  }

  // On rare occasions, |instance| may have started, but not finished,
  // closing. In this case, the modal dialog manager will have removed the
  // dialog from its list of tracked dialogs, and therefore might not have any
  // active dialog. This should be rare enough that it's not worth trying to
  // re-open the dialog. See https://crbug.com/989888
  if (instance->GetWidget()->IsClosed())
    return;

  auto* dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  CHECK(dialog_manager->IsDialogActive());
  dialog_manager->FocusTopmostDialog();
}

PageSpecificSiteDataDialogController::PageSpecificSiteDataDialogController(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PageSpecificSiteDataDialogController>(
          *web_contents) {
  views::Widget* const widget = ShowPageSpecificSiteDataDialog(web_contents);
  tracker_.SetView(widget->GetRootView());
}

views::View* PageSpecificSiteDataDialogController::GetDialogView() {
  // TODO(crbug.com/40231917): Revisit this after the new dialog is launched.
  // Consider not using the view tracker here but using instead a flag to
  // track if the widget is open and a CancelableCallback to track that the
  // widget is closed.
  return tracker_.view();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageSpecificSiteDataDialogController);
