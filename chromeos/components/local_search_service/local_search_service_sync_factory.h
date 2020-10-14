// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_SYNC_FACTORY_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_SYNC_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace chromeos {

namespace local_search_service {

class LocalSearchServiceSync;

class LocalSearchServiceSyncFactory : public BrowserContextKeyedServiceFactory {
 public:
  static LocalSearchServiceSync* GetForBrowserContext(
      content::BrowserContext* context);

  static LocalSearchServiceSyncFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<LocalSearchServiceSyncFactory>;

  LocalSearchServiceSyncFactory();
  ~LocalSearchServiceSyncFactory() override;

  LocalSearchServiceSyncFactory(const LocalSearchServiceSyncFactory&) = delete;
  LocalSearchServiceSyncFactory& operator=(
      const LocalSearchServiceSyncFactory&) = delete;

  // BrowserContextKeyedServiceFactory overrides.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_SYNC_FACTORY_H_
