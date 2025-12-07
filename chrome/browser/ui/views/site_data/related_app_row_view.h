// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SITE_DATA_RELATED_APP_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SITE_DATA_RELATED_APP_ROW_VIEW_H_

#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/view.h"

class Profile;

DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kRelatedAppRowMenuItemClicked);

// A view that represents 1 app's entry in the PageSpecificSiteDataDialog menu.
// An entry has the app icon, the app name, and a button that links to the app's
// site settings page.
class RelatedAppRowView : public views::View,
                          web_app::WebAppInstallManagerObserver {
  METADATA_HEADER(RelatedAppRowView, views::View)

 public:
  RelatedAppRowView(Profile* profile,
                    const webapps::AppId& app_id,
                    base::RepeatingCallback<void(const webapps::AppId&)>
                        open_site_settings_callback);

  ~RelatedAppRowView() override;

  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kLinkToAppSettings);

 private:
  friend class PageSpecificSiteDataDialogBrowserTest;

  void OnAppSettingsLinkClick();

  webapps::AppId app_id_;
  base::RepeatingCallback<void(const webapps::AppId&)>
      open_site_settings_callback_;

  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};
  bool app_was_uninstalled_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SITE_DATA_RELATED_APP_ROW_VIEW_H_
