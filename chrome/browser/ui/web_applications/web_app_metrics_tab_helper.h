// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_TAB_HELPER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_TAB_HELPER_H_

#include <optional>

#include "base/scoped_observation.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace webapps {
enum class InstallableWebAppCheckResult;
struct WebAppBannerData;
}  // namespace webapps

namespace web_app {

class WebAppMetricsTabHelper
    : public content::WebContentsUserData<WebAppMetricsTabHelper>,
      public content::WebContentsObserver,
      public webapps::AppBannerManager::Observer {
 public:
  WebAppMetricsTabHelper(const WebAppMetricsTabHelper&) = delete;
  WebAppMetricsTabHelper& operator=(const WebAppMetricsTabHelper&) = delete;
  ~WebAppMetricsTabHelper() override;

  // Whether WebAppMetricsTabHelper should be created.
  static bool IsEnabled(content::WebContents* contents);

 private:
  explicit WebAppMetricsTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<WebAppMetricsTabHelper>;

  // content::WebContentsObserver overrides.
  void WebContentsDestroyed() override;

  // webapps::AppBannerManager::Observer:
  void OnInstallableWebAppStatusUpdated(
      webapps::InstallableWebAppCheckResult result,
      const std::optional<webapps::WebAppBannerData>& data) override;

  base::ScopedObservation<webapps::AppBannerManager,
                          webapps::AppBannerManager::Observer>
      app_banner_manager_observer_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_TAB_HELPER_H_
