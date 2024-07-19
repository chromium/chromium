// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/profile_invalidation_provider.h"

#include <utility>

#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/invalidation_factory.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace invalidation {

ProfileInvalidationProvider::ProfileInvalidationProvider(
    std::unique_ptr<IdentityProvider> identity_provider,
    InvalidationServiceOrListenerFactory
        invalidation_service_or_listener_factory)
    : identity_provider_(std::move(identity_provider)),
      invalidation_service_or_listener_factory_(
          std::move(invalidation_service_or_listener_factory)) {}

ProfileInvalidationProvider::~ProfileInvalidationProvider() = default;

IdentityProvider* ProfileInvalidationProvider::GetIdentityProvider() {
  return identity_provider_.get();
}

std::variant<InvalidationService*, InvalidationListener*>
ProfileInvalidationProvider::GetInvalidationServiceOrListener(
    const std::string& sender_id,
    const std::string& project_id) {
  DCHECK(invalidation_service_or_listener_factory_);

  if (!sender_id_to_invalidation_service_or_listener_.contains(
          {sender_id, project_id})) {
    sender_id_to_invalidation_service_or_listener_[{sender_id, project_id}] =
        invalidation_service_or_listener_factory_.Run(
            sender_id, project_id, "ProfileInvalidationProvider");
  }
  return invalidation::UniquePointerVariantToPointer(
      sender_id_to_invalidation_service_or_listener_[{sender_id, project_id}]);
}

void ProfileInvalidationProvider::Shutdown() {
  sender_id_to_invalidation_service_or_listener_.clear();
  invalidation_service_or_listener_factory_.Reset();
}

// static
void ProfileInvalidationProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kInvalidationClientIDCache);
}

}  // namespace invalidation
