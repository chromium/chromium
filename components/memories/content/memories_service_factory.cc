// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memories/content/memories_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/memories/core/memories_service.h"

// static
memories::MemoriesService* MemoriesServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<memories::MemoriesService*>(
      GetInstance().GetServiceForBrowserContext(browser_context, true));
}

// static
MemoriesServiceFactory& MemoriesServiceFactory::GetInstance() {
  static base::NoDestructor<MemoriesServiceFactory> instance;
  return *instance;
}

MemoriesServiceFactory::MemoriesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "MemoriesService",
          BrowserContextDependencyManager::GetInstance()) {}

MemoriesServiceFactory::~MemoriesServiceFactory() = default;

KeyedService* MemoriesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new memories::MemoriesService();
}

content::BrowserContext* MemoriesServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Give incognito its own isolated service.
  return context;
}
