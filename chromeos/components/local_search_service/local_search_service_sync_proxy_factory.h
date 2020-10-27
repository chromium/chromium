// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_SYNC_PROXY_FACTORY_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_SYNC_PROXY_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace chromeos {
namespace local_search_service {

class LocalSearchServiceSyncProxy;

class LocalSearchServiceSyncProxyFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static LocalSearchServiceSyncProxy* GetForBrowserContext(
      content::BrowserContext* context);
  static LocalSearchServiceSyncProxyFactory* GetInstance();

  LocalSearchServiceSyncProxyFactory(
      const LocalSearchServiceSyncProxyFactory&) = delete;
  LocalSearchServiceSyncProxyFactory& operator=(
      const LocalSearchServiceSyncProxyFactory&) = delete;

 private:
  friend class base::NoDestructor<LocalSearchServiceSyncProxyFactory>;

  LocalSearchServiceSyncProxyFactory();
  ~LocalSearchServiceSyncProxyFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_SYNC_PROXY_FACTORY_H_
