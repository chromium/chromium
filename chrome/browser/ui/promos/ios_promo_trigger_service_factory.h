// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROMOS_IOS_PROMO_TRIGGER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_PROMOS_IOS_PROMO_TRIGGER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class IOSPromoTriggerService;
class Profile;

// Factory to get the IOSPromoTriggerService for a Profile.
class IOSPromoTriggerServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static IOSPromoTriggerService* GetForProfile(Profile* profile);
  static IOSPromoTriggerServiceFactory* GetInstance();

  IOSPromoTriggerServiceFactory(const IOSPromoTriggerServiceFactory&) = delete;
  IOSPromoTriggerServiceFactory& operator=(
      const IOSPromoTriggerServiceFactory&) = delete;

 private:
  friend base::NoDestructor<IOSPromoTriggerServiceFactory>;

  IOSPromoTriggerServiceFactory();
  ~IOSPromoTriggerServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_PROMOS_IOS_PROMO_TRIGGER_SERVICE_FACTORY_H_
