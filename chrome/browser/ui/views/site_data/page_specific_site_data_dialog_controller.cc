// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"
#include "components/page_info/core/features.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/widget/widget.h"

void RecordPageSpecificSiteDataDialogAction(
    PageSpecificSiteDataDialogAction action) {
  switch (action) {
    case PageSpecificSiteDataDialogAction::kSiteDeleted:
    case PageSpecificSiteDataDialogAction::kSingleCookieDeleted:
    case PageSpecificSiteDataDialogAction::kCookiesFolderDeleted:
    case PageSpecificSiteDataDialogAction::kFolderDeleted:
      base::RecordAction(
          base::UserMetricsAction("CookiesInUseDialog.RemoveButtonClicked"));
      break;
    case PageSpecificSiteDataDialogAction::kDialogOpened:
      base::RecordAction(base::UserMetricsAction("CookiesInUseDialog.Opened"));
      break;
    case PageSpecificSiteDataDialogAction::kSiteBlocked:
    case PageSpecificSiteDataDialogAction::kSiteAllowed:
    case PageSpecificSiteDataDialogAction::kSiteClearedOnExit:
      // No user actions for these metrics.
      break;
  }

  base::UmaHistogramEnumeration("Privacy.CookiesInUseDialog.Action", action);
}

PageSpecificSiteDataDialogAction GetDialogActionForContentSetting(
    ContentSetting setting) {
  switch (setting) {
    case ContentSetting::CONTENT_SETTING_BLOCK:
      return PageSpecificSiteDataDialogAction::kSiteBlocked;
    case ContentSetting::CONTENT_SETTING_ALLOW:
      return PageSpecificSiteDataDialogAction::kSiteAllowed;
    case ContentSetting::CONTENT_SETTING_SESSION_ONLY:
      return PageSpecificSiteDataDialogAction::kSiteClearedOnExit;
    case ContentSetting::CONTENT_SETTING_DEFAULT:
    case ContentSetting::CONTENT_SETTING_ASK:
    case ContentSetting::CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
    case ContentSetting::CONTENT_SETTING_NUM_SETTINGS:
      NOTREACHED_NORETURN() << "Unknown ContentSetting value: " << setting;
  }
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
CollectedCookiesViews*
PageSpecificSiteDataDialogController::GetDialogViewForTesting(
    content::WebContents* web_contents) {
  CHECK(!base::FeatureList::IsEnabled(page_info::kPageSpecificSiteDataDialog));
  return static_cast<CollectedCookiesViews*>(
      PageSpecificSiteDataDialogController::GetDialogView(web_contents));
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
  if (base::FeatureList::IsEnabled(page_info::kPageSpecificSiteDataDialog)) {
    views::Widget* const widget = ShowPageSpecificSiteDataDialog(web_contents);
    tracker_.SetView(widget->GetRootView());
  } else {
    // CollectedCookiesViews is DialogDelegateView and it's owned by its
    // widget. It created the widget in the constructor using
    // `ShowWebModalDialogViews()`. It will be destroyed when its widget is
    // destroyed.
    CollectedCookiesViews* const dialog =
        new CollectedCookiesViews(web_contents);
    tracker_.SetView(dialog);
  }
}

views::View* PageSpecificSiteDataDialogController::GetDialogView() {
  // TODO(crbug.com/1344787): Revisit this after the new dialog is launched.
  // Consider not using the view tracker here but using instead a flag to
  // track if the widget is open and a CancelableCallback to track that the
  // widget is closed.
  return tracker_.view();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageSpecificSiteDataDialogController);
