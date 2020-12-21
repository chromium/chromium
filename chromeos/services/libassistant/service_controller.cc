// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/service_controller.h"

#include <memory>

#include "base/check.h"
#include "chromeos/services/assistant/public/cpp/migration/assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"

namespace chromeos {
namespace libassistant {

using mojom::ServiceState;

ServiceController::ServiceController(
    assistant::AssistantManagerServiceDelegate* delegate,
    assistant_client::PlatformApi* platform_api)
    : delegate_(delegate), platform_api_(platform_api), receiver_(this) {}

ServiceController::~ServiceController() {
  // Ensure all our observers know this service is no longer running.
  // This will be a noop if we're already stopped.
  Stop();
}

void ServiceController::Bind(
    mojo::PendingReceiver<mojom::ServiceController> receiver) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
}

void ServiceController::SetInitializeCallback(InitializeCallback callback) {
  initialize_callback_ = std::move(callback);
}

void ServiceController::Start(const std::string& libassistant_config) {
  if (state_ != ServiceState::kStopped)
    return;

  assistant_manager_ =
      delegate_->CreateAssistantManager(platform_api_, libassistant_config);
  assistant_manager_internal_ =
      delegate_->UnwrapAssistantManagerInternal(assistant_manager_.get());
  libassistant_v1_api_ = std::make_unique<assistant::LibassistantV1Api>(
      assistant_manager_.get(), assistant_manager_internal_);

  if (initialize_callback_) {
    std::move(initialize_callback_)
        .Run(assistant_manager(), assistant_manager_internal());
  }

  assistant_manager()->Start();

  SetStateAndInformObservers(ServiceState::kStarted);

  for (auto& observer : assistant_manager_observers_) {
    observer.OnAssistantManagerCreated(assistant_manager(),
                                       assistant_manager_internal());
  }
}

void ServiceController::Stop() {
  if (state_ == ServiceState::kStopped)
    return;

  SetStateAndInformObservers(ServiceState::kStopped);

  for (auto& observer : assistant_manager_observers_) {
    observer.OnDestroyingAssistantManager(assistant_manager(),
                                          assistant_manager_internal());
  }

  libassistant_v1_api_ = nullptr;
  assistant_manager_ = nullptr;
  assistant_manager_internal_ = nullptr;
}

void ServiceController::AddAndFireStateObserver(
    mojo::PendingRemote<mojom::StateObserver> pending_observer) {
  mojo::Remote<mojom::StateObserver> observer(std::move(pending_observer));

  observer->OnStateChanged(state_);

  state_observers_.Add(std::move(observer));
}

void ServiceController::AddAndFireAssistantManagerObserver(
    AssistantManagerObserver* observer) {
  DCHECK(observer);

  assistant_manager_observers_.AddObserver(observer);

  if (IsStarted()) {
    observer->OnAssistantManagerCreated(assistant_manager(),
                                        assistant_manager_internal());
  }
}

void ServiceController::RemoveAssistantManagerObserver(
    AssistantManagerObserver* observer) {
  assistant_manager_observers_.RemoveObserver(observer);
}

bool ServiceController::IsStarted() const {
  return state_ != mojom::ServiceState::kStopped;
}

assistant_client::AssistantManager* ServiceController::assistant_manager() {
  return assistant_manager_.get();
}

assistant_client::AssistantManagerInternal*
ServiceController::assistant_manager_internal() {
  return assistant_manager_internal_;
}

void ServiceController::SetStateAndInformObservers(
    mojom::ServiceState new_state) {
  DCHECK_NE(state_, new_state);

  state_ = new_state;

  for (auto& observer : state_observers_)
    observer->OnStateChanged(state_);
}

}  // namespace libassistant
}  // namespace chromeos
