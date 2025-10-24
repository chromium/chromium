// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_REVOKED_PERMISSIONS_OS_NOTIFICATION_DISPLAY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_REVOKED_PERMISSIONS_OS_NOTIFICATION_DISPLAY_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class RevokedPermissionsOSNotificationDisplayManager;
class Profile;

class RevokedPermissionsOSNotificationDisplayManagerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static RevokedPermissionsOSNotificationDisplayManager* GetForProfile(
      Profile* profile);
  static RevokedPermissionsOSNotificationDisplayManagerFactory* GetInstance();

  // Non-copyable, non-moveable.
  RevokedPermissionsOSNotificationDisplayManagerFactory(
      const RevokedPermissionsOSNotificationDisplayManagerFactory&) = delete;
  RevokedPermissionsOSNotificationDisplayManagerFactory& operator=(
      const RevokedPermissionsOSNotificationDisplayManagerFactory&) = delete;

 private:
  friend base::NoDestructor<
      RevokedPermissionsOSNotificationDisplayManagerFactory>;

  RevokedPermissionsOSNotificationDisplayManagerFactory();
  ~RevokedPermissionsOSNotificationDisplayManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_REVOKED_PERMISSIONS_OS_NOTIFICATION_DISPLAY_MANAGER_FACTORY_H_
