// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/enclave_manager_factory.h"

#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"

// static
EnclaveManager* EnclaveManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<EnclaveManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
EnclaveManagerFactory* EnclaveManagerFactory::GetInstance() {
  static base::NoDestructor<EnclaveManagerFactory> instance;
  return instance.get();
}

EnclaveManagerFactory::EnclaveManagerFactory()
    : ProfileKeyedServiceFactory(
          "EnclaveManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

EnclaveManagerFactory::~EnclaveManagerFactory() = default;

std::unique_ptr<KeyedService>
EnclaveManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  return std::make_unique<EnclaveManager>(
      /*base_dir=*/profile->GetPath(),
      IdentityManagerFactory::GetForProfile(profile),
      SystemNetworkContextManager::GetInstance()->GetContext(),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}
