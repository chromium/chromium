// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_ios_promotion/sms_service_factory.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/desktop_ios_promotion/sms_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
SMSServiceFactory* SMSServiceFactory::GetInstance() {
  return base::Singleton<SMSServiceFactory>::get();
}

// static
SMSService* SMSServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SMSService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

KeyedService* SMSServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new SMSService(
      IdentityManagerFactory::GetForProfile(profile),
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess());
}

SMSServiceFactory::SMSServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SMSServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

SMSServiceFactory::~SMSServiceFactory() {}
