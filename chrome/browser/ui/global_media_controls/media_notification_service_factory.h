// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class MediaNotificationService;

class MediaNotificationServiceFactory : public ProfileKeyedServiceFactory {
 public:
  MediaNotificationServiceFactory(const MediaNotificationServiceFactory&) =
      delete;
  MediaNotificationServiceFactory& operator=(
      const MediaNotificationServiceFactory&) = delete;

  static MediaNotificationServiceFactory* GetInstance();

  static MediaNotificationService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<MediaNotificationServiceFactory>;

  MediaNotificationServiceFactory();
  ~MediaNotificationServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_FACTORY_H_
