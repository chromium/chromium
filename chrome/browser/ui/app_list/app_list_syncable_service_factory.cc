// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"

#include <set>

#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace app_list {

namespace {
bool use_in_testing = false;
}

// static
AppListSyncableService* AppListSyncableServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AppListSyncableService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AppListSyncableServiceFactory* AppListSyncableServiceFactory::GetInstance() {
  return base::Singleton<AppListSyncableServiceFactory>::get();
}

// static
std::unique_ptr<KeyedService> AppListSyncableServiceFactory::BuildInstanceFor(
    content::BrowserContext* browser_context) {
  Profile* profile = static_cast<Profile*>(browser_context);
  if (chromeos::ProfileHelper::IsSigninProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile)) {
    return nullptr;
  }
  VLOG(1) << "BuildInstanceFor: " << profile->GetDebugName()
          << " (" << profile << ")";
  return std::make_unique<AppListSyncableService>(profile);
}

// static
void AppListSyncableServiceFactory::SetUseInTesting() {
  use_in_testing = true;
}

AppListSyncableServiceFactory::AppListSyncableServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "AppListSyncableService",
        BrowserContextDependencyManager::GetInstance()) {
  VLOG(1) << "AppListSyncableServiceFactory()";
  typedef std::set<BrowserContextKeyedServiceFactory*> FactorySet;
  FactorySet dependent_factories;
  dependent_factories.insert(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  dependent_factories.insert(ArcAppListPrefsFactory::GetInstance());
  dependent_factories.insert(apps::AppServiceProxyFactory::GetInstance());
  for (FactorySet::iterator it = dependent_factories.begin();
       it != dependent_factories.end();
       ++it) {
    DependsOn(*it);
  }
}

AppListSyncableServiceFactory::~AppListSyncableServiceFactory() {
}

KeyedService* AppListSyncableServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return BuildInstanceFor(static_cast<Profile*>(browser_context)).release();
}

void AppListSyncableServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
}

content::BrowserContext* AppListSyncableServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  // No service if |context| is not a profile.
  if (!profile)
    return nullptr;

  // No service for system profile.
  if (profile->IsSystemProfile())
    return nullptr;

  // No service for sign in profile.
  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return nullptr;

  // Use profile as-is for guest session.
  if (profile->IsGuestSession())
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);

  // This matches the logic in ExtensionSyncServiceFactory, which uses the
  // orginal browser context.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool AppListSyncableServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // Start AppListSyncableService early so that the app list positions are
  // available before the app list is opened.
  return true;
}

bool AppListSyncableServiceFactory::ServiceIsNULLWhileTesting() const {
  return !use_in_testing;
}

}  // namespace app_list
