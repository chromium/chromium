// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/service_controller.h"

#include <memory>

#include "base/check.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/cpp/migration/assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "chromeos/services/libassistant/util.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"

namespace chromeos {
namespace libassistant {

namespace {

using mojom::ServiceState;

std::vector<std::pair<std::string, std::string>> ToAuthTokens(
    const std::vector<mojom::AuthenticationTokenPtr>& mojo_tokens) {
  std::vector<std::pair<std::string, std::string>> result;

  for (const auto& token : mojo_tokens)
    result.emplace_back(token->gaia_id, token->access_token);

  return result;
}

std::string ToLibassistantConfig(const mojom::BootupConfig& bootup_config) {
  return CreateLibAssistantConfig(bootup_config.s3_server_uri_override,
                                  bootup_config.device_id_override,
                                  bootup_config.log_in_home_dir);
}

}  // namespace

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

void ServiceController::Initialize(mojom::BootupConfigPtr config) {
  if (assistant_manager_ != nullptr) {
    LOG(ERROR) << "Initialize() should only be called once.";
    return;
  }

  assistant_manager_ = delegate_->CreateAssistantManager(
      platform_api_, ToLibassistantConfig(*config));
  assistant_manager_internal_ =
      delegate_->UnwrapAssistantManagerInternal(assistant_manager_.get());

  for (auto& observer : assistant_manager_observers_) {
    observer.OnAssistantManagerCreated(assistant_manager(),
                                       assistant_manager_internal());
  }
}

void ServiceController::Start() {
  if (state_ != ServiceState::kStopped)
    return;

  DCHECK(IsInitialized()) << "Initialize() must be called before Start()";

  if (initialize_callback_) {
    std::move(initialize_callback_)
        .Run(assistant_manager(), assistant_manager_internal());
  }

  assistant_manager()->Start();

  libassistant_v1_api_ = std::make_unique<assistant::LibassistantV1Api>(
      assistant_manager_.get(), assistant_manager_internal_);

  SetStateAndInformObservers(ServiceState::kStarted);

  for (auto& observer : assistant_manager_observers_) {
    observer.OnAssistantManagerStarted(assistant_manager(),
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

void ServiceController::SetLocaleOverride(const std::string& value) {
  DCHECK(IsInitialized());

  assistant_manager_internal()->SetLocaleOverride(value);
}

void ServiceController::SetInternalOptions(const std::string& locale,
                                           bool spoken_feedback_enabled) {
  DCHECK(IsInitialized());

  auto* internal_options =
      assistant_manager_internal()->CreateDefaultInternalOptions();
  assistant::SetAssistantOptions(internal_options, locale,
                                 spoken_feedback_enabled);

  internal_options->SetClientControlEnabled(
      assistant::features::IsRoutinesEnabled());

  if (!assistant::features::IsVoiceMatchDisabled())
    internal_options->EnableRequireVoiceMatchVerification();

  assistant_manager_internal()->SetOptions(*internal_options, [](bool success) {
    DVLOG(2) << "set options: " << success;
  });
}

void ServiceController::SetAuthenticationTokens(
    std::vector<mojom::AuthenticationTokenPtr> tokens) {
  DCHECK(IsInitialized());

  assistant_manager()->SetAuthTokens(ToAuthTokens(tokens));
}

void ServiceController::AddAndFireAssistantManagerObserver(
    AssistantManagerObserver* observer) {
  DCHECK(observer);

  assistant_manager_observers_.AddObserver(observer);

  if (IsInitialized()) {
    observer->OnAssistantManagerCreated(assistant_manager(),
                                        assistant_manager_internal());
  }
  if (IsStarted()) {
    observer->OnAssistantManagerStarted(assistant_manager(),
                                        assistant_manager_internal());
  }
}

void ServiceController::RemoveAssistantManagerObserver(
    AssistantManagerObserver* observer) {
  assistant_manager_observers_.RemoveObserver(observer);
}

bool ServiceController::IsStarted() const {
  return state_ != ServiceState::kStopped;
}

bool ServiceController::IsInitialized() const {
  return assistant_manager_ != nullptr;
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
