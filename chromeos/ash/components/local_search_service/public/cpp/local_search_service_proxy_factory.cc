// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"

#include "base/no_destructor.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash::local_search_service {

namespace {

PrefService* local_state = nullptr;

}  // namespace

// static
LocalSearchServiceProxy* LocalSearchServiceProxyFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  DCHECK(local_state);
  auto* local_search_service_proxy = static_cast<LocalSearchServiceProxy*>(
      LocalSearchServiceProxyFactory::GetInstance()
          ->GetServiceForBrowserContext(context, /*create=*/true));
  local_search_service_proxy->SetLocalState(local_state);
  return local_search_service_proxy;
}

// static
LocalSearchServiceProxyFactory* LocalSearchServiceProxyFactory::GetInstance() {
  static base::NoDestructor<LocalSearchServiceProxyFactory> instance;
  return instance.get();
}

void LocalSearchServiceProxyFactory::SetLocalState(
    PrefService* local_state_pref_service) {
  DCHECK(local_state_pref_service);
  local_state = local_state_pref_service;
}

LocalSearchServiceProxyFactory::LocalSearchServiceProxyFactory()
    : BrowserContextKeyedServiceFactory(
          "LocalSearchServiceProxy",
          BrowserContextDependencyManager::GetInstance()) {}

LocalSearchServiceProxyFactory::~LocalSearchServiceProxyFactory() = default;

content::BrowserContext* LocalSearchServiceProxyFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // The service should exist in incognito mode.
  return context;
}

std::unique_ptr<KeyedService>
LocalSearchServiceProxyFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* /*context*/) const {
  return std::make_unique<LocalSearchServiceProxy>();
}

}  // namespace ash::local_search_service
