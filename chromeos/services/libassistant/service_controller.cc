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
#include "chromeos/services/assistant/public/proto/assistant_device_settings_ui.pb.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "chromeos/services/libassistant/chromium_api_delegate.h"
#include "chromeos/services/libassistant/util.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/device_state_listener.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace chromeos {
namespace libassistant {

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

constexpr base::Feature kChromeOSAssistantDogfood{
    "ChromeOSAssistantDogfood", base::FEATURE_DISABLED_BY_DEFAULT};

constexpr char kServersideDogfoodExperimentId[] = "20347368";
constexpr char kServersideOpenAppExperimentId[] = "39651593";
constexpr char kServersideResponseProcessingV2ExperimentId[] = "1793869";

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

void SetServerExperiments(
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  std::vector<std::string> server_experiment_ids;
  FillServerExperimentIds(&server_experiment_ids);

  if (server_experiment_ids.size() > 0) {
    assistant_manager_internal->AddExtraExperimentIds(server_experiment_ids);
  }
}

const char* LocaleOrDefault(const std::string& locale) {
  if (locale.empty()) {
    // When |locale| is not provided we fall back to approximate locale.
    return icu::Locale::getDefault().getName();
  } else {
    return locale.c_str();
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//  DeviceSettingsUpdater
////////////////////////////////////////////////////////////////////////////////

// Will update the device settings as soon as Libassistant is started.
// The device settings can not be updated earlier, as they require the
// device-id that is only assigned by Libassistant when it starts.
class ServiceController::DeviceSettingsUpdater
    : public AssistantManagerObserver {
 public:
  DeviceSettingsUpdater(ServiceController* parent,
                        const std::string& locale,
                        bool hotword_enabled)
      : parent_(parent), locale_(locale), hotword_enabled_(hotword_enabled) {
    parent_->AddAndFireAssistantManagerObserver(this);
  }
  DeviceSettingsUpdater(const DeviceSettingsUpdater&) = delete;
  DeviceSettingsUpdater& operator=(const DeviceSettingsUpdater&) = delete;
  ~DeviceSettingsUpdater() override {
    parent_->RemoveAssistantManagerObserver(this);
  }

 private:
  // AssistantManagerObserver implementation:
  void OnAssistantManagerStarted(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      override {
    const std::string device_id = assistant_manager->GetDeviceId();
    if (device_id.empty())
      return;

    // Update device id and device type.
    assistant::SettingsUiUpdate update;
    assistant::AssistantDeviceSettingsUpdate* device_settings_update =
        update.mutable_assistant_device_settings_update()
            ->add_assistant_device_settings_update();
    device_settings_update->set_device_id(device_id);
    device_settings_update->set_assistant_device_type(
        assistant::AssistantDevice::CROS);

    if (hotword_enabled_) {
      device_settings_update->mutable_device_settings()->set_speaker_id_enabled(
          true);
    }

    VLOG(1) << "Assistant: Update device locale: " << locale_;
    device_settings_update->mutable_device_settings()->set_locale(locale_);

    // Enable personal readout to grant permission for personal features.
    device_settings_update->mutable_device_settings()->set_personal_readout(
        assistant::AssistantDeviceSettings::PERSONAL_READOUT_ENABLED);

    // Device settings update result is not handled because it is not included
    // in the SettingsUiUpdateResult.
    parent_->UpdateSettings(update.SerializeAsString(), base::DoNothing());
  }

  ServiceController* const parent_;
  const std::string locale_;
  const bool hotword_enabled_;
};

////////////////////////////////////////////////////////////////////////////////
//  DeviceStateListener
////////////////////////////////////////////////////////////////////////////////

class ServiceController::DeviceStateListener
    : public assistant_client::DeviceStateListener {
 public:
  explicit DeviceStateListener(ServiceController* parent)
      : parent_(parent),
        mojom_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  DeviceStateListener(const DeviceStateListener&) = delete;
  DeviceStateListener& operator=(const DeviceStateListener&) = delete;
  ~DeviceStateListener() override = default;

  // assistant_client::DeviceStateListener overrides:
  // Called on Libassistant thread.
  void OnStartFinished() override {
    ENSURE_MOJOM_THREAD(&DeviceStateListener::OnStartFinished);
    parent_->OnStartFinished();
  }

 private:
  ServiceController* const parent_;
  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;
  base::WeakPtrFactory<DeviceStateListener> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
//  ServiceController
////////////////////////////////////////////////////////////////////////////////

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

void ServiceController::Initialize(
    mojom::BootupConfigPtr config,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory) {
  if (assistant_manager_ != nullptr) {
    LOG(ERROR) << "Initialize() should only be called once.";
    return;
  }

  assistant_manager_ = delegate_->CreateAssistantManager(
      platform_api_, ToLibassistantConfig(*config));
  assistant_manager_internal_ =
      delegate_->UnwrapAssistantManagerInternal(assistant_manager_.get());

  SetLocale(config->locale);
  SetHotwordEnabled(config->hotword_enabled);
  SetSpokenFeedbackEnabled(config->spoken_feedback_enabled);

  CreateAndRegisterDeviceStateListener();
  CreateAndRegisterChromiumApiDelegate(std::move(url_loader_factory));

  SetServerExperiments(assistant_manager_internal());

  libassistant_v1_api_ = std::make_unique<assistant::LibassistantV1Api>(
      assistant_manager_.get(), assistant_manager_internal_);

  for (auto& observer : assistant_manager_observers_) {
    observer.OnAssistantManagerCreated(assistant_manager(),
                                       assistant_manager_internal());
  }
}

void ServiceController::Start() {
  if (state_ != ServiceState::kStopped)
    return;

  DCHECK(IsInitialized()) << "Initialize() must be called before Start()";
  DVLOG(1) << "Starting Libassistant service";

  if (initialize_callback_) {
    std::move(initialize_callback_)
        .Run(assistant_manager(), assistant_manager_internal());
  }

  assistant_manager()->Start();

  SetStateAndInformObservers(ServiceState::kStarted);

  for (auto& observer : assistant_manager_observers_) {
    observer.OnAssistantManagerStarted(assistant_manager(),
                                       assistant_manager_internal());
  }

  DVLOG(1) << "Started Libassistant service";
}

void ServiceController::Stop() {
  if (state_ == ServiceState::kStopped)
    return;

  DVLOG(1) << "Stopping Libassistant service";
  SetStateAndInformObservers(ServiceState::kStopped);

  for (auto& observer : assistant_manager_observers_) {
    observer.OnDestroyingAssistantManager(assistant_manager(),
                                          assistant_manager_internal());
  }

  libassistant_v1_api_ = nullptr;
  assistant_manager_ = nullptr;
  assistant_manager_internal_ = nullptr;
  chromium_api_delegate_ = nullptr;
  device_state_listener_ = nullptr;
  locale_.reset();
  hotword_enabled_.reset();
  spoken_feedback_enabled_.reset();

  for (auto& observer : assistant_manager_observers_)
    observer.OnAssistantManagerDestroyed();

  DVLOG(1) << "Stopped Libassistant service";
}

void ServiceController::ResetAllDataAndStop() {
  if (assistant_manager()) {
    DVLOG(1) << "Resetting all Libassistant data";
    assistant_manager()->ResetAllDataAndShutdown();
  }
  Stop();
}

void ServiceController::AddAndFireStateObserver(
    mojo::PendingRemote<mojom::StateObserver> pending_observer) {
  mojo::Remote<mojom::StateObserver> observer(std::move(pending_observer));

  observer->OnStateChanged(state_);

  state_observers_.Add(std::move(observer));
}

void ServiceController::SetLocale(const std::string& value) {
  DCHECK(IsInitialized());

  locale_ = LocaleOrDefault(value);
  SetInternalOptions(locale_, spoken_feedback_enabled_);
  SetDeviceSettings(locale_, hotword_enabled_);
}

void ServiceController::SetSpokenFeedbackEnabled(bool value) {
  DCHECK(IsInitialized());

  spoken_feedback_enabled_ = value;
  SetInternalOptions(locale_, spoken_feedback_enabled_);
}

void ServiceController::SetHotwordEnabled(bool value) {
  DCHECK(IsInitialized());

  hotword_enabled_ = value;
  SetDeviceSettings(locale_, hotword_enabled_);
}

void ServiceController::SetInternalOptions(
    const base::Optional<std::string>& locale,
    base::Optional<bool> spoken_feedback_enabled) {
  if (!locale.has_value() || !spoken_feedback_enabled.has_value())
    return;

  assistant_manager_internal()->SetLocaleOverride(locale.value());

  auto* internal_options =
      assistant_manager_internal()->CreateDefaultInternalOptions();
  assistant::SetAssistantOptions(internal_options, locale.value(),
                                 spoken_feedback_enabled.value());

  internal_options->SetClientControlEnabled(
      assistant::features::IsRoutinesEnabled());

  if (!assistant::features::IsVoiceMatchDisabled())
    internal_options->EnableRequireVoiceMatchVerification();

  assistant_manager_internal()->SetOptions(*internal_options, [](bool success) {
    DVLOG(2) << "set options: " << success;
  });
}

void ServiceController::SetDeviceSettings(
    const base::Optional<std::string>& locale,
    base::Optional<bool> hotword_enabled) {
  if (!locale.has_value() || !hotword_enabled.has_value())
    return;

  device_settings_updater_ = std::make_unique<DeviceSettingsUpdater>(
      this, locale.value(), hotword_enabled.value());
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
  // Note we do send the |OnAssistantManagerStarted| event even if the service
  // is currently running, to ensure that an observer that only observes
  // |OnAssistantManagerStarted| will not miss a currently running instance
  // when it is being added.
  if (IsStarted()) {
    observer->OnAssistantManagerStarted(assistant_manager(),
                                        assistant_manager_internal());
  }
  if (IsRunning()) {
    observer->OnAssistantManagerRunning(assistant_manager(),
                                        assistant_manager_internal());
  }
}

void ServiceController::RemoveAssistantManagerObserver(
    AssistantManagerObserver* observer) {
  assistant_manager_observers_.RemoveObserver(observer);
}

bool ServiceController::IsStarted() const {
  switch (state_) {
    case ServiceState::kStopped:
      return false;
    case ServiceState::kStarted:
    case ServiceState::kRunning:
      return true;
  }
}

bool ServiceController::IsInitialized() const {
  return assistant_manager_ != nullptr;
}

bool ServiceController::IsRunning() const {
  switch (state_) {
    case ServiceState::kStopped:
    case ServiceState::kStarted:
      return false;
    case ServiceState::kRunning:
      return true;
  }
}

assistant_client::AssistantManager* ServiceController::assistant_manager() {
  return assistant_manager_.get();
}

assistant_client::AssistantManagerInternal*
ServiceController::assistant_manager_internal() {
  return assistant_manager_internal_;
}

void ServiceController::OnStartFinished() {
  DVLOG(1) << "Libassistant start is finished";
  SetStateAndInformObservers(mojom::ServiceState::kRunning);

  for (auto& observer : assistant_manager_observers_) {
    observer.OnAssistantManagerRunning(assistant_manager(),
                                       assistant_manager_internal());
  }
}

void ServiceController::UpdateSettings(const std::string& settings,
                                       UpdateSettingsCallback callback) {
  if (!IsStarted()) {
    std::move(callback).Run(std::string());
    return;
  }

  // Wraps the callback into a repeating callback since the server side
  // interface requires the callback to be copyable.
  std::string serialized_proto =
      assistant::SerializeUpdateSettingsUiRequest(settings);
  assistant_manager_internal()->SendUpdateSettingsUiRequest(
      serialized_proto, /*user_id=*/std::string(),
      [repeating_callback =
           base::AdaptCallbackForRepeating(std::move(callback)),
       task_runner = base::ThreadTaskRunnerHandle::Get()](
          const assistant_client::VoicelessResponse& response) {
        std::string update =
            assistant::UnwrapUpdateSettingsUiResponse(response);
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](base::RepeatingCallback<void(const std::string&)> callback,
                   const std::string& result) { callback.Run(result); },
                repeating_callback, update));
      });
}

void ServiceController::SetStateAndInformObservers(
    mojom::ServiceState new_state) {
  DCHECK_NE(state_, new_state);

  state_ = new_state;

  for (auto& observer : state_observers_)
    observer->OnStateChanged(state_);
}

void ServiceController::CreateAndRegisterDeviceStateListener() {
  device_state_listener_ = std::make_unique<DeviceStateListener>(this);
  assistant_manager()->AddDeviceStateListener(device_state_listener_.get());
}

void ServiceController::CreateAndRegisterChromiumApiDelegate(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        url_loader_factory_remote) {
  CreateChromiumApiDelegate(std::move(url_loader_factory_remote));

  assistant_manager_internal()
      ->GetFuchsiaApiHelperOrDie()
      ->SetFuchsiaApiDelegate(chromium_api_delegate_.get());
}

void ServiceController::CreateChromiumApiDelegate(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        url_loader_factory_remote) {
  DCHECK(!chromium_api_delegate_);

  chromium_api_delegate_ = std::make_unique<ChromiumApiDelegate>(
      CreatePendingURLLoaderFactory(std::move(url_loader_factory_remote)));
}

}  // namespace libassistant
}  // namespace chromeos
