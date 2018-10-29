// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/profile_invalidation_provider.h"

#include <utility>

#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace invalidation {

ProfileInvalidationProvider::ProfileInvalidationProvider(
    std::unique_ptr<InvalidationService> invalidation_service,
    std::unique_ptr<IdentityProvider> identity_provider)
    : identity_provider_(std::move(identity_provider)),
      invalidation_service_(std::move(invalidation_service)) {}

ProfileInvalidationProvider::~ProfileInvalidationProvider() {
}

InvalidationService* ProfileInvalidationProvider::GetInvalidationService() {
  return invalidation_service_.get();
}

IdentityProvider* ProfileInvalidationProvider::GetIdentityProvider() {
  return identity_provider_.get();
}

void ProfileInvalidationProvider::Shutdown() {
  invalidation_service_.reset();
}

// static
void ProfileInvalidationProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kInvalidationServiceUseGCMChannel,
      true);  // if no value in prefs, use GCM channel.

  registry->RegisterStringPref(prefs::kFCMInvalidationClientIDCache,
                               /*default_value=*/std::string());
}

}  // namespace invalidation
