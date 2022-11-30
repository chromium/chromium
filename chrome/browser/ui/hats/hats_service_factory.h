// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_HATS_HATS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class HatsService;
class Profile;

class HatsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  HatsServiceFactory(const HatsServiceFactory&) = delete;
  HatsServiceFactory& operator=(const HatsServiceFactory&) = delete;

  static HatsService* GetForProfile(Profile* profile, bool create_if_necessary);
  static HatsServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<HatsServiceFactory>;

  HatsServiceFactory();
  ~HatsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_SERVICE_FACTORY_H_
