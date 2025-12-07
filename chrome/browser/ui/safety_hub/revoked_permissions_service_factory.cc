// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/revoked_permissions_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/revoked_permissions_service.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"

// static
RevokedPermissionsServiceFactory*
RevokedPermissionsServiceFactory::GetInstance() {
  static base::NoDestructor<RevokedPermissionsServiceFactory> instance;
  return instance.get();
}

// static
RevokedPermissionsService* RevokedPermissionsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<RevokedPermissionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

RevokedPermissionsServiceFactory::RevokedPermissionsServiceFactory()
    : ProfileKeyedServiceFactory(
          "RevokedPermissionsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(site_engagement::SiteEngagementServiceFactory::GetInstance());
}

RevokedPermissionsServiceFactory::~RevokedPermissionsServiceFactory() = default;

std::unique_ptr<KeyedService>
RevokedPermissionsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<RevokedPermissionsService>(
      context, Profile::FromBrowserContext(context)->GetPrefs());
}

bool RevokedPermissionsServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool RevokedPermissionsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
