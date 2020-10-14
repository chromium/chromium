// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/local_search_service_proxy_factory.h"

#include "chromeos/components/local_search_service/local_search_service_proxy.h"
#include "chromeos/components/local_search_service/local_search_service_sync_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace local_search_service {

// static
LocalSearchServiceProxy* LocalSearchServiceProxyFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LocalSearchServiceProxy*>(
      LocalSearchServiceProxyFactory::GetInstance()
          ->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
LocalSearchServiceProxyFactory* LocalSearchServiceProxyFactory::GetInstance() {
  static base::NoDestructor<LocalSearchServiceProxyFactory> instance;
  return instance.get();
}

LocalSearchServiceProxyFactory::LocalSearchServiceProxyFactory()
    : BrowserContextKeyedServiceFactory(
          "LocalSearchServiceProxy",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(LocalSearchServiceSyncFactory::GetInstance());
}

LocalSearchServiceProxyFactory::~LocalSearchServiceProxyFactory() = default;

content::BrowserContext* LocalSearchServiceProxyFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // The service should exist in incognito mode.
  return context;
}

KeyedService* LocalSearchServiceProxyFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(context);
  return new LocalSearchServiceProxy(
      LocalSearchServiceSyncFactory::GetForBrowserContext(context));
}

}  // namespace local_search_service
}  // namespace chromeos
