// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/webapps/isolated_web_apps/service/isolated_web_app_browser_context_service_factory.h"

class Profile;

namespace web_app {

class IsolatedWebAppsOpenedTabsCounterService;

class IsolatedWebAppsOpenedTabsCounterServiceFactory
    : public IsolatedWebAppBrowserContextServiceFactory {
 public:
  static IsolatedWebAppsOpenedTabsCounterServiceFactory* GetInstance();
  static IsolatedWebAppsOpenedTabsCounterService* GetForProfile(
      Profile* profile);

 protected:
  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend base::NoDestructor<IsolatedWebAppsOpenedTabsCounterServiceFactory>;

  IsolatedWebAppsOpenedTabsCounterServiceFactory();
  ~IsolatedWebAppsOpenedTabsCounterServiceFactory() override = default;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_FACTORY_H_
