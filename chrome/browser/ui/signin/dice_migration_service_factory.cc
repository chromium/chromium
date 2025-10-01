// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_migration_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/signin/dice_migration_service.h"
#include "components/signin/public/base/signin_switches.h"

namespace {

ProfileSelections BuildDiceMigrationServiceProfileSelections() {
  return base::FeatureList::IsEnabled(switches::kOfferMigrationToDiceUsers) ||
                 base::FeatureList::IsEnabled(switches::kForcedDiceMigration)
             ? ProfileSelections::BuildForRegularProfile()
             : ProfileSelections::BuildNoProfilesSelected();
}

}  // namespace

DiceMigrationServiceFactory::DiceMigrationServiceFactory()
    : ProfileKeyedServiceFactory("DiceMigrationService",
                                 BuildDiceMigrationServiceProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

DiceMigrationServiceFactory::~DiceMigrationServiceFactory() = default;

// static
DiceMigrationService* DiceMigrationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DiceMigrationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
DiceMigrationService* DiceMigrationServiceFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<DiceMigrationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
DiceMigrationServiceFactory* DiceMigrationServiceFactory::GetInstance() {
  static base::NoDestructor<DiceMigrationServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
DiceMigrationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<DiceMigrationService>(profile);
}

bool DiceMigrationServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // The service is created when the browser context is created to trigger the
  // migration flow.
  return true;
}

void DiceMigrationServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DiceMigrationService::RegisterProfilePrefs(registry);
}
