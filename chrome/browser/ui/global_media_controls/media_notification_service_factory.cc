// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"

MediaNotificationServiceFactory::MediaNotificationServiceFactory()
    : ProfileKeyedServiceFactory(
          "MediaNotificationService",
          ProfileSelections::BuildForRegularAndIncognito()) {}

MediaNotificationServiceFactory::~MediaNotificationServiceFactory() {}

// static
MediaNotificationServiceFactory*
MediaNotificationServiceFactory::GetInstance() {
  return base::Singleton<MediaNotificationServiceFactory>::get();
}

// static
MediaNotificationService* MediaNotificationServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MediaNotificationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* MediaNotificationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  bool show_from_all_profiles = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  show_from_all_profiles = true;
#endif
  return new MediaNotificationService(Profile::FromBrowserContext(context),
                                      show_from_all_profiles);
}
