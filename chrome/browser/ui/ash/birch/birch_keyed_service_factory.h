// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_KEYED_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class BirchKeyedService;

// Factory class for `BirchKeyedService`. Creates instances of that
// service for regular profiles only.
class BirchKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  BirchKeyedServiceFactory(const BirchKeyedServiceFactory&) = delete;
  BirchKeyedServiceFactory& operator=(const BirchKeyedServiceFactory&) = delete;

  static BirchKeyedServiceFactory* GetInstance();

  BirchKeyedService* GetService(content::BrowserContext* context);

 protected:
  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  friend base::NoDestructor<BirchKeyedServiceFactory>;
  BirchKeyedServiceFactory();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_KEYED_SERVICE_FACTORY_H_
