// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_metrics_factory.h"

#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_metrics.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace web_app {

// static
WebAppMetrics* WebAppMetricsFactory::GetForProfile(Profile* profile) {
  return static_cast<WebAppMetrics*>(
      WebAppMetricsFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
WebAppMetricsFactory* WebAppMetricsFactory::GetInstance() {
  static base::NoDestructor<WebAppMetricsFactory> instance;
  return instance.get();
}

WebAppMetricsFactory::WebAppMetricsFactory()
    : BrowserContextKeyedServiceFactory(
          "WebAppMetrics",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(site_engagement::SiteEngagementServiceFactory::GetInstance());
  DependsOn(WebAppProviderFactory::GetInstance());
}

WebAppMetricsFactory::~WebAppMetricsFactory() = default;

std::unique_ptr<KeyedService>
WebAppMetricsFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<WebAppMetrics>(profile);
}

content::BrowserContext* WebAppMetricsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextForWebAppMetrics(context);
}

bool WebAppMetricsFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  //  namespace web_app
