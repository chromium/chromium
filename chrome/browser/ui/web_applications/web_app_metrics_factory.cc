// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_metrics_factory.h"

#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace web_app {

// static
WebAppMetrics* WebAppMetricsFactory::GetForProfile(Profile* profile) {
  return static_cast<WebAppMetrics*>(
      WebAppMetricsFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create_service=*/true));
}

// static
WebAppMetricsFactory* WebAppMetricsFactory::GetInstance() {
  return base::Singleton<WebAppMetricsFactory>::get();
}

WebAppMetricsFactory::WebAppMetricsFactory()
    : BrowserContextKeyedServiceFactory(
          "WebAppMetrics",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SiteEngagementServiceFactory::GetInstance());
  DependsOn(WebAppProviderFactory::GetInstance());
}

WebAppMetricsFactory::~WebAppMetricsFactory() = default;

KeyedService* WebAppMetricsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new WebAppMetrics(profile);
}

bool WebAppMetricsFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}

content::BrowserContext* WebAppMetricsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextForWebAppMetrics(context);
}

}  //  namespace web_app
