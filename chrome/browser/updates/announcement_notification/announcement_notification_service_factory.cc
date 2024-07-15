// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"  // nogncheck
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_delegate.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"
#include "chrome/browser/updates/announcement_notification/empty_announcement_notification_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/updates/announcement_notification/announcement_notification_delegate_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

// static
AnnouncementNotificationServiceFactory*
AnnouncementNotificationServiceFactory::GetInstance() {
  static base::NoDestructor<AnnouncementNotificationServiceFactory> instance;
  return instance.get();
}

// static
AnnouncementNotificationService*
AnnouncementNotificationServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AnnouncementNotificationService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true /* create */));
}

std::unique_ptr<KeyedService>
AnnouncementNotificationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord()) {
    return std::make_unique<EmptyAnnouncementNotificationService>();
  }

  Profile* profile = Profile::FromBrowserContext(context);
  PrefService* pref = profile->GetPrefs();
#if BUILDFLAG(IS_ANDROID)
  auto delegate = std::make_unique<AnnouncementNotificationDelegateAndroid>();
#else
  NotificationDisplayService* display_service =
      static_cast<NotificationDisplayService*>(
          NotificationDisplayServiceFactory::GetInstance()->GetForProfile(
              profile));
  auto delegate =
      std::make_unique<AnnouncementNotificationDelegate>(display_service);
#endif  // BUILDFLAG(IS_ANDROID)
  return AnnouncementNotificationService::Create(
      profile, pref, std::move(delegate), base::DefaultClock::GetInstance());
}

AnnouncementNotificationServiceFactory::AnnouncementNotificationServiceFactory()
    : ProfileKeyedServiceFactory(
          "AnnouncementNotificationService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

AnnouncementNotificationServiceFactory::
    ~AnnouncementNotificationServiceFactory() = default;
