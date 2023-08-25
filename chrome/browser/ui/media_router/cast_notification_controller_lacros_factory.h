// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_NOTIFICATION_CONTROLLER_LACROS_FACTORY_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_NOTIFICATION_CONTROLLER_LACROS_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace media_router {

class CastNotificationControllerLacrosFactory
    : public ProfileKeyedServiceFactory {
 public:
  CastNotificationControllerLacrosFactory();
  CastNotificationControllerLacrosFactory(
      const CastNotificationControllerLacrosFactory&) = delete;
  CastNotificationControllerLacrosFactory& operator=(
      const CastNotificationControllerLacrosFactory&) = delete;
  ~CastNotificationControllerLacrosFactory() override;

  static CastNotificationControllerLacrosFactory* GetInstance();

 private:
  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_NOTIFICATION_CONTROLLER_LACROS_FACTORY_H_
