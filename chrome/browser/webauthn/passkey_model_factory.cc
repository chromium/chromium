// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_model_factory.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "components/sync/base/features.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/webauthn/core/browser/passkey_sync_bridge.h"

PasskeyModelFactory* PasskeyModelFactory::GetInstance() {
  static base::NoDestructor<PasskeyModelFactory> instance;
  return instance.get();
}

PasskeyModel* PasskeyModelFactory::GetForProfile(Profile* profile) {
  return static_cast<PasskeyModel*>(
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
}

PasskeyModelFactory::~PasskeyModelFactory() = default;

KeyedService* PasskeyModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials));
  return new PasskeySyncBridge(ModelTypeStoreServiceFactory::GetForProfile(
                                   Profile::FromBrowserContext(context))
                                   ->GetStoreFactory());
}
