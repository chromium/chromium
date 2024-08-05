// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_model_factory.h"

#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/passkey_affiliation_source_adapter.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/sync/base/features.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/webauthn/core/browser/passkey_sync_bridge.h"

PasskeyModelFactory* PasskeyModelFactory::GetInstance() {
  static base::NoDestructor<PasskeyModelFactory> instance;
  return instance.get();
}

webauthn::PasskeyModel* PasskeyModelFactory::GetForProfile(Profile* profile) {
  return static_cast<webauthn::PasskeyModel*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PasskeyModelFactory::PasskeyModelFactory()
    : ProfileKeyedServiceFactory(
          "PasskeyModel",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Enable PasskeyModel for guest profiles. Guest profiles are
              // never signed in so they don't have have access to GPM passkeys,
              // but this simplifies handling by clients.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(AffiliationServiceFactory::GetInstance());
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

PasskeyModelFactory::~PasskeyModelFactory() = default;

std::unique_ptr<KeyedService>
PasskeyModelFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials));
  auto sync_bridge = std::make_unique<webauthn::PasskeySyncBridge>(
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());
  // Do not instantiate the affiliation service for guest profiles, since the
  // password manager does not run for them.
  if (!profile->IsGuestSession()) {
    std::unique_ptr<password_manager::PasskeyAffiliationSourceAdapter> adapter =
        std::make_unique<password_manager::PasskeyAffiliationSourceAdapter>(
            sync_bridge.get());
    AffiliationServiceFactory::GetForProfile(profile)->RegisterSource(
        std::move(adapter));
  }
  return sync_bridge;
}
