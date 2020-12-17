// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/test_support/fake_service_controller.h"

#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

FakeServiceController::FakeServiceController() : receiver_(this) {}
FakeServiceController::~FakeServiceController() = default;

void FakeServiceController::SetState(State new_state) {
  DCHECK_NE(state_, new_state);

  state_ = new_state;

  for (auto& observer : state_observers_)
    observer->OnStateChanged(state_);
}

void FakeServiceController::Bind(
    mojo::PendingReceiver<libassistant::mojom::ServiceController>
        pending_receiver) {
  EXPECT_FALSE(receiver_.is_bound());
  receiver_.Bind(std::move(pending_receiver));
}

void FakeServiceController::Unbind() {
  // All mojom objects must now be unbound, as that needs to happen on the
  // same thread as they were bound (which is the background thread).
  receiver_.reset();
  state_observers_.Clear();
}

void FakeServiceController::SetInitializeCallback(InitializeCallback callback) {
  initialize_callback_ = std::move(callback);
}

void FakeServiceController::BlockStartCalls() {
  // This lock will be release in |UnblockStartCalls|.
  start_mutex_.lock();
}

void FakeServiceController::UnblockStartCalls() {
  start_mutex_.unlock();
}

void FakeServiceController::Start(const std::string& libassistant_config) {
  libassistant_config_ = libassistant_config;

  // Will block if |BlockStartCalls| was invoked.
  std::lock_guard<std::mutex> lock(start_mutex_);

  if (initialize_callback_) {
    std::move(initialize_callback_)
        .Run(LibassistantV1Api::Get()->assistant_manager(),
             LibassistantV1Api::Get()->assistant_manager_internal());
  }

  SetState(State::kStarted);
}

void FakeServiceController::Stop() {
  SetState(State::kStopped);
}

void FakeServiceController::AddAndFireStateObserver(
    mojo::PendingRemote<libassistant::mojom::StateObserver> pending_observer) {
  mojo::Remote<libassistant::mojom::StateObserver> observer(
      std::move(pending_observer));

  observer->OnStateChanged(state_);

  state_observers_.Add(std::move(observer));
}

}  // namespace assistant

}  // namespace chromeos
