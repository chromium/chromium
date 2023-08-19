// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SITE_DATA_PAGE_SPECIFIC_SITE_DATA_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SITE_DATA_PAGE_SPECIFIC_SITE_DATA_DIALOG_CONTROLLER_H_

#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/view_tracker.h"

namespace content {
class WebContents;
}  // namespace content

// Used for UMA histogram to record types of actions done by the user in
// the "Cookies in use" dialog. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class PageSpecificSiteDataDialogAction {
  kDialogOpened = 0,
  kSingleCookieDeleted = 1,
  kCookiesFolderDeleted = 2,
  kFolderDeleted = 3,
  kSiteDeleted = 4,
  kSiteBlocked = 5,
  kSiteAllowed = 6,
  kSiteClearedOnExit = 7,
  kMaxValue = kSiteClearedOnExit,
};

// Records UMA for the |action| and an user action if applicable (for opening
// dialog and deleting action).
void RecordPageSpecificSiteDataDialogAction(
    PageSpecificSiteDataDialogAction action);

// Returns a dialog action that corresponds to the |setting|.
PageSpecificSiteDataDialogAction GetDialogActionForContentSetting(
    ContentSetting setting);

// The controller responsible for creating, showing and holding the reference to
// the page specific site data dialog. The actual dialog opened could be either
// PageSpecificSiteDataDialog or CollectedCookiesView, depending on
// `kPageSpecificSiteDataDialog` feature. The dialog is set as user data to the
// WebContents. The dialog represents the state of the site data of the
// WebContents. To display the dialog, invoke `ShowCollectedCookies()` on the
// `TabDialogs`.
class PageSpecificSiteDataDialogController
    : public content::WebContentsUserData<
          PageSpecificSiteDataDialogController> {
 public:
  static void CreateAndShowForWebContents(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      PageSpecificSiteDataDialogController>;

  static views::View* GetDialogView(content::WebContents* web_contents);

  explicit PageSpecificSiteDataDialogController(
      content::WebContents* web_contents);

  views::View* GetDialogView();

  views::ViewTracker tracker_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SITE_DATA_PAGE_SPECIFIC_SITE_DATA_DIALOG_CONTROLLER_H_
