// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/passkey_unlock_manager.h"

namespace webauthn {

// static
PasskeyUnlockManager* PasskeyUnlockManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PasskeyUnlockManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PasskeyUnlockManagerFactory* PasskeyUnlockManagerFactory::GetInstance() {
  static base::NoDestructor<PasskeyUnlockManagerFactory> instance;
  return instance.get();
}

PasskeyUnlockManagerFactory::PasskeyUnlockManagerFactory()
    : ProfileKeyedServiceFactory(
          "PasskeyUnlockManager",
          ProfileSelections::Builder()
              // PasskeyUnlockManager is created for regular profiles but not
              // for Incognito profiles.
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(PasskeyModelFactory::GetInstance());
  DependsOn(EnclaveManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

PasskeyUnlockManagerFactory::~PasskeyUnlockManagerFactory() = default;

std::unique_ptr<KeyedService>
PasskeyUnlockManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<PasskeyUnlockManager>(profile);
}

}  // namespace webauthn
