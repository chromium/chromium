// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/profile_invalidation_provider.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace invalidation {

ProfileInvalidationProvider::ProfileInvalidationProvider() = default;

ProfileInvalidationProvider::ProfileInvalidationProvider(
    std::unique_ptr<IdentityProvider> identity_provider,
    InvalidationListenerFactory invalidation_listener_factory)
    : invalidation_listener_factory_(std::move(invalidation_listener_factory)) {
}

ProfileInvalidationProvider::~ProfileInvalidationProvider() = default;

InvalidationListener* ProfileInvalidationProvider::GetInvalidationListener(
    int64_t project_number) {
  if (!invalidation_listener_factory_) {
    return nullptr;
  }

  auto& listener = project_number_to_invalidation_listener_[project_number];

  if (!listener) {
    listener = invalidation_listener_factory_.Run(
        project_number, "ProfileInvalidationProvider");
  }

  return listener.get();
}

void ProfileInvalidationProvider::Shutdown() {
  project_number_to_invalidation_listener_.clear();
  invalidation_listener_factory_.Reset();
}

// static
void ProfileInvalidationProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kInvalidationClientIDCache);
}

}  // namespace invalidation
