// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/web_applications/diagnostics/web_app_icon_health_checks.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/site_engagement/content/site_engagement_observer.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace site_engagement {
enum class EngagementType;
}  // namespace site_engagement

namespace web_app {

// A per-profile keyed service, responsible for all Web Applications-related
// metrics recording (UMA histograms and UKM keyed by web-apps).
class WebAppMetrics : public KeyedService,
                      public site_engagement::SiteEngagementObserver {
 public:
  static WebAppMetrics* Get(Profile* profile);

  static void DisableAutomaticIconHealthChecksForTesting();

  explicit WebAppMetrics(Profile* profile);
  WebAppMetrics(const WebAppMetrics&) = delete;
  WebAppMetrics& operator=(const WebAppMetrics&) = delete;
  ~WebAppMetrics() override;

  // SiteEngagementObserver:
  void OnEngagementEvent(content::WebContents* web_contents,
                         const GURL& url,
                         double score,
                         double old_score,
                         site_engagement::EngagementType engagement_type,
                         const std::optional<webapps::AppId>& app_id) override;

  // Browser activation causes flaky tests. Call observer methods directly.
  void CountUserInstalledAppsForTesting();

  WebAppIconHealthChecks& icon_health_checks_for_testing() {
    return icon_health_checks_;
  }

 private:
  void CountUserInstalledApps();

  // Calculate number of user installed apps once on start to avoid cpu costs
  // in OnEngagementEvent: sacrifice histograms accuracy for speed.
  static constexpr int kNumUserInstalledAppsNotCounted = -1;
  int num_user_installed_apps_ = kNumUserInstalledAppsNotCounted;

  const raw_ptr<Profile> profile_;

  WebAppIconHealthChecks icon_health_checks_;

  base::WeakPtrFactory<WebAppMetrics> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_H_
