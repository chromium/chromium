// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_MANAGER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_BROWSER_MANAGER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class BrowserManagerService;
class Profile;

class BrowserManagerServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static BrowserManagerService* GetForProfile(Profile* profile);
  static BrowserManagerServiceFactory* GetInstance();

  BrowserManagerServiceFactory(const BrowserManagerServiceFactory&) = delete;
  BrowserManagerServiceFactory& operator=(const BrowserManagerServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<BrowserManagerServiceFactory>;

  BrowserManagerServiceFactory();
  ~BrowserManagerServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_BROWSER_MANAGER_SERVICE_FACTORY_H_
