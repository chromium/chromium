// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"

// static
NotificationPermissionsReviewServiceFactory*
NotificationPermissionsReviewServiceFactory::GetInstance() {
  static base::NoDestructor<NotificationPermissionsReviewServiceFactory>
      instance;
  return instance.get();
}

// static
NotificationPermissionsReviewService*
NotificationPermissionsReviewServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<NotificationPermissionsReviewService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

NotificationPermissionsReviewServiceFactory::
    NotificationPermissionsReviewServiceFactory()
    : ProfileKeyedServiceFactory(
          "NotificationPermissionsReviewService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(site_engagement::SiteEngagementServiceFactory::GetInstance());
}

NotificationPermissionsReviewServiceFactory::
    ~NotificationPermissionsReviewServiceFactory() = default;

std::unique_ptr<KeyedService> NotificationPermissionsReviewServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(
          Profile::FromBrowserContext(context));
  return std::make_unique<NotificationPermissionsReviewService>(
      HostContentSettingsMapFactory::GetForProfile(context),
      engagement_service);
}

#if BUILDFLAG(IS_ANDROID)
bool NotificationPermissionsReviewServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return base::FeatureList::IsEnabled(features::kSafetyHub);
}
#endif  // BUILDFLAG(IS_ANDROID)
