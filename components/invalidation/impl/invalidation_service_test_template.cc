// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_service_test_template.h"

namespace invalidation {

BoundFakeInvalidationHandler::BoundFakeInvalidationHandler(
    const InvalidationService& invalidator,
    const std::string& owner)
    : FakeInvalidationHandler(owner),
      invalidator_(invalidator),
      last_retrieved_state_(DEFAULT_INVALIDATION_ERROR) {}

BoundFakeInvalidationHandler::~BoundFakeInvalidationHandler() = default;

InvalidatorState BoundFakeInvalidationHandler::GetLastRetrievedState() const {
  return last_retrieved_state_;
}

void BoundFakeInvalidationHandler::OnInvalidatorStateChange(
    InvalidatorState state) {
  FakeInvalidationHandler::OnInvalidatorStateChange(state);
  last_retrieved_state_ = invalidator_->GetInvalidatorState();
}

// This suite is instantiated in binaries that use
// //components/invalidation/impl:test_support.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(InvalidationServiceTest);

}  // namespace invalidation
