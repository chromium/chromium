// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/test_support/fake_invalidation_listener.h"

namespace invalidation {

void FakeInvalidationListener::Shutdown() {
  invalidations_state_ = invalidation::InvalidationsExpected::kMaybe;
  if (observer_) {
    observer_->OnExpectationChanged(invalidations_state_);
  }
}

void FakeInvalidationListener::FireInvalidation(
    const invalidation::DirectInvalidation& invalidation) {
  if (observer_ && observer_->GetType() == invalidation.type()) {
    observer_->OnInvalidationReceived(invalidation);
  }
}

void FakeInvalidationListener::AddObserver(Observer* handler) {
  observer_ = handler;
  observer_->OnExpectationChanged(invalidations_state_);
}

bool FakeInvalidationListener::HasObserver(const Observer* handler) const {
  return observer_ == handler;
}

void FakeInvalidationListener::RemoveObserver(const Observer* handler) {
  observer_ = nullptr;
}

void FakeInvalidationListener::Start(invalidation::RegistrationTokenHandler*) {
  invalidations_state_ = invalidation::InvalidationsExpected::kYes;
  if (observer_) {
    observer_->OnExpectationChanged(invalidations_state_);
  }
}

}  // namespace invalidation
