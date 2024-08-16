// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/web_applications/daily_metrics_helper.h"
#include "chrome/browser/web_applications/extensions_manager.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app_pref_guardrails.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)
namespace web_app {

// static
WebAppProvider* WebAppProviderFactory::GetForProfile(Profile* profile) {
  return static_cast<WebAppProvider*>(
      WebAppProviderFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
WebAppProviderFactory* WebAppProviderFactory::GetInstance() {
  static base::NoDestructor<WebAppProviderFactory> instance;
  return instance.get();
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
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(ukm::UkmBackgroundRecorderFactory::GetInstance());
  // Required to listen to file handling settings change in
  // `WebAppInstallFinalizer::OnContentSettingChanged()`
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(ExtensionsManager::GetExtensionSystemSharedFactory());
  // Required to use different preinstalled app configs for managed devices.
  DependsOn(policy::ManagementServiceFactory::GetInstance());
}

WebAppProviderFactory::~WebAppProviderFactory() = default;

std::unique_ptr<KeyedService>
WebAppProviderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto provider = std::make_unique<WebAppProvider>(profile);
  provider->Start();

  return provider;
}

bool WebAppProviderFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* WebAppProviderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextForWebApps(context);
}

void WebAppProviderFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  UserUninstalledPreinstalledWebAppPrefs::RegisterProfilePrefs(registry);
  PreinstalledWebAppManager::RegisterProfilePrefs(registry);
  WebAppPrefGuardrails::RegisterProfilePrefs(registry);
  WebAppPolicyManager::RegisterProfilePrefs(registry);

#if BUILDFLAG(IS_CHROMEOS)
  IsolatedWebAppPolicyManager::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_CHROMEOS)

  registry->RegisterBooleanPref(prefs::kShouldGarbageCollectStoragePartitions,
                                false);
  RegisterInstallBounceMetricProfilePrefs(registry);
  RegisterDailyWebAppMetricsProfilePrefs(registry);
  OsIntegrationManager::RegisterProfilePrefs(registry);
}

}  //  namespace web_app
