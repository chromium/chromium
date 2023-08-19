// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/cast_notification_controller_lacros_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/media_router/cast_notification_controller_lacros.h"

namespace media_router {

CastNotificationControllerLacrosFactory::
    CastNotificationControllerLacrosFactory()
    : ProfileKeyedServiceFactory(
          "CastNotificationControllerLacros",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(media_router::ChromeMediaRouterFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}
CastNotificationControllerLacrosFactory::
    ~CastNotificationControllerLacrosFactory() = default;

// static
CastNotificationControllerLacrosFactory*
CastNotificationControllerLacrosFactory::GetInstance() {
  static base::NoDestructor<CastNotificationControllerLacrosFactory> factory;
  return factory.get();
}

KeyedService* CastNotificationControllerLacrosFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!media_router::MediaRouterEnabled(context)) {
    return nullptr;
  }
  return new CastNotificationControllerLacros(
      Profile::FromBrowserContext(context));
}

bool CastNotificationControllerLacrosFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool CastNotificationControllerLacrosFactory::ServiceIsNULLWhileTesting()
    const {
  // CastNotificationControllerLacros depends on MediaRouter that needs an IO
  // thread which is missing in many tests. So we disable it in tests.
  return true;
}

}  // namespace media_router
