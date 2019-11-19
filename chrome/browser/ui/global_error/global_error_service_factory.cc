// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_error/global_error_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
GlobalErrorService* GlobalErrorServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<GlobalErrorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GlobalErrorServiceFactory* GlobalErrorServiceFactory::GetInstance() {
  return base::Singleton<GlobalErrorServiceFactory>::get();
}

GlobalErrorServiceFactory::GlobalErrorServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "GlobalErrorService",
          BrowserContextDependencyManager::GetInstance()) {}

GlobalErrorServiceFactory::~GlobalErrorServiceFactory() = default;

KeyedService* GlobalErrorServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new GlobalErrorService();
}

content::BrowserContext* GlobalErrorServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
