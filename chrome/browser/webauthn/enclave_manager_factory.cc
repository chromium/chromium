// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/enclave_manager_factory.h"

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

scoped_refptr<network::SharedURLLoaderFactory>& GetTestURLLoader() {
  static base::NoDestructor<scoped_refptr<network::SharedURLLoaderFactory>>
      refptr;
  return *refptr.get();
}

EnclaveManagerInterface* EnclaveManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<EnclaveManagerInterface*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

EnclaveManager* EnclaveManagerFactory::GetAsEnclaveManagerForProfile(
    Profile* profile) {
  EnclaveManagerInterface* interface = GetForProfile(profile);
  if (!interface) {
    return nullptr;
  }
  return interface->GetEnclaveManager();
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
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

// static
void EnclaveManagerFactory::SetUrlLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  GetTestURLLoader() = factory;
}

// static
scoped_refptr<network::SharedURLLoaderFactory>
EnclaveManagerFactory::url_loader_override() {
  return GetTestURLLoader();
}

EnclaveManagerFactory::~EnclaveManagerFactory() = default;

std::unique_ptr<KeyedService>
EnclaveManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_override =
      GetTestURLLoader();
  Profile* const profile = Profile::FromBrowserContext(context);
  return std::make_unique<EnclaveManager>(
      /*base_dir=*/profile->GetPath(),
      IdentityManagerFactory::GetForProfile(profile),
      base::BindRepeating(
          [](base::WeakPtr<Profile> profile)
              -> network::mojom::NetworkContext* {
            if (!profile) {
              return nullptr;
            }
            return profile->GetDefaultStoragePartition()->GetNetworkContext();
          },
          profile->GetWeakPtr()),
      url_loader_override ? url_loader_override
                          : profile->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess());
}
