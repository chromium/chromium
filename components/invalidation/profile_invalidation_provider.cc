// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/profile_invalidation_provider.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/legacy_topics_cleaner.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace invalidation {

ProfileInvalidationProvider::ProfileInvalidationProvider() = default;

ProfileInvalidationProvider::ProfileInvalidationProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<IdentityProvider> identity_provider,
    PrefService* pref_service,
    InvalidationListenerFactory invalidation_listener_factory)
    : invalidation_listener_factory_(std::move(invalidation_listener_factory)) {
  legacy_topics_cleaner_ = std::make_unique<invalidation::LegacyTopicsCleaner>(
      url_loader_factory, std::move(identity_provider), pref_service);
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

}  // namespace invalidation
