// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"

HistorySyncOptinServiceFactory::HistorySyncOptinServiceFactory()
    : ProfileKeyedServiceFactory(
          "HistorySyncOptinService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

HistorySyncOptinServiceFactory::~HistorySyncOptinServiceFactory() = default;

// static
HistorySyncOptinService* HistorySyncOptinServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<HistorySyncOptinService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
HistorySyncOptinServiceFactory* HistorySyncOptinServiceFactory::GetInstance() {
  static base::NoDestructor<HistorySyncOptinServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
HistorySyncOptinServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<HistorySyncOptinService>(
      Profile::FromBrowserContext(context));
}

bool HistorySyncOptinServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
