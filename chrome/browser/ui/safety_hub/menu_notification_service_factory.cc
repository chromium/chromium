// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"

// static
SafetyHubMenuNotificationServiceFactory*
SafetyHubMenuNotificationServiceFactory::GetInstance() {
  static base::NoDestructor<SafetyHubMenuNotificationServiceFactory> instance;
  return instance.get();
}

// static
SafetyHubMenuNotificationService*
SafetyHubMenuNotificationServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SafetyHubMenuNotificationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SafetyHubMenuNotificationServiceFactory::
    SafetyHubMenuNotificationServiceFactory()
    : ProfileKeyedServiceFactory(
          "SafetyHubMenuNotificationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(UnusedSitePermissionsServiceFactory::GetInstance());
}

SafetyHubMenuNotificationServiceFactory::
    ~SafetyHubMenuNotificationServiceFactory() = default;

std::unique_ptr<KeyedService>
SafetyHubMenuNotificationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  UnusedSitePermissionsService* unused_site_permissions_service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile);
  return std::make_unique<SafetyHubMenuNotificationService>(
      unused_site_permissions_service);
}
