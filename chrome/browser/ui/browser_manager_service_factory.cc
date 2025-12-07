// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_manager_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
BrowserManagerService* BrowserManagerServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BrowserManagerService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
BrowserManagerServiceFactory* BrowserManagerServiceFactory::GetInstance() {
  static base::NoDestructor<BrowserManagerServiceFactory> factory;
  return factory.get();
}

BrowserManagerServiceFactory::BrowserManagerServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "BrowserManagerService",
          BrowserContextDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
BrowserManagerServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<BrowserManagerService>(
      Profile::FromBrowserContext(context));
}

content::BrowserContext* BrowserManagerServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Only instantiate the service for Profiles that support Browser instances.
  if (!Profile::FromBrowserContext(context)->AllowsBrowserWindows()) {
    return nullptr;
  }
  return context;
}

BrowserManagerServiceFactory::~BrowserManagerServiceFactory() = default;
