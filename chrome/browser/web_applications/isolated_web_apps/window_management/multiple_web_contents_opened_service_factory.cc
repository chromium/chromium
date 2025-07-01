// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/window_management/multiple_web_contents_opened_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/common/content_features.h"

namespace {
BASE_FEATURE(kMultipleWebContentsOpenedByIwaNotification,
             "MultipleWebContentsOpenedByIwaNotification",
             base::FEATURE_ENABLED_BY_DEFAULT);
}

MultipleWebContentsOpenedServiceFactory*
MultipleWebContentsOpenedServiceFactory::GetInstance() {
  static base::NoDestructor<MultipleWebContentsOpenedServiceFactory> instance;
  return instance.get();
}

// static
MultipleWebContentsOpenedService*
MultipleWebContentsOpenedServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<MultipleWebContentsOpenedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

MultipleWebContentsOpenedServiceFactory::
    MultipleWebContentsOpenedServiceFactory()
    : ProfileKeyedServiceFactory("MultipleWebContentsOpenedService") {
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}
bool MultipleWebContentsOpenedServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  // TODO(crbug.com/428672473)
  return true;
}
std::unique_ptr<KeyedService>
MultipleWebContentsOpenedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  if (!base::FeatureList::IsEnabled(
          kMultipleWebContentsOpenedByIwaNotification) ||
      !content::AreIsolatedWebAppsEnabled(browser_context)) {
    return nullptr;
  }
  return std::make_unique<MultipleWebContentsOpenedService>(
      Profile::FromBrowserContext(browser_context));
}
