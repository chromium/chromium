// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/local_search_service_sync_proxy_factory.h"

#include "chromeos/components/local_search_service/local_search_service_sync_factory.h"
#include "chromeos/components/local_search_service/local_search_service_sync_proxy.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace local_search_service {

// static
LocalSearchServiceSyncProxy*
LocalSearchServiceSyncProxyFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LocalSearchServiceSyncProxy*>(
      LocalSearchServiceSyncProxyFactory::GetInstance()
          ->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
LocalSearchServiceSyncProxyFactory*
LocalSearchServiceSyncProxyFactory::GetInstance() {
  static base::NoDestructor<LocalSearchServiceSyncProxyFactory> instance;
  return instance.get();
}

LocalSearchServiceSyncProxyFactory::LocalSearchServiceSyncProxyFactory()
    : BrowserContextKeyedServiceFactory(
          "LocalSearchServiceSyncProxy",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(LocalSearchServiceSyncFactory::GetInstance());
}

LocalSearchServiceSyncProxyFactory::~LocalSearchServiceSyncProxyFactory() =
    default;

content::BrowserContext*
LocalSearchServiceSyncProxyFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // The service should exist in incognito mode.
  return context;
}

KeyedService* LocalSearchServiceSyncProxyFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(context);
  return new LocalSearchServiceSyncProxy(
      LocalSearchServiceSyncFactory::GetForBrowserContext(context));
}

}  // namespace local_search_service
}  // namespace chromeos
