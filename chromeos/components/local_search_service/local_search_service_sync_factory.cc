// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/local_search_service_sync_factory.h"

#include "chromeos/components/local_search_service/local_search_service_sync.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace local_search_service {

LocalSearchServiceSync* LocalSearchServiceSyncFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LocalSearchServiceSync*>(
      LocalSearchServiceSyncFactory::GetInstance()->GetServiceForBrowserContext(
          context, true /* create */));
}

LocalSearchServiceSyncFactory* LocalSearchServiceSyncFactory::GetInstance() {
  return base::Singleton<LocalSearchServiceSyncFactory>::get();
}

LocalSearchServiceSyncFactory::LocalSearchServiceSyncFactory()
    : BrowserContextKeyedServiceFactory(
          "LocalSearchServiceSync",
          BrowserContextDependencyManager::GetInstance()) {}

LocalSearchServiceSyncFactory::~LocalSearchServiceSyncFactory() = default;

content::BrowserContext* LocalSearchServiceSyncFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // The service should exist in incognito mode.
  return context;
}

KeyedService* LocalSearchServiceSyncFactory::BuildServiceInstanceFor(
    content::BrowserContext* /* context */) const {
  return new LocalSearchServiceSync();
}

}  // namespace local_search_service
}  // namespace chromeos
