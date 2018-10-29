// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unified_consent/unified_consent_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/unified_consent/chrome_unified_consent_service_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/unified_consent_service.h"

UnifiedConsentServiceFactory::UnifiedConsentServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "UnifiedConsentService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

UnifiedConsentServiceFactory::~UnifiedConsentServiceFactory() = default;

// static
unified_consent::UnifiedConsentService*
UnifiedConsentServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<unified_consent::UnifiedConsentService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
UnifiedConsentServiceFactory* UnifiedConsentServiceFactory::GetInstance() {
  return base::Singleton<UnifiedConsentServiceFactory>::get();
}

void UnifiedConsentServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  unified_consent::UnifiedConsentService::RegisterPrefs(registry);
}

KeyedService* UnifiedConsentServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetSyncServiceForBrowserContext(profile);
  if (!sync_service)
    return nullptr;

  if (!unified_consent::IsUnifiedConsentFeatureEnabled()) {
    ChromeUnifiedConsentServiceClient service_client(profile->GetPrefs());
    unified_consent::UnifiedConsentService::RollbackIfNeeded(
        profile->GetPrefs(), sync_service, &service_client);
    return nullptr;
  }

  return new unified_consent::UnifiedConsentService(
      std::make_unique<ChromeUnifiedConsentServiceClient>(profile->GetPrefs()),
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
      sync_service);
}

bool UnifiedConsentServiceFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

bool UnifiedConsentServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}
