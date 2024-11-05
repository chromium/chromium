// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_TEST_SUPPORT_FAKE_INVALIDATION_LISTENER_H_
#define COMPONENTS_INVALIDATION_TEST_SUPPORT_FAKE_INVALIDATION_LISTENER_H_

#include <string>
#include <utility>

#include "components/invalidation/invalidation_listener.h"

namespace invalidation {

class FakeInvalidationListener : public invalidation::InvalidationListener {
 public:
  static constexpr char kFakeProjectNumber[] = "fake_project_number";

  FakeInvalidationListener();
  explicit FakeInvalidationListener(std::string project_number);
  ~FakeInvalidationListener() override = default;

  FakeInvalidationListener(const FakeInvalidationListener&) = delete;
  FakeInvalidationListener& operator=(const FakeInvalidationListener&) = delete;

  void Start() { Start(nullptr); }

  void Shutdown() override;

  void FireInvalidation(const invalidation::DirectInvalidation& invalidation);

  bool HasObserver(const Observer* handler) const override;

  const std::string& project_number() const override;

 private:
  void AddObserver(Observer* handler) override;

  void RemoveObserver(const Observer* handler) override;

  void Start(invalidation::RegistrationTokenHandler*) override;

  void SetRegistrationUploadStatus(
      RegistrationTokenUploadStatus status) override {}

  invalidation::InvalidationsExpected invalidations_state_ =
      invalidation::InvalidationsExpected::kMaybe;
  raw_ptr<Observer> observer_ = nullptr;

  const std::string project_number_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_TEST_SUPPORT_FAKE_INVALIDATION_LISTENER_H_
