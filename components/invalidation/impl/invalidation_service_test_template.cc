// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_service_test_template.h"

namespace internal {

BoundFakeInvalidationHandler::BoundFakeInvalidationHandler(
    const invalidation::InvalidationService& invalidator)
    : invalidator_(invalidator),
      last_retrieved_state_(syncer::DEFAULT_INVALIDATION_ERROR) {}

BoundFakeInvalidationHandler::~BoundFakeInvalidationHandler() = default;

syncer::InvalidatorState
BoundFakeInvalidationHandler::GetLastRetrievedState() const {
  return last_retrieved_state_;
}

void BoundFakeInvalidationHandler::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  FakeInvalidationHandler::OnInvalidatorStateChange(state);
  last_retrieved_state_ = invalidator_.GetInvalidatorState();
}

}  // namespace internal
