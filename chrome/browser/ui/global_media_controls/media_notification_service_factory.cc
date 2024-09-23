// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"

MediaNotificationServiceFactory::MediaNotificationServiceFactory()
    : ProfileKeyedServiceFactory(
          "MediaNotificationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

MediaNotificationServiceFactory::~MediaNotificationServiceFactory() = default;

// static
MediaNotificationServiceFactory*
MediaNotificationServiceFactory::GetInstance() {
  static base::NoDestructor<MediaNotificationServiceFactory> instance;
  return instance.get();
}

// static
MediaNotificationService* MediaNotificationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MediaNotificationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

std::unique_ptr<KeyedService>
MediaNotificationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  bool show_from_all_profiles = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  show_from_all_profiles = true;
#endif
  return std::make_unique<MediaNotificationService>(
      Profile::FromBrowserContext(context), show_from_all_profiles);
}
