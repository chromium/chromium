// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class HoldingSpaceKeyedService;

// Factory class for browser context keyed HoldingSpace services.
class HoldingSpaceKeyedServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static HoldingSpaceKeyedServiceFactory* GetInstance();

  HoldingSpaceKeyedService* GetService(content::BrowserContext* context);

 protected:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

 private:
  friend base::NoDestructor<HoldingSpaceKeyedServiceFactory>;

  HoldingSpaceKeyedServiceFactory();
  HoldingSpaceKeyedServiceFactory(
      const HoldingSpaceKeyedServiceFactory& other) = delete;
  HoldingSpaceKeyedServiceFactory& operator=(
      const HoldingSpaceKeyedServiceFactory& other) = delete;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_FACTORY_H_
