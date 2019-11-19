// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unified_consent/unified_consent_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "components/unified_consent/unified_consent_service.h"

using unified_consent::UnifiedConsentService;
using unified_consent::metrics::RecordSettingsHistogram;

namespace {

// Returns the synced pref names of the services on the "Sync and Google
// services" settings page.
// Note: The synced prefs returned by this method have to match the prefs
// shown in
// chrome/browser/resources/settings/privacy_page/personalization_options.html
// on Desktop and chrome/android/java/res/xml/sync_and_services_preferences.xml
// on Android.

std::vector<std::string> GetSyncedServicePrefNames() {
  return {
    prefs::kSearchSuggestEnabled, prefs::kAlternateErrorPagesEnabled,
        prefs::kSafeBrowsingEnabled, prefs::kSafeBrowsingScoutReportingEnabled,
        spellcheck::prefs::kSpellCheckUseSpellingService,
#if defined(OS_ANDROID)
        prefs::kContextualSearchEnabled
#endif
  };
}

}  // namespace

UnifiedConsentServiceFactory::UnifiedConsentServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "UnifiedConsentService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

UnifiedConsentServiceFactory::~UnifiedConsentServiceFactory() = default;

// static
UnifiedConsentService* UnifiedConsentServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<UnifiedConsentService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
UnifiedConsentServiceFactory* UnifiedConsentServiceFactory::GetInstance() {
  return base::Singleton<UnifiedConsentServiceFactory>::get();
}

void UnifiedConsentServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  UnifiedConsentService::RegisterPrefs(registry);
}

KeyedService* UnifiedConsentServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  sync_preferences::PrefServiceSyncable* pref_service =
      PrefServiceSyncableFromProfile(profile);
  // Record settings for pre- and post-UnifiedConsent users.
  RecordSettingsHistogram(pref_service);

  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (!sync_service)
    return nullptr;

  return new UnifiedConsentService(
      pref_service, IdentityManagerFactory::GetForProfile(profile),
      sync_service, GetSyncedServicePrefNames());
}

bool UnifiedConsentServiceFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

bool UnifiedConsentServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}
