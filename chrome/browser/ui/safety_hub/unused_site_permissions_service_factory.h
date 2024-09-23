// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class UnusedSitePermissionsService;

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class BrowserContext;
}

class UnusedSitePermissionsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static UnusedSitePermissionsServiceFactory* GetInstance();

  static UnusedSitePermissionsService* GetForProfile(Profile* profile);

  // Non-copyable, non-moveable.
  UnusedSitePermissionsServiceFactory(
      const UnusedSitePermissionsServiceFactory&) = delete;
  UnusedSitePermissionsServiceFactory& operator=(
      const UnusedSitePermissionsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<UnusedSitePermissionsServiceFactory>;

  UnusedSitePermissionsServiceFactory();
  ~UnusedSitePermissionsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

#if BUILDFLAG(IS_ANDROID)
  bool ServiceIsCreatedWithBrowserContext() const override;
#endif  // BUILDFLAG(ANDROID)
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_SERVICE_FACTORY_H_
