// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_model_factory.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/password_manager/affiliations_prefetcher_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/sync/base/features.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_service.h"
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
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasskeysPrefetchAffiliations)) {
    DependsOn(AffiliationsPrefetcherFactory::GetInstance());
  }
}

PasskeyModelFactory::~PasskeyModelFactory() = default;

std::unique_ptr<KeyedService>
PasskeyModelFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials));
  auto sync_bridge = std::make_unique<webauthn::PasskeySyncBridge>(
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasskeysPrefetchAffiliations)) {
    AffiliationsPrefetcherFactory::GetForProfile(profile)->RegisterPasskeyModel(
        sync_bridge.get());
  }
  return sync_bridge;
}
