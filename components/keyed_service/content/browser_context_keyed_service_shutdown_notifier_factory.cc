// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"

BrowserContextKeyedServiceShutdownNotifierFactory::
    BrowserContextKeyedServiceShutdownNotifierFactory(const char* name)
    : BrowserContextKeyedServiceFactory(
          name,
          BrowserContextDependencyManager::GetInstance()) {
}
BrowserContextKeyedServiceShutdownNotifierFactory::
    ~BrowserContextKeyedServiceShutdownNotifierFactory() {
}

KeyedServiceShutdownNotifier*
BrowserContextKeyedServiceShutdownNotifierFactory::Get(
    content::BrowserContext* context) {
  return static_cast<KeyedServiceShutdownNotifier*>(
      GetServiceForBrowserContext(context, true));
}

std::unique_ptr<KeyedService>
  BrowserContextKeyedServiceShutdownNotifierFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique <KeyedServiceShutdownNotifier>();
}

content::BrowserContext*
BrowserContextKeyedServiceShutdownNotifierFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
