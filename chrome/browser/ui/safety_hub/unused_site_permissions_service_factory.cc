// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"

// static
UnusedSitePermissionsServiceFactory*
UnusedSitePermissionsServiceFactory::GetInstance() {
  static base::NoDestructor<UnusedSitePermissionsServiceFactory> instance;
  return instance.get();
}

// static
UnusedSitePermissionsService*
UnusedSitePermissionsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<UnusedSitePermissionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

UnusedSitePermissionsServiceFactory::UnusedSitePermissionsServiceFactory()
    : ProfileKeyedServiceFactory(
          "UnusedSitePermissionsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

UnusedSitePermissionsServiceFactory::~UnusedSitePermissionsServiceFactory() =
    default;

KeyedService* UnusedSitePermissionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* service = new UnusedSitePermissionsService(
      context, Profile::FromBrowserContext(context)->GetPrefs());
  return service;
}

#if BUILDFLAG(IS_ANDROID)
bool UnusedSitePermissionsServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return base::FeatureList::IsEnabled(features::kSafetyHub) ||
         base::FeatureList::IsEnabled(
             safe_browsing::kSafetyHubAbusiveNotificationRevocation);
}
#endif  // BUILDFLAG(IS_ANDROID)
