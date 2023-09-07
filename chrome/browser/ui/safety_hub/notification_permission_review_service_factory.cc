// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"

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
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

NotificationPermissionsReviewServiceFactory::
    ~NotificationPermissionsReviewServiceFactory() = default;

std::unique_ptr<KeyedService> NotificationPermissionsReviewServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<NotificationPermissionsReviewService>(
      HostContentSettingsMapFactory::GetForProfile(context));
}
