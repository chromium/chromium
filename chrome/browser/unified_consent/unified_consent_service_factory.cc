// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unified_consent/unified_consent_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/commerce/core/pref_names.h"
#include "components/embedder_support/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
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
// on Desktop and chrome/android/java/res/xml/google_services_preferences.xml
// on Android.

std::vector<std::string> GetSyncedServicePrefNames() {
  return {
    prefs::kSearchSuggestEnabled, prefs::kSafeBrowsingEnabled,
        prefs::kSafeBrowsingScoutReportingEnabled,
        spellcheck::prefs::kSpellCheckUseSpellingService,
        commerce::kPriceEmailNotificationsEnabled,
#if BUILDFLAG(IS_ANDROID)
        prefs::kContextualSearchEnabled
#endif
  };
}

}  // namespace

UnifiedConsentServiceFactory::UnifiedConsentServiceFactory()
    : ProfileKeyedServiceFactory(
          "UnifiedConsentService",
          ProfileSelections::Builder()
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              // ChromeOS creates various profiles (login, lock screen...) that
              // do not have unified consent.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
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
  static base::NoDestructor<UnifiedConsentServiceFactory> instance;
  return instance.get();
}

void UnifiedConsentServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  UnifiedConsentService::RegisterPrefs(registry);
}

std::unique_ptr<KeyedService>
UnifiedConsentServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  sync_preferences::PrefServiceSyncable* pref_service =
      PrefServiceSyncableFromProfile(profile);
  // Record settings for pre- and post-UnifiedConsent users.
  RecordSettingsHistogram(pref_service);

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service)
    return nullptr;

  return std::make_unique<UnifiedConsentService>(
      pref_service, IdentityManagerFactory::GetForProfile(profile),
      sync_service, GetSyncedServicePrefNames());
}
