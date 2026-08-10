// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNIVERSAL_OPTOUT_UNIVERSAL_OPTOUT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UNIVERSAL_OPTOUT_UNIVERSAL_OPTOUT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace universal_optout {

class UniversalOptOutService;

class UniversalOptOutServiceFactory : public ProfileKeyedServiceFactory {
 public:
  UniversalOptOutServiceFactory(const UniversalOptOutServiceFactory&) = delete;
  UniversalOptOutServiceFactory& operator=(
      const UniversalOptOutServiceFactory&) = delete;

  static UniversalOptOutServiceFactory* GetInstance();
  static UniversalOptOutService* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<UniversalOptOutServiceFactory>;

  UniversalOptOutServiceFactory();
  ~UniversalOptOutServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace universal_optout

#endif  // CHROME_BROWSER_UNIVERSAL_OPTOUT_UNIVERSAL_OPTOUT_SERVICE_FACTORY_H_
