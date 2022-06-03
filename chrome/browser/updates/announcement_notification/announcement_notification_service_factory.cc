// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_service_factory.h"

#include <memory>

#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"  // nogncheck
#include "chrome/browser/profiles/incognito_helpers.h"  // nogncheck
#include "chrome/browser/profiles/profile.h"            // nogncheck
#include "chrome/browser/updates/announcement_notification/announcement_notification_delegate.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"
#include "chrome/browser/updates/announcement_notification/empty_announcement_notification_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if defined(OS_ANDROID)
#include "chrome/browser/updates/announcement_notification/announcement_notification_delegate_android.h"
#else
#include "chrome/browser/updates/announcement_notification/announcement_notification_delegate.h"
#endif  // OS_ANDROID

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

KeyedService* AnnouncementNotificationServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord()) {
    return new EmptyAnnouncementNotificationService();
  }

  Profile* profile = Profile::FromBrowserContext(context);
  PrefService* pref = profile->GetPrefs();
#if defined(OS_ANDROID)
  auto delegate = std::make_unique<AnnouncementNotificationDelegateAndroid>();
#else
  NotificationDisplayService* display_service =
      static_cast<NotificationDisplayService*>(
          NotificationDisplayServiceFactory::GetInstance()->GetForProfile(
              profile));
  auto delegate =
      std::make_unique<AnnouncementNotificationDelegate>(display_service);
#endif  // OS_ANDROID
  return AnnouncementNotificationService::Create(
      profile, pref, std::move(delegate), base::DefaultClock::GetInstance());
}

content::BrowserContext*
AnnouncementNotificationServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

AnnouncementNotificationServiceFactory::AnnouncementNotificationServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AnnouncementNotificationService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

AnnouncementNotificationServiceFactory::
    ~AnnouncementNotificationServiceFactory() = default;
