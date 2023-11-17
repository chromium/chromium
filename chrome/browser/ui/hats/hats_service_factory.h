// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_HATS_HATS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"

class Profile;

class HatsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  HatsServiceFactory(const HatsServiceFactory&) = delete;
  HatsServiceFactory& operator=(const HatsServiceFactory&) = delete;

  static HatsService* GetForProfile(Profile* profile, bool create_if_necessary);

  static HatsServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<HatsServiceFactory>;

  HatsServiceFactory();
  ~HatsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_SERVICE_FACTORY_H_
