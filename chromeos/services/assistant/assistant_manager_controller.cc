// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_controller.h"

#include "base/feature_list.h"
#include "chromeos/assistant/internal/cros_display_connection.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/assistant/assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/assistant_manager_service_impl.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"

namespace chromeos {
namespace assistant {

namespace {

using DoneCallback =
    base::OnceCallback<void(std::unique_ptr<CrosDisplayConnection>,
                            std::unique_ptr<assistant_client::AssistantManager>,
                            assistant_client::AssistantManagerInternal*)>;

constexpr base::Feature kChromeOSAssistantDogfood{
    "ChromeOSAssistantDogfood", base::FEATURE_DISABLED_BY_DEFAULT};

constexpr char kServersideDogfoodExperimentId[] = "20347368";
constexpr char kServersideOpenAppExperimentId[] = "39651593";
constexpr char kServersideResponseProcessingV2ExperimentId[] = "1793869";

struct AssistantObjects {
  std::unique_ptr<CrosDisplayConnection> display_connection;
  std::unique_ptr<assistant_client::AssistantManager> assistant_manager;
  assistant_client::AssistantManagerInternal* assistant_manager_internal =
      nullptr;
};

void FillServerExperimentIds(std::vector<std::string>* server_experiment_ids) {
  if (base::FeatureList::IsEnabled(kChromeOSAssistantDogfood)) {
    server_experiment_ids->emplace_back(kServersideDogfoodExperimentId);
  }

  if (base::FeatureList::IsEnabled(features::kAssistantAppSupport))
    server_experiment_ids->emplace_back(kServersideOpenAppExperimentId);

  if (features::IsResponseProcessingV2Enabled()) {
    server_experiment_ids->emplace_back(
        kServersideResponseProcessingV2ExperimentId);
  }
}

void SetServerExperiments(
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  std::vector<std::string> server_experiment_ids;
  FillServerExperimentIds(&server_experiment_ids);

  if (server_experiment_ids.size() > 0) {
    assistant_manager_internal->AddExtraExperimentIds(server_experiment_ids);
  }
}

void SetAuthTokens(assistant_client::AssistantManager* assistant_manager,
                   const AssistantManagerController::AuthTokens& tokens) {
  assistant_manager->SetAuthTokens(tokens);
}

void UpdateInternalOptions(
    assistant_client::AssistantManagerInternal* assistant_manager_internal,
    const std::string& locale,
    bool spoken_feedback_enabled) {
  // NOTE: this method is called on multiple threads, it needs to be
  // thread-safe.
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

// Creates the Assistant on the current thread, and stores the resulting
// objects in |result|.
void CreateAssistantOnCurrentThread(
    AssistantObjects* result,
    AssistantManagerServiceDelegate* delegate,
    assistant_client::PlatformApi* platform_api,
    assistant_client::ActionModule* action_module,
    assistant_client::FuchsiaApiDelegate* fuchsia_api_delegate,
    AssistantManagerServiceImpl* service,
    const std::string& libassistant_config,
    const std::string& locale,
    const std::string& locale_override,
    bool spoken_feedback_enabled,
    const AssistantManagerController::AuthTokens& auth_tokens) {
  result->display_connection = std::make_unique<CrosDisplayConnection>(
      service, /*feedback_ui_enabled=*/true,
      assistant::features::IsMediaSessionIntegrationEnabled());

  result->assistant_manager =
      delegate->CreateAssistantManager(platform_api, libassistant_config);

  result->assistant_manager_internal =
      delegate->UnwrapAssistantManagerInternal(result->assistant_manager.get());

  UpdateInternalOptions(result->assistant_manager_internal, locale,
                        spoken_feedback_enabled);

  result->assistant_manager_internal->SetDisplayConnection(
      result->display_connection.get());
  result->assistant_manager_internal->SetLocaleOverride(locale_override);
  result->assistant_manager_internal->RegisterActionModule(action_module);
  result->assistant_manager_internal->SetAssistantManagerDelegate(service);
  result->assistant_manager_internal->GetFuchsiaApiHelperOrDie()
      ->SetFuchsiaApiDelegate(fuchsia_api_delegate);

  result->assistant_manager->AddConversationStateListener(service);
  result->assistant_manager->AddDeviceStateListener(service);

  SetServerExperiments(result->assistant_manager_internal);
  SetAuthTokens(result->assistant_manager.get(), auth_tokens);

  result->assistant_manager->Start();
}

// Creates the Assistant on the given (background) thread, and passes it to
// the callback on the current thread.
void CreateAssistantOnBackgroundThread(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    AssistantManagerServiceDelegate* delegate,
    assistant_client::PlatformApi* platform_api,
    assistant_client::ActionModule* action_module,
    assistant_client::FuchsiaApiDelegate* fuchsia_api_delegate,
    AssistantManagerServiceImpl* service,
    const std::string& libassistant_config,
    const std::string& locale,
    const std::string& locale_override,
    bool spoken_feedback_enabled,
    const AssistantManagerController::AuthTokens& auth_tokens,
    DoneCallback done_callback) {
  auto result = std::make_unique<AssistantObjects>();
  auto* result_pointer = result.get();
  task_runner->PostTaskAndReply(
      FROM_HERE,
      BindOnce(CreateAssistantOnCurrentThread, result_pointer, delegate,
               platform_api, action_module, fuchsia_api_delegate, service,
               libassistant_config, locale, locale_override,
               spoken_feedback_enabled, auth_tokens),
      BindOnce(
          [](std::unique_ptr<AssistantObjects> result, DoneCallback callback) {
            std::move(callback).Run(std::move(result->display_connection),
                                    std::move(result->assistant_manager),
                                    result->assistant_manager_internal);
          },
          std::move(result), std::move(done_callback)));
}

}  // namespace

AssistantManagerController::AssistantManagerController()
    : weak_factory_(this) {}

AssistantManagerController::~AssistantManagerController() = default;

void AssistantManagerController::Initialize(
    AssistantManagerServiceImpl* service,
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner,
    AssistantManagerServiceDelegate* delegate,
    assistant_client::PlatformApi* platform_api,
    assistant_client::ActionModule* action_module,
    assistant_client::FuchsiaApiDelegate* fuchsia_api_delegate,
    const std::string& libassistant_config,
    const std::string& locale,
    const std::string& locale_override,
    bool spoken_feedback_enabled,
    const AuthTokens& auth_tokens,
    base::OnceClosure done_callback) {
  CreateAssistantOnBackgroundThread(
      background_task_runner, delegate, platform_api, action_module,
      fuchsia_api_delegate, service, libassistant_config, locale,
      locale_override, spoken_feedback_enabled, auth_tokens,
      base::BindOnce(&AssistantManagerController::OnAssistantCreated,
                     weak_factory_.GetWeakPtr(), std::move(done_callback)));
}

void AssistantManagerController::UpdateInternalOptions(
    const std::string& locale,
    bool spoken_feedback_enabled) {
  chromeos::assistant::UpdateInternalOptions(assistant_manager_internal(),
                                             locale, spoken_feedback_enabled);
}

void AssistantManagerController::SetAuthTokens(const AuthTokens& tokens) {
  chromeos::assistant::SetAuthTokens(assistant_manager(), tokens);
}

bool AssistantManagerController::IsInitialized() const {
  return (display_connection_ != nullptr) && (assistant_manager_ != nullptr) &&
         (assistant_manager_internal_ != nullptr);
}

void AssistantManagerController::OnAssistantCreated(
    base::OnceClosure done_callback,
    std::unique_ptr<CrosDisplayConnection> display_connection,
    std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  DCHECK(display_connection);
  DCHECK(assistant_manager);
  DCHECK(assistant_manager_internal);

  display_connection_ = std::move(display_connection);
  assistant_manager_ = std::move(assistant_manager);
  assistant_manager_internal_ = assistant_manager_internal;

  std::move(done_callback).Run();
}

}  // namespace assistant
}  // namespace chromeos
