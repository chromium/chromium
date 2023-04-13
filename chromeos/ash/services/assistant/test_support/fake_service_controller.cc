// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/test_support/fake_service_controller.h"

#include "base/task/sequenced_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::assistant {

// A macro which ensures we are running on the mojom thread.
#define ENSURE_MOJOM_THREAD(method, ...)                                    \
  if (!mojom_task_runner_->RunsTasksInCurrentSequence()) {                  \
    mojom_task_runner_->PostTask(                                           \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }

FakeServiceController::FakeServiceController() {}
FakeServiceController::~FakeServiceController() = default;

void FakeServiceController::SetState(State new_state) {
  // SetState() is called from our unittests, but the observers are registered
  // on the mojom thread so we must switch threads.
  ENSURE_MOJOM_THREAD(&FakeServiceController::SetState, new_state);
  DCHECK_NE(state_, new_state);

  state_ = new_state;

  for (auto& observer : state_observers_)
    observer->OnStateChanged(state_);
}

void FakeServiceController::Bind(
    mojo::PendingReceiver<libassistant::mojom::ServiceController>
        service_receiver,
    mojo::PendingReceiver<libassistant::mojom::SettingsController>
        settings_receiver) {
  service_receiver_.Bind(std::move(service_receiver));
  settings_receiver_.Bind(std::move(settings_receiver));
}

void FakeServiceController::Unbind() {
  // All mojom objects must now be unbound, as that needs to happen on the
  // same thread as they were bound (which is the background thread).
  service_receiver_.reset();
  settings_receiver_.reset();
  state_observers_.Clear();
}

void FakeServiceController::BlockStartCalls() {
  // This lock will be release in |UnblockStartCalls|.
  start_mutex_.lock();
}

void FakeServiceController::UnblockStartCalls() {
  start_mutex_.unlock();
}

std::string FakeServiceController::access_token() {
  if (authentication_tokens_.size())
    return authentication_tokens_[0]->access_token;
  else
    return kNoValue;
}

std::string FakeServiceController::gaia_id() {
  if (authentication_tokens_.size())
    return authentication_tokens_[0]->gaia_id;
  else
    return kNoValue;
}

void FakeServiceController::Initialize(
    libassistant::mojom::BootupConfigPtr config,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory) {
  mojom_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  libassistant_config_ = std::move(config);

  authentication_tokens_ =
      std::move(libassistant_config_->authentication_tokens);
  dark_mode_enabled_ = libassistant_config_->dark_mode_enabled;
}

void FakeServiceController::Start() {
  // Will block if |BlockStartCalls| was invoked.
  std::lock_guard<std::mutex> lock(start_mutex_);

  SetState(State::kStarted);
}

void FakeServiceController::Stop() {
  // Post a delayed task to make it possible to set other state between
  // kStopping and kStopped.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeServiceController::SetState,
                     weak_factory_.GetWeakPtr(), State::kStopped),
      base::Milliseconds(1));
}

void FakeServiceController::ResetAllDataAndStop() {
  SetState(State::kStopped);
  has_data_been_reset_ = true;
}

void FakeServiceController::AddAndFireStateObserver(
    mojo::PendingRemote<libassistant::mojom::StateObserver> pending_observer) {
  mojo::Remote<libassistant::mojom::StateObserver> observer(
      std::move(pending_observer));

  observer->OnStateChanged(state_);

  state_observers_.Add(std::move(observer));
}

void FakeServiceController::SetAuthenticationTokens(
    std::vector<libassistant::mojom::AuthenticationTokenPtr> tokens) {
  authentication_tokens_ = std::move(tokens);
}

void FakeServiceController::SetDarkModeEnabled(bool value) {
  dark_mode_enabled_ = value;
}

void FakeServiceController::UpdateSettings(const std::string& settings,
                                           UpdateSettingsCallback callback) {
  // Callback must be called to satisfy the mojom contract.
  std::move(callback).Run(std::string());
}

void FakeServiceController::GetSettings(const std::string& selector,
                                        bool include_header,
                                        GetSettingsCallback callback) {
  // Callback must be called to satisfy the mojom contract.
  std::move(callback).Run(std::string());
}

}  // namespace ash::assistant
