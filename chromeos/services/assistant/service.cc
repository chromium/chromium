// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/service.h"

#include <algorithm>
#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "build/buildflag.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/assistant_settings_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/services/assistant/assistant_manager_service_impl.h"
#include "chromeos/services/assistant/assistant_settings_manager_impl.h"
#include "chromeos/services/assistant/utils.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#else
#include "chromeos/services/assistant/fake_assistant_manager_service_impl.h"
#include "chromeos/services/assistant/fake_assistant_settings_manager_impl.h"
#endif

namespace chromeos {
namespace assistant {

namespace {

constexpr char kScopeAuthGcm[] = "https://www.googleapis.com/auth/gcm";
constexpr char kScopeAssistant[] =
    "https://www.googleapis.com/auth/assistant-sdk-prototype";

constexpr base::TimeDelta kMinTokenRefreshDelay =
    base::TimeDelta::FromMilliseconds(1000);
constexpr base::TimeDelta kMaxTokenRefreshDelay =
    base::TimeDelta::FromMilliseconds(60 * 1000);

}  // namespace

Service::Service(network::NetworkConnectionTracker* network_connection_tracker)
    : platform_binding_(this),
      session_observer_binding_(this),
      token_refresh_timer_(std::make_unique<base::OneShotTimer>()),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      power_manager_observer_(this),
      voice_interaction_observer_binding_(this),
      network_connection_tracker_(network_connection_tracker),
      weak_ptr_factory_(this) {
  registry_.AddInterface<mojom::AssistantPlatform>(base::BindRepeating(
      &Service::BindAssistantPlatformConnection, base::Unretained(this)));

  // TODO(xiaohuic): in MASH we will need to setup the dbus client if assistant
  // service runs in its own process.
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  power_manager_observer_.Add(power_manager_client);
}

Service::~Service() = default;

void Service::SetIdentityManagerForTesting(
    identity::mojom::IdentityManagerPtr identity_manager) {
  identity_manager_ = std::move(identity_manager);
}

void Service::SetAssistantManagerForTesting(
    std::unique_ptr<AssistantManagerService> assistant_manager_service) {
  assistant_manager_service_ = std::move(assistant_manager_service);
}

void Service::SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer) {
  token_refresh_timer_ = std::move(timer);
}

void Service::OnStart() {}

void Service::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void Service::BindAssistantConnection(mojom::AssistantRequest request) {
  DCHECK(assistant_manager_service_);
  bindings_.AddBinding(assistant_manager_service_.get(), std::move(request));
}

void Service::BindAssistantPlatformConnection(
    mojom::AssistantPlatformRequest request) {
  platform_binding_.Bind(std::move(request));
}

void Service::SuspendDone(const base::TimeDelta& sleep_duration) {
  // |token_refresh_timer_| may become stale during sleeping, so we immediately
  // request a new token to make sure it is fresh.
  if (token_refresh_timer_->IsRunning()) {
    token_refresh_timer_->AbandonAndStop();
    RequestAccessToken();
  }
}

void Service::OnSessionActivated(bool activated) {
  DCHECK(client_);
  session_active_ = activated;

  if (assistant_manager_service_->GetState() !=
      AssistantManagerService::State::RUNNING) {
    return;
  }

  client_->OnAssistantStatusChanged(activated /* running */);
  UpdateListeningState();
}

void Service::OnLockStateChanged(bool locked) {
  locked_ = locked;

  if (assistant_manager_service_->GetState() !=
      AssistantManagerService::State::RUNNING) {
    return;
  }

  UpdateListeningState();
}

void Service::OnVoiceInteractionSettingsEnabled(bool enabled) {
  settings_enabled_ = enabled;
  if (enabled && assistant_manager_service_->GetState() ==
                     AssistantManagerService::State::STOPPED) {
    // This will eventually trigger the actual start of assistant services
    // because they all depend on it.
    RequestAccessToken();
  } else if (!enabled && assistant_manager_service_->GetState() !=
                             AssistantManagerService::State::STOPPED) {
    assistant_manager_service_->Stop();
    client_->OnAssistantStatusChanged(false /* running */);
  }
}

void Service::OnVoiceInteractionHotwordEnabled(bool enabled) {
  if (hotword_enabled_ == enabled)
    return;
  hotword_enabled_ = enabled;

  if (assistant_manager_service_->GetState() !=
      AssistantManagerService::State::RUNNING) {
    return;
  }

  assistant_manager_service_->Stop();
  client_->OnAssistantStatusChanged(false /* running */);
  RequestAccessToken();
}

void Service::BindAssistantSettingsManager(
    mojom::AssistantSettingsManagerRequest request) {
  DCHECK(assistant_settings_manager_);
  assistant_settings_manager_->BindRequest(std::move(request));
}

void Service::RequestAccessToken() {
  VLOG(1) << "Start requesting access token.";
  GetIdentityManager()->GetPrimaryAccountInfo(base::BindOnce(
      &Service::GetPrimaryAccountInfoCallback, base::Unretained(this)));
}

identity::mojom::IdentityManager* Service::GetIdentityManager() {
  if (!identity_manager_) {
    context()->connector()->BindInterface(
        identity::mojom::kServiceName, mojo::MakeRequest(&identity_manager_));
  }
  return identity_manager_.get();
}

void Service::RetryRefreshToken() {
  base::TimeDelta backoff_delay =
      std::min(kMinTokenRefreshDelay *
                   (1 << (token_refresh_error_backoff_factor - 1)),
               kMaxTokenRefreshDelay) +
      base::RandDouble() * kMinTokenRefreshDelay;
  if (backoff_delay < kMaxTokenRefreshDelay)
    ++token_refresh_error_backoff_factor;
  token_refresh_timer_->Start(FROM_HERE, backoff_delay, this,
                              &Service::RequestAccessToken);
}

void Service::Init(mojom::ClientPtr client,
                   mojom::DeviceActionsPtr device_actions) {
  client_ = std::move(client);
  device_actions_ = std::move(device_actions);
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  context()->connector()->BindInterface(ash::mojom::kServiceName,
                                        &voice_interaction_controller_);
  ash::mojom::VoiceInteractionObserverPtr ptr;
  voice_interaction_observer_binding_.Bind(mojo::MakeRequest(&ptr));
  voice_interaction_controller_->IsHotwordEnabled(base::BindOnce(
      &Service::CreateAssistantManagerService, weak_ptr_factory_.GetWeakPtr()));
  voice_interaction_controller_->AddObserver(std::move(ptr));
  voice_interaction_controller_->IsSettingEnabled(
      base::BindOnce(&Service::OnVoiceInteractionSettingsEnabled,
                     weak_ptr_factory_.GetWeakPtr()));
#else
  assistant_manager_service_ =
      std::make_unique<FakeAssistantManagerServiceImpl>();
  RequestAccessToken();
#endif
}

void Service::GetPrimaryAccountInfoCallback(
    const base::Optional<AccountInfo>& account_info,
    const identity::AccountState& account_state) {
  if (!account_info.has_value() || !account_state.has_refresh_token ||
      account_info.value().gaia.empty()) {
    LOG(ERROR) << "Failed to retrieve primary account info.";
    RetryRefreshToken();
    return;
  }
  account_id_ = AccountId::FromUserEmailGaiaId(account_info.value().email,
                                               account_info.value().gaia);
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(kScopeAssistant);
  scopes.insert(kScopeAuthGcm);
  identity_manager_->GetAccessToken(
      account_info.value().account_id, scopes, "cros_assistant",
      base::BindOnce(&Service::GetAccessTokenCallback, base::Unretained(this)));
}

void Service::GetAccessTokenCallback(const base::Optional<std::string>& token,
                                     base::Time expiration_time,
                                     const GoogleServiceAuthError& error) {
  if (!token.has_value()) {
    LOG(ERROR) << "Failed to retrieve token, error: " << error.ToString();
    RetryRefreshToken();
    return;
  }

  DCHECK(assistant_manager_service_);
  switch (assistant_manager_service_->GetState()) {
    case AssistantManagerService::State::STOPPED:
      assistant_manager_service_->Start(
          token.value(),
          base::BindOnce(
              [](scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 base::OnceCallback<void()> callback) {
                task_runner->PostTask(FROM_HERE, std::move(callback));
              },
              main_thread_task_runner_,
              base::BindOnce(&Service::FinalizeAssistantManagerService,
                             weak_ptr_factory_.GetWeakPtr())));
      DVLOG(1) << "Request Assistant start";
      break;
    case AssistantManagerService::State::RUNNING:
      assistant_manager_service_->SetAccessToken(token.value());
      break;
    case AssistantManagerService::State::STARTED:
      // in the process of starting, no need to do anything.
      break;
  }

  token_refresh_timer_->Start(FROM_HERE, expiration_time - base::Time::Now(),
                              this, &Service::RequestAccessToken);
}

void Service::CreateAssistantManagerService(bool enable_hotword) {
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  hotword_enabled_ = enable_hotword;
  device::mojom::BatteryMonitorPtr battery_monitor;
  context()->connector()->BindInterface(device::mojom::kServiceName,
                                        mojo::MakeRequest(&battery_monitor));
  assistant_manager_service_ = std::make_unique<AssistantManagerServiceImpl>(
      context()->connector(), std::move(battery_monitor), this, enable_hotword,
      network_connection_tracker_);

  // Bind to Assistant controller in ash.
  context()->connector()->BindInterface(ash::mojom::kServiceName,
                                        &assistant_controller_);
  mojom::AssistantPtr ptr;
  BindAssistantConnection(mojo::MakeRequest(&ptr));
  assistant_controller_->SetAssistant(std::move(ptr));

  registry_.AddInterface<mojom::Assistant>(base::BindRepeating(
      &Service::BindAssistantConnection, base::Unretained(this)));

  assistant_settings_manager_ =
      assistant_manager_service_.get()->GetAssistantSettingsManager();
  registry_.AddInterface<mojom::AssistantSettingsManager>(base::BindRepeating(
      &Service::BindAssistantSettingsManager, base::Unretained(this)));
#endif
}

void Service::FinalizeAssistantManagerService() {
  DCHECK(assistant_manager_service_->GetState() ==
         AssistantManagerService::State::RUNNING);

  if (!session_observer_binding_)
    AddAshSessionObserver();

  // Double check settings enabled status to avoid racing issue.
  if (!settings_enabled_) {
    assistant_manager_service_->Stop();
    client_->OnAssistantStatusChanged(false /* running */);
    return;
  }

  client_->OnAssistantStatusChanged(true /* running */);
  UpdateListeningState();
  DVLOG(1) << "Assistant is running";

  // Double check hotword status to avoid racing issue.
  voice_interaction_controller_->IsHotwordEnabled(
      base::BindOnce(&Service::OnVoiceInteractionHotwordEnabled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void Service::AddAshSessionObserver() {
  ash::mojom::SessionControllerPtr session_controller;
  context()->connector()->BindInterface(ash::mojom::kServiceName,
                                        &session_controller);
  ash::mojom::SessionActivationObserverPtr observer;
  session_observer_binding_.Bind(mojo::MakeRequest(&observer));
  session_controller->AddSessionActivationObserverForAccountId(
      account_id_, std::move(observer));
}

void Service::UpdateListeningState() {
  bool should_listen = !locked_ && session_active_;
  DVLOG(1) << "Update assistant listening state: " << should_listen;
  assistant_manager_service_->EnableListening(should_listen);
}

}  // namespace assistant
}  // namespace chromeos
