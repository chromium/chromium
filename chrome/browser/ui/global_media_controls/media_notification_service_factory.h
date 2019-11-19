// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class MediaNotificationService;

class MediaNotificationServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  MediaNotificationServiceFactory(const MediaNotificationServiceFactory&) =
      delete;
  MediaNotificationServiceFactory& operator=(
      const MediaNotificationServiceFactory&) = delete;

  static MediaNotificationServiceFactory* GetInstance();

  static MediaNotificationService* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<MediaNotificationServiceFactory>;

  MediaNotificationServiceFactory();
  ~MediaNotificationServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_FACTORY_H_
