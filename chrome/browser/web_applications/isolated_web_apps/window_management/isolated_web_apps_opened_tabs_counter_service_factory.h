// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WINDOW_MANAGEMENT_ISOLATED_WEB_APPS_OPENED_TABS_COUNTER_SERVICE_FACTORY_H_

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service.h"

namespace web_app {

class IsolatedWebAppsOpenedTabsCounterServiceFactory
    : public ProfileKeyedServiceFactory {
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
