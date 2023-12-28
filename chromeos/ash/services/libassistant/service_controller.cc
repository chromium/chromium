// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/service_controller.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/chromium_api_delegate.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/libassistant_factory.h"
#include "chromeos/ash/services/libassistant/settings_controller.h"
#include "chromeos/ash/services/libassistant/util.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"

namespace ash::libassistant {

namespace {

using mojom::ServiceState;

// A macro which ensures we are running on the mojom thread.
#define ENSURE_MOJOM_THREAD(method, ...)                                    \
  if (!mojom_task_runner_->RunsTasksInCurrentSequence()) {                  \
    mojom_task_runner_->PostTask(                                           \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }

BASE_FEATURE(kChromeOSAssistantDogfood,
             "ChromeOSAssistantDogfood",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr char kServersideDogfoodExperimentId[] = "20347368";
constexpr char kServersideOpenAppExperimentId[] = "39651593";
constexpr char kServersideResponseProcessingV2ExperimentId[] = "1793869";

std::string ToLibassistantConfig(const mojom::BootupConfig& bootup_config) {
  return CreateLibAssistantConfig(bootup_config.s3_server_uri_override,
                                  bootup_config.device_id_override);
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
CreatePendingURLLoaderFactory(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        url_loader_factory_remote) {
  // First create a wrapped factory that can accept the pending remote.
  auto pending_url_loader_factory =
      std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
          std::move(url_loader_factory_remote));
  auto wrapped_factory = network::SharedURLLoaderFactory::Create(
      std::move(pending_url_loader_factory));

  // Then move it into a cross thread factory, as the url loader factory will be
  // used from internal Libassistant threads.
  return std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
      std::move(wrapped_factory));
}

void FillServerExperimentIds(std::vector<std::string>* server_experiment_ids) {
  if (base::FeatureList::IsEnabled(kChromeOSAssistantDogfood)) {
    server_experiment_ids->emplace_back(kServersideDogfoodExperimentId);
  }

  if (base::FeatureList::IsEnabled(assistant::features::kAssistantAppSupport))
    server_experiment_ids->emplace_back(kServersideOpenAppExperimentId);

  server_experiment_ids->emplace_back(
      kServersideResponseProcessingV2ExperimentId);
}

void SetServerExperiments(AssistantClient* assistant_client) {
  std::vector<std::string> server_experiment_ids;
  FillServerExperimentIds(&server_experiment_ids);

  if (server_experiment_ids.size() > 0) {
    assistant_client->AddExperimentIds(server_experiment_ids);
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//  ServiceController
////////////////////////////////////////////////////////////////////////////////

ServiceController::ServiceController(LibassistantFactory* factory)
    : libassistant_factory_(*factory) {
  DCHECK(factory);
}

ServiceController::~ServiceController() {
  // Ensure all our observers know this service is no longer running.
  // This will be a noop if we're already stopped.
  Stop();
}

void ServiceController::Bind(
    mojo::PendingReceiver<mojom::ServiceController> receiver,
    mojom::SettingsController* settings_controller) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  settings_controller_ = settings_controller;
}

void ServiceController::Initialize(
    mojom::BootupConfigPtr config,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory) {
  if (assistant_client_) {
    LOG(ERROR) << "Initialize() should only be called once.";
    return;
  }

  auto assistant_manager = libassistant_factory_->CreateAssistantManager(
      ToLibassistantConfig(*config));
  assistant_client_ = AssistantClient::Create(std::move(assistant_manager));

  DCHECK(settings_controller_);
  settings_controller_->SetAuthenticationTokens(
      std::move(config->authentication_tokens));
  settings_controller_->SetLocale(config->locale);
  settings_controller_->SetHotwordEnabled(config->hotword_enabled);
  settings_controller_->SetSpokenFeedbackEnabled(
      config->spoken_feedback_enabled);
  settings_controller_->SetDarkModeEnabled(config->dark_mode_enabled);

  CreateAndRegisterChromiumApiDelegate(std::move(url_loader_factory));
  for (auto& observer : assistant_client_observers_) {
    observer.OnAssistantClientCreated(assistant_client_.get());
  }
}

void ServiceController::Start() {
  if (state_ != ServiceState::kStopped)
    return;

  DCHECK(IsInitialized()) << "Initialize() must be called before Start()";
  DVLOG(1) << "Starting Libassistant service";

  // |this| will outlive |assistant_client_|.
  assistant_client_->StartServices(/*services_status_observer=*/this);
}

void ServiceController::Stop() {
  if (state_ == ServiceState::kStopped)
    return;

  DVLOG(1) << "Stopping Libassistant service";

  for (auto& observer : assistant_client_observers_) {
    observer.OnDestroyingAssistantClient(assistant_client_.get());
  }

  assistant_client_ = nullptr;
  chromium_api_delegate_ = nullptr;

  DVLOG(1) << "Stopped Libassistant service";
  SetStateAndInformObservers(ServiceState::kStopped);

  for (auto& observer : assistant_client_observers_)
    observer.OnAssistantClientDestroyed();
}

void ServiceController::ResetAllDataAndStop() {
  if (assistant_client_) {
    DVLOG(1) << "Resetting all Libassistant data";
    assistant_client_->ResetAllDataAndShutdown();
  }
  Stop();
}

void ServiceController::AddAndFireStateObserver(
    mojo::PendingRemote<mojom::StateObserver> pending_observer) {
  mojo::Remote<mojom::StateObserver> observer(std::move(pending_observer));

  observer->OnStateChanged(state_);

  state_observers_.Add(std::move(observer));
}

void ServiceController::OnServicesStatusChanged(ServicesStatus status) {
  switch (status) {
    case ServicesStatus::ONLINE_ALL_SERVICES_AVAILABLE:
      OnAllServicesReady();
      break;
    case ServicesStatus::ONLINE_BOOTING_UP:
      // Configing internal options or other essential services that are
      // supported during bootup stage should happen here.
      OnServicesBootingUp();
      break;
    case ServicesStatus::OFFLINE:
      // No action needed.
      break;
  }
}

void ServiceController::AddAndFireAssistantClientObserver(
    AssistantClientObserver* observer) {
  DCHECK(observer);

  assistant_client_observers_.AddObserver(observer);

  if (IsInitialized()) {
    observer->OnAssistantClientCreated(assistant_client_.get());
  }
  // Note we do send the |OnAssistantClientStarted| event even if the service
  // is currently running, to ensure that an observer that only observes
  // |OnAssistantClientStarted| will not miss a currently running instance
  // when it is being added.
  if (IsStarted()) {
    observer->OnAssistantClientStarted(assistant_client_.get());
  }
  if (IsRunning()) {
    observer->OnAssistantClientRunning(assistant_client_.get());
  }
}

void ServiceController::RemoveAssistantClientObserver(
    AssistantClientObserver* observer) {
  assistant_client_observers_.RemoveObserver(observer);
}

void ServiceController::RemoveAllAssistantClientObservers() {
  assistant_client_observers_.Clear();
}

bool ServiceController::IsStarted() const {
  switch (state_) {
    case ServiceState::kStopped:
    case ServiceState::kDisconnected:
      return false;
    case ServiceState::kStarted:
    case ServiceState::kRunning:
      return true;
  }
}

bool ServiceController::IsInitialized() const {
  return assistant_client_ != nullptr;
}

bool ServiceController::IsRunning() const {
  switch (state_) {
    case ServiceState::kStopped:
    case ServiceState::kStarted:
    case ServiceState::kDisconnected:
      return false;
    case ServiceState::kRunning:
      return true;
  }
}

AssistantClient* ServiceController::assistant_client() {
  return assistant_client_.get();
}

void ServiceController::OnAllServicesReady() {
  DVLOG(1) << "Libassistant services are ready.";

  SetServerExperiments(assistant_client_.get());

  // Notify observers on Libassistant services ready.
  SetStateAndInformObservers(mojom::ServiceState::kRunning);

  for (auto& observer : assistant_client_observers_)
    observer.OnAssistantClientRunning(assistant_client_.get());
}

void ServiceController::OnServicesBootingUp() {
  DVLOG(1) << "Started Libassistant service";

  // We set one precondition of BootupState to reach `INITIALIZING_INTERNAL`
  // is to wait for the gRPC HttpConnection be ready. Only after the BootupState
  // meets the state, can AssistantManager start.
  assistant_client_->StartGrpcHttpConnectionClient(
      chromium_api_delegate_->GetHttpConnectionFactory());

  // The Libassistant BootupState goes to `RUNNING` right after
  // `SETTING_UP_ESSENTIAL_SERVICES` if AssistantManager::Start() is called
  // right after the AssistantManager is created. And Libassistant emits signals
  // of `ESSENTIAL_SERVICES_AVAILABLE` and `ALL_SERVICES_AVAILABLE` almost the
  // same time. However, unary gRPC does not guarantee order. ChromeOS could
  // receive these signals out of order.
  // We call AssistantManager::Start() here, ServicesBootingUp(), which is
  // triggered by `ESSENTIAL_SERVICES_AVAILABLE`. After the AssistantManager is
  // started, it will trigger `ALL_SERVICES_AVAILABLE`. Therefore these two
  // signals are generated in order.
  assistant_client_->assistant_manager()->Start();

  // Notify observer on Libassistant services started.
  SetStateAndInformObservers(ServiceState::kStarted);

  for (auto& observer : assistant_client_observers_)
    observer.OnAssistantClientStarted(assistant_client_.get());
}

void ServiceController::SetStateAndInformObservers(
    mojom::ServiceState new_state) {
  DCHECK_NE(state_, new_state);

  state_ = new_state;

  for (auto& observer : state_observers_)
    observer->OnStateChanged(state_);
}

void ServiceController::CreateAndRegisterChromiumApiDelegate(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        url_loader_factory_remote) {
  CreateChromiumApiDelegate(std::move(url_loader_factory_remote));
}

void ServiceController::CreateChromiumApiDelegate(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        url_loader_factory_remote) {
  DCHECK(!chromium_api_delegate_);

  chromium_api_delegate_ = std::make_unique<ChromiumApiDelegate>(
      CreatePendingURLLoaderFactory(std::move(url_loader_factory_remote)));
}

}  // namespace ash::libassistant
