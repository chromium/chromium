// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/profile_invalidation_provider.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "components/invalidation/invalidation_listener.h"
#include "components/keyed_service/core/keyed_service.h"

namespace invalidation {

ProfileInvalidationProvider::ProfileInvalidationProvider() = default;

ProfileInvalidationProvider::ProfileInvalidationProvider(
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

}  // namespace invalidation
