// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_metrics_tab_helper.h"

#include <optional>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/web_applications/web_app_utils.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

WebAppMetricsTabHelper::~WebAppMetricsTabHelper() = default;

// static
bool WebAppMetricsTabHelper::IsEnabled(content::WebContents* contents) {
  DCHECK(contents);
  bool metrics_enabled =
      web_app::GetBrowserContextForWebAppMetrics(Profile::FromBrowserContext(
          contents->GetBrowserContext())) != nullptr;
  bool app_banner_manager_enabled =
      webapps::AppBannerManager::FromWebContents(contents) != nullptr;
  return metrics_enabled && app_banner_manager_enabled;
}

WebAppMetricsTabHelper::WebAppMetricsTabHelper(content::WebContents* contents)
    : content::WebContentsUserData<WebAppMetricsTabHelper>(*contents),
      content::WebContentsObserver(contents) {
  DCHECK(IsEnabled(contents));
  DCHECK(web_app::WebAppMetrics::Get(
      Profile::FromBrowserContext(contents->GetBrowserContext())));
  auto* app_banner_manager =
      webapps::AppBannerManager::FromWebContents(contents);
  DCHECK(app_banner_manager);
  app_banner_manager_observer_.Observe(app_banner_manager);
}

void WebAppMetricsTabHelper::WebContentsDestroyed() {
  content::WebContents* contents = web_contents();
  DCHECK(contents);
  auto* metrics = WebAppMetrics::Get(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  DCHECK(metrics);
  metrics->NotifyWebContentsDestroyed(contents);

  app_banner_manager_observer_.Reset();
}

void WebAppMetricsTabHelper::OnInstallableWebAppStatusUpdated(
    webapps::InstallableWebAppCheckResult result,
    const std::optional<webapps::WebAppBannerData>& data) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);
  auto* metrics = WebAppMetrics::Get(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  DCHECK(metrics);
  metrics->NotifyInstallableWebAppStatusUpdated(contents, result, data);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebAppMetricsTabHelper);

}  // namespace web_app
