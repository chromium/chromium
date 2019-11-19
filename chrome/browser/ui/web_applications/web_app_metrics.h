// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/engagement/site_engagement_observer.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// A per-profile keyed service, responsible for all Web Applications-related
// metrics recording (UMA histograms).
class WebAppMetrics : public KeyedService, public SiteEngagementObserver {
 public:
  static WebAppMetrics* Get(Profile* profile);

  explicit WebAppMetrics(Profile* profile);
  ~WebAppMetrics() override;

  // SiteEngagementObserver:
  void OnEngagementEvent(
      content::WebContents* web_contents,
      const GURL& url,
      double score,
      SiteEngagementService::EngagementType engagement_type) override;

  void CountUserInstalledAppsForTesting();

 private:
  void CountUserInstalledApps();

  // Calculate number of user installed apps once on start to avoid cpu costs
  // in OnEngagementEvent: sacrifice histograms accuracy for speed.
  static constexpr int kNumUserInstalledAppsNotCounted = -1;
  int num_user_installed_apps_ = kNumUserInstalledAppsNotCounted;

  Profile* const profile_;

  base::WeakPtrFactory<WebAppMetrics> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppMetrics);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_METRICS_H_
