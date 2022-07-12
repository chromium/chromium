// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_CPP_LOCAL_SEARCH_SERVICE_PROXY_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_CPP_LOCAL_SEARCH_SERVICE_PROXY_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class PrefService;

namespace chromeos {
namespace local_search_service {
class LocalSearchServiceProxy;

class LocalSearchServiceProxyFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static LocalSearchServiceProxy* GetForBrowserContext(
      content::BrowserContext* context);
  static LocalSearchServiceProxyFactory* GetInstance();

  void SetLocalState(PrefService* local_state_pref_service);

  LocalSearchServiceProxyFactory(const LocalSearchServiceProxyFactory&) =
      delete;
  LocalSearchServiceProxyFactory& operator=(
      const LocalSearchServiceProxyFactory&) = delete;

 private:
  friend class base::NoDestructor<LocalSearchServiceProxyFactory>;

  LocalSearchServiceProxyFactory();
  ~LocalSearchServiceProxyFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace local_search_service
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace local_search_service {
using ::chromeos::local_search_service::LocalSearchServiceProxyFactory;
}  // namespace local_search_service
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_CPP_LOCAL_SEARCH_SERVICE_PROXY_FACTORY_H_
