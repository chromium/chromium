// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class KeyedService;
class Profile;
class TabDeclutterService;

class TabDeclutterServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static TabDeclutterServiceFactory* GetInstance();
  static TabDeclutterService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<TabDeclutterServiceFactory>;

  TabDeclutterServiceFactory();
  ~TabDeclutterServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DECLUTTER_SERVICE_FACTORY_H_
