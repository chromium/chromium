// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_REVOKED_PERMISSIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_REVOKED_PERMISSIONS_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class RevokedPermissionsService;

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class BrowserContext;
}

class RevokedPermissionsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static RevokedPermissionsServiceFactory* GetInstance();

  static RevokedPermissionsService* GetForProfile(Profile* profile);

  // Non-copyable, non-moveable.
  RevokedPermissionsServiceFactory(const RevokedPermissionsServiceFactory&) =
      delete;
  RevokedPermissionsServiceFactory& operator=(
      const RevokedPermissionsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<RevokedPermissionsServiceFactory>;

  RevokedPermissionsServiceFactory();
  ~RevokedPermissionsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_REVOKED_PERMISSIONS_SERVICE_FACTORY_H_
