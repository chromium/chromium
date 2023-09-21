// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class KeyedService;
class Profile;
class TabOrganizationService;

class TabOrganizationServiceFactory : public ProfileKeyedServiceFactory {
 public:
  TabOrganizationServiceFactory();
  TabOrganizationServiceFactory(const TabOrganizationServiceFactory&) = delete;
  void operator=(const TabOrganizationServiceFactory&) = delete;
  ~TabOrganizationServiceFactory() override;

  static TabOrganizationServiceFactory* GetInstance();
  static TabOrganizationService* GetForProfile(Profile* profile);

 protected:
  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  friend base::NoDestructor<TabOrganizationServiceFactory>;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_SERVICE_FACTORY_H_
