// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/test_support/fake_invalidation_listener.h"

namespace invalidation {

FakeInvalidationListener::FakeInvalidationListener()
    : FakeInvalidationListener(kFakeProjectNumber) {}

FakeInvalidationListener::FakeInvalidationListener(int64_t project_number)
    : project_number_(project_number) {}

FakeInvalidationListener::~FakeInvalidationListener() = default;

void FakeInvalidationListener::Shutdown() {
  invalidations_state_ = invalidation::InvalidationsExpected::kMaybe;
  for (auto& observer : observers_) {
    observer.OnExpectationChanged(invalidations_state_);
  }
}

void FakeInvalidationListener::FireInvalidation(
    const invalidation::DirectInvalidation& invalidation) {
  for (auto& observer : observers_) {
    if (observer.GetType() == invalidation.type()) {
      observer.OnInvalidationReceived(invalidation);
    }
  }
}

void FakeInvalidationListener::AddObserver(Observer* handler) {
  observers_.AddObserver(handler);
  handler->OnExpectationChanged(invalidations_state_);
}

bool FakeInvalidationListener::HasObserver(const Observer* handler) const {
  return observers_.HasObserver(handler);
}

void FakeInvalidationListener::RemoveObserver(const Observer* handler) {
  observers_.RemoveObserver(handler);
}

void FakeInvalidationListener::Start(invalidation::RegistrationTokenHandler*) {
  invalidations_state_ = invalidation::InvalidationsExpected::kYes;
  for (auto& observer : observers_) {
    observer.OnExpectationChanged(invalidations_state_);
  }
}

int64_t FakeInvalidationListener::project_number() const {
  return project_number_;
}

}  // namespace invalidation
