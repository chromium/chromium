// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class BrowserContext;
}

class SafetyHubMenuNotificationServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static SafetyHubMenuNotificationServiceFactory* GetInstance();

  static SafetyHubMenuNotificationService* GetForProfile(Profile* profile);

  // Non-copyable, non-moveable.
  SafetyHubMenuNotificationServiceFactory(
      const SafetyHubMenuNotificationServiceFactory&) = delete;
  SafetyHubMenuNotificationServiceFactory& operator=(
      const SafetyHubMenuNotificationServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SafetyHubMenuNotificationServiceFactory>;

  SafetyHubMenuNotificationServiceFactory();
  ~SafetyHubMenuNotificationServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_MENU_NOTIFICATION_SERVICE_FACTORY_H_
