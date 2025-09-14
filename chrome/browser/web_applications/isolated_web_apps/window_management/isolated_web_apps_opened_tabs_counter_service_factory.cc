// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/isolated_web_apps/service/isolated_web_app_browser_context_service_factory.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/common/content_features.h"

namespace web_app {

namespace {

BASE_FEATURE(kIsolatedWebAppsOpenedTabsCounterServiceNotification,
             base::FEATURE_ENABLED_BY_DEFAULT);
}

IsolatedWebAppsOpenedTabsCounterServiceFactory*
IsolatedWebAppsOpenedTabsCounterServiceFactory::GetInstance() {
  static base::NoDestructor<IsolatedWebAppsOpenedTabsCounterServiceFactory>
      instance;
  return instance.get();
}

// static
IsolatedWebAppsOpenedTabsCounterService*
IsolatedWebAppsOpenedTabsCounterServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<IsolatedWebAppsOpenedTabsCounterService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

IsolatedWebAppsOpenedTabsCounterServiceFactory::
    IsolatedWebAppsOpenedTabsCounterServiceFactory()
    : IsolatedWebAppBrowserContextServiceFactory(
          "IsolatedWebAppsOpenedTabsCounterService") {
  DependsOn(WebAppProviderFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

bool IsolatedWebAppsOpenedTabsCounterServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  // TODO(crbug.com/428672473): switch to lazy service initialization.
  return true;
}

std::unique_ptr<KeyedService> IsolatedWebAppsOpenedTabsCounterServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* browser_context) const {
  if (!base::FeatureList::IsEnabled(
          kIsolatedWebAppsOpenedTabsCounterServiceNotification)) {
    return nullptr;
  }
  return std::make_unique<IsolatedWebAppsOpenedTabsCounterService>(
      Profile::FromBrowserContext(browser_context));
}

}  // namespace web_app
