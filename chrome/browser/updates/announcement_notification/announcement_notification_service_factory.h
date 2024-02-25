// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class AnnouncementNotificationService;
class Profile;

// Factory to create FastTrackNotificationService.
class AnnouncementNotificationServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static AnnouncementNotificationServiceFactory* GetInstance();
  static AnnouncementNotificationService* GetForProfile(Profile* profile);

  AnnouncementNotificationServiceFactory(
      const AnnouncementNotificationServiceFactory&) = delete;
  AnnouncementNotificationServiceFactory& operator=(
      const AnnouncementNotificationServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<AnnouncementNotificationServiceFactory>;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  AnnouncementNotificationServiceFactory();
  ~AnnouncementNotificationServiceFactory() override;
};

#endif  // CHROME_BROWSER_UPDATES_ANNOUNCEMENT_NOTIFICATION_ANNOUNCEMENT_NOTIFICATION_SERVICE_FACTORY_H_
