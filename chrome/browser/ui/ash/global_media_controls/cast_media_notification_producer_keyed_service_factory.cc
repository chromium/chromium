// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/global_media_controls/cast_media_notification_producer_keyed_service_factory.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/global_media_controls/cast_media_notification_producer_keyed_service.h"
#include "components/user_manager/user_manager.h"
#include "media/base/media_switches.h"

CastMediaNotificationProducerKeyedServiceFactory::
    CastMediaNotificationProducerKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "CastMediaNotificationProducerKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(media_router::ChromeMediaRouterFactory::GetInstance());
}
CastMediaNotificationProducerKeyedServiceFactory::
    ~CastMediaNotificationProducerKeyedServiceFactory() = default;

// static
CastMediaNotificationProducerKeyedServiceFactory*
CastMediaNotificationProducerKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<CastMediaNotificationProducerKeyedServiceFactory>
      factory;
  return factory.get();
}

// static
CastMediaNotificationProducerKeyedService*
CastMediaNotificationProducerKeyedServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CastMediaNotificationProducerKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

std::unique_ptr<KeyedService> CastMediaNotificationProducerKeyedServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  if (!media_router::MediaRouterEnabled(context)) {
    return nullptr;
  }
  return std::make_unique<CastMediaNotificationProducerKeyedService>(
      Profile::FromBrowserContext(context));
}

bool CastMediaNotificationProducerKeyedServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool CastMediaNotificationProducerKeyedServiceFactory::
    ServiceIsNULLWhileTesting() const {
  return true;
}
