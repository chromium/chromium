// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/proxy/service_controller_proxy.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "chromeos/assistant/internal/cros_display_connection.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/assistant/proxy/libassistant_service_host.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/cpp/migration/assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "chromeos/services/libassistant/libassistant_service.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/internal_api/fuchsia_api_helper.h"

namespace chromeos {
namespace assistant {

// TODO(b/171748795): Most of the work that is done here right now (especially
// the work related to starting Libassistant) should be moved to the mojom
// service.

namespace {

using libassistant::mojom::ServiceState;

constexpr base::Feature kChromeOSAssistantDogfood{
    "ChromeOSAssistantDogfood", base::FEATURE_DISABLED_BY_DEFAULT};

constexpr char kServersideDogfoodExperimentId[] = "20347368";
constexpr char kServersideOpenAppExperimentId[] = "39651593";
constexpr char kServersideResponseProcessingV2ExperimentId[] = "1793869";

struct StartArguments {
  StartArguments() = default;
  StartArguments(StartArguments&&) = default;
  StartArguments& operator=(StartArguments&&) = default;
  ~StartArguments() = default;

  assistant_client::ActionModule* action_module;
  assistant_client::FuchsiaApiDelegate* fuchsia_api_delegate;
  assistant_client::AssistantManagerDelegate* assistant_manager_delegate;
  assistant_client::ConversationStateListener* conversation_state_listener;
  assistant_client::DeviceStateListener* device_state_listener;
  CrosDisplayConnection* display_connection;
  std::string libassistant_config;
  std::string locale;
  std::string locale_override;
  bool spoken_feedback_enabled;
  ServiceControllerProxy::AuthTokens auth_tokens;
};

void FillServerExperimentIds(std::vector<std::string>* server_experiment_ids) {
  if (base::FeatureList::IsEnabled(kChromeOSAssistantDogfood)) {
    server_experiment_ids->emplace_back(kServersideDogfoodExperimentId);
  }

  if (base::FeatureList::IsEnabled(features::kAssistantAppSupport))
    server_experiment_ids->emplace_back(kServersideOpenAppExperimentId);

  server_experiment_ids->emplace_back(
      kServersideResponseProcessingV2ExperimentId);
}

void SetServerExperiments(
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  std::vector<std::string> server_experiment_ids;
  FillServerExperimentIds(&server_experiment_ids);

  if (server_experiment_ids.size() > 0) {
    assistant_manager_internal->AddExtraExperimentIds(server_experiment_ids);
  }
}

void SetInternalOptions(
    assistant_client::AssistantManagerInternal* assistant_manager_internal,
    const std::string& locale,
    bool spoken_feedback_enabled) {
  auto* internal_options =
      assistant_manager_internal->CreateDefaultInternalOptions();
  SetAssistantOptions(internal_options, locale, spoken_feedback_enabled);

  internal_options->SetClientControlEnabled(
      assistant::features::IsRoutinesEnabled());

  if (!features::IsVoiceMatchDisabled())
    internal_options->EnableRequireVoiceMatchVerification();

  assistant_manager_internal->SetOptions(*internal_options, [](bool success) {
    DVLOG(2) << "set options: " << success;
  });
}

// TODO(b/171748795): This should all be migrated to the mojom service, which
// should be responsible for the complete creation of the Libassistant
// objects.
// Note: this method will be called from the mojom (background) thread.
void InitializeAssistantManager(
    StartArguments arguments,
    assistant_client::AssistantManager* assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  SetInternalOptions(assistant_manager_internal, arguments.locale,
                     arguments.spoken_feedback_enabled);
  assistant_manager_internal->SetDisplayConnection(
      arguments.display_connection);
  assistant_manager_internal->SetLocaleOverride(arguments.locale_override);
  assistant_manager_internal->RegisterActionModule(arguments.action_module);
  assistant_manager_internal->SetAssistantManagerDelegate(
      arguments.assistant_manager_delegate);
  assistant_manager_internal->GetFuchsiaApiHelperOrDie()->SetFuchsiaApiDelegate(
      arguments.fuchsia_api_delegate);
  assistant_manager->AddConversationStateListener(
      arguments.conversation_state_listener);
  assistant_manager->AddDeviceStateListener(arguments.device_state_listener);
  SetServerExperiments(assistant_manager_internal);
  assistant_manager->SetAuthTokens(arguments.auth_tokens);
}

}  // namespace

ServiceControllerProxy::ServiceControllerProxy(
    LibassistantServiceHost* host,
    mojo::PendingRemote<chromeos::libassistant::mojom::ServiceController>
        client)
    : host_(host),
      service_controller_remote_(std::move(client)),
      state_observer_receiver_(this) {
  service_controller_remote_->AddAndFireStateObserver(
      state_observer_receiver_.BindNewPipeAndPassRemote());
}

ServiceControllerProxy::~ServiceControllerProxy() = default;

void ServiceControllerProxy::Start(
    assistant_client::ActionModule* action_module,
    assistant_client::FuchsiaApiDelegate* fuchsia_api_delegate,
    assistant_client::AssistantManagerDelegate* assistant_manager_delegate,
    assistant_client::ConversationStateListener* conversation_state_listener,
    assistant_client::DeviceStateListener* device_state_listener,
    AssistantEventObserver* event_observer,
    const std::string& libassistant_config,
    const std::string& locale,
    const std::string& locale_override,
    bool spoken_feedback_enabled,
    const AuthTokens& auth_tokens,
    base::OnceClosure done_callback) {
  // Start can only be called once (unless Stop() was called).
  DCHECK_EQ(state_, State::kStopped);
  state_ = State::kStarting;

  pending_display_connection_ = std::make_unique<CrosDisplayConnection>(
      event_observer, /*feedback_ui_enabled=*/true,
      assistant::features::IsMediaSessionIntegrationEnabled());

  // The mojom service will create the |AssistantManager|.
  service_controller_remote_->Start(libassistant_config);

  // We need to initialize the |AssistantManager| once it's created and before
  // it's started, so we register a callback to do just that.
  StartArguments arguments;
  arguments.action_module = action_module;
  arguments.fuchsia_api_delegate = fuchsia_api_delegate;
  arguments.assistant_manager_delegate = assistant_manager_delegate;
  arguments.conversation_state_listener = conversation_state_listener;
  arguments.device_state_listener = device_state_listener;
  arguments.display_connection = pending_display_connection_.get();
  arguments.locale = locale;
  arguments.locale_override = locale_override;
  arguments.spoken_feedback_enabled = spoken_feedback_enabled;
  arguments.auth_tokens = auth_tokens;

  host_->SetInitializeCallback(
      base::BindOnce(InitializeAssistantManager, std::move(arguments)));

  on_start_done_callback_ = std::move(done_callback);
}

void ServiceControllerProxy::Stop() {
  // We can not cleanly stop if we're still starting.
  DCHECK_NE(state_, State::kStarting);
  state_ = State::kStopped;

  service_controller_remote_->Stop();
  // display_connection_ is used by the assistant manager and can only be
  // deleted once we have confirmation the assistant manager is gone.
  // so we do not reset it here but in |OnStateChanged| instead.
}

void ServiceControllerProxy::UpdateInternalOptions(
    const std::string& locale,
    bool spoken_feedback_enabled) {
  SetInternalOptions(assistant_manager_internal(), locale,
                     spoken_feedback_enabled);
}

void ServiceControllerProxy::SetAuthTokens(const AuthTokens& tokens) {
  assistant_manager()->SetAuthTokens(tokens);
}

bool ServiceControllerProxy::IsStarted() const {
  return state_ == State::kStarted;
}

void ServiceControllerProxy::OnStateChanged(ServiceState new_state) {
  DVLOG(1) << "Libassistant service state changed to " << new_state;

  switch (new_state) {
    case ServiceState::kStarted:
      FinishCreatingAssistant();
      break;
    case ServiceState::kRunning:
      NOTIMPLEMENTED();
      break;
    case ServiceState::kStopped:
      display_connection_ = nullptr;
      break;
  }
}

assistant_client::AssistantManager*
ServiceControllerProxy::assistant_manager() {
  auto* api = assistant::LibassistantV1Api::Get();
  return api ? api->assistant_manager() : nullptr;
}

assistant_client::AssistantManagerInternal*
ServiceControllerProxy::assistant_manager_internal() {
  auto* api = assistant::LibassistantV1Api::Get();
  return api ? api->assistant_manager_internal() : nullptr;
}

void ServiceControllerProxy::FinishCreatingAssistant() {
  if (state_ == State::kStopped) {
    // We can come here if the system went into shutdown while the mojom
    // service was busy starting Libassistant.
    // This means the |AssistantManager| could be destroyed at any second,
    // so we simply clean up and bail out.
    on_start_done_callback_.reset();
    pending_display_connection_ = nullptr;
    return;
  }

  DCHECK(on_start_done_callback_.has_value());
  DCHECK_NE(pending_display_connection_, nullptr);

  state_ = State::kStarted;
  display_connection_ = std::move(pending_display_connection_);
  std::move(on_start_done_callback_.value()).Run();
}

}  // namespace assistant
}  // namespace chromeos
