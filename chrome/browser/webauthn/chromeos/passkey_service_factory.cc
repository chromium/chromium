// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chromeos/passkey_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/browser/webauthn/chromeos/passkey_service.h"
#include "components/trusted_vault/frontend_trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "device/fido/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

PasskeyServiceFactory* PasskeyServiceFactory::GetInstance() {
  static base::NoDestructor<PasskeyServiceFactory> instance;
  return instance.get();
}  // namespace PasskeyServiceFactory::GetInstance()

PasskeyService* PasskeyServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PasskeyService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PasskeyServiceFactory::PasskeyServiceFactory()
    : ProfileKeyedServiceFactory(
          "PasskeyService",
          ProfileSelections::Builder()
              // GPM passkeys are shared between incognito and the original
              // profile.
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              // System profiles don't exist in Ash; in Lacros they render the
              // profile selection screen, which should not access GPM.
              .WithSystem(ProfileSelection::kNone)
              // The sign-in profile and lock screen should not access GPM.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(TrustedVaultServiceFactory::GetInstance());
}

PasskeyServiceFactory::~PasskeyServiceFactory() = default;

std::unique_ptr<KeyedService>
PasskeyServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(device::kChromeOsPasskeys)) {
    return nullptr;
  }

  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<PasskeyService>(
      IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      TrustedVaultServiceFactory::GetForProfile(profile)->GetTrustedVaultClient(
          trusted_vault::SecurityDomainId::kPasskeys),
      trusted_vault::NewFrontendTrustedVaultConnection(
          trusted_vault::SecurityDomainId::kPasskeys,
          IdentityManagerFactory::GetForProfile(profile),
          SystemNetworkContextManager::GetInstance()
              ->GetSharedURLLoaderFactory()));
}

}  // namespace chromeos
