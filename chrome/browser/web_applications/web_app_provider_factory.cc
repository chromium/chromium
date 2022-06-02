// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider_factory.h"

#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace web_app {

// static
WebAppProvider* WebAppProviderFactory::GetForProfile(Profile* profile) {
  return static_cast<WebAppProvider*>(
      WebAppProviderFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
WebAppProviderFactory* WebAppProviderFactory::GetInstance() {
  return base::Singleton<WebAppProviderFactory>::get();
}

// static
bool WebAppProviderFactory::IsServiceCreatedForProfile(Profile* profile) {
  return WebAppProviderFactory::GetInstance()->GetServiceForBrowserContext(
             profile, /*create=*/false) != nullptr;
}

WebAppProviderFactory::WebAppProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "WebAppProvider",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOnExtensionsSystem();
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(ukm::UkmBackgroundRecorderFactory::GetInstance());
  // Required to listen to file handling settings change in
  // `WebAppInstallFinalizer::OnContentSettingChanged()`
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

WebAppProviderFactory::~WebAppProviderFactory() = default;

KeyedService* WebAppProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  WebAppProvider* provider = new WebAppProvider(profile);
  provider->Start();

  // TODO(crbug.com/1321984): Make SWAM a KeyedService and move this scheduling
  // to that new service factory.
  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(&ash::SystemWebAppManager::Start,
                     provider->system_web_app_manager_->GetWeakPtr()));

  return provider;
}

bool WebAppProviderFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* WebAppProviderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextForWebApps(context);
}

}  //  namespace web_app
