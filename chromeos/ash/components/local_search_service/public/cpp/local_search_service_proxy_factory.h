// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_CPP_LOCAL_SEARCH_SERVICE_PROXY_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_CPP_LOCAL_SEARCH_SERVICE_PROXY_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class PrefService;

namespace ash::local_search_service {

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
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash::local_search_service

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_CPP_LOCAL_SEARCH_SERVICE_PROXY_FACTORY_H_
