// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/display_controller.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/display_connection.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/public/mojom/speech_recognition_observer.mojom.h"
#include "chromeos/ash/services/libassistant/util.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/conversation.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/internal_options.pb.h"

namespace ash::libassistant {

namespace {
// A macro which ensures we are running on the main thread.
#define ENSURE_MOJOM_THREAD(method, ...)                                    \
  if (!mojom_task_runner_->RunsTasksInCurrentSequence()) {                  \
    mojom_task_runner_->PostTask(                                           \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }
}  // namespace

class DisplayController::EventObserver : public DisplayConnectionObserver {
 public:
  explicit EventObserver(DisplayController* parent) : parent_(parent) {}
  EventObserver(const EventObserver&) = delete;
  EventObserver& operator=(const EventObserver&) = delete;
  ~EventObserver() override = default;

  void OnSpeechLevelUpdated(const float speech_level) override {
    for (auto& observer : *parent_->speech_recognition_observers_) {
      observer->OnSpeechLevelUpdated(speech_level);
    }
  }

 private:
  const raw_ptr<DisplayController> parent_;
};

DisplayController::DisplayController(
    mojo::RemoteSet<mojom::SpeechRecognitionObserver>*
        speech_recognition_observers)
    : event_observer_(std::make_unique<EventObserver>(this)),
      display_connection_(
          std::make_unique<DisplayConnection>(event_observer_.get(),
                                              /*feedback_ui_enabled=*/true)),
      speech_recognition_observers_(*speech_recognition_observers),
      mojom_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK(speech_recognition_observers);
}

DisplayController::~DisplayController() = default;

void DisplayController::Bind(
    mojo::PendingReceiver<mojom::DisplayController> receiver) {
  receiver_.Bind(std::move(receiver));
}

void DisplayController::SetActionModule(
    chromeos::assistant::action::CrosActionModule* action_module) {
  DCHECK(action_module);
  action_module_ = action_module;
}

void DisplayController::SetArcPlayStoreEnabled(bool enabled) {
  display_connection_->SetArcPlayStoreEnabled(enabled);
}

void DisplayController::SetDeviceAppsEnabled(bool enabled) {
  display_connection_->SetDeviceAppsEnabled(enabled);

  DCHECK(action_module_);
  action_module_->SetAppSupportEnabled(
      assistant::features::IsAppSupportEnabled() && enabled);
}

void DisplayController::SetRelatedInfoEnabled(bool enabled) {
  display_connection_->SetAssistantContextEnabled(enabled);
}

void DisplayController::SetAndroidAppList(
    const std::vector<assistant::AndroidAppInfo>& apps) {
  display_connection_->OnAndroidAppListRefreshed(apps);
}

void DisplayController::OnAssistantClientCreated(
    AssistantClient* assistant_client) {
  assistant_client_ = assistant_client;
  display_connection_->SetAssistantClient(assistant_client_);

  // |display_connection_| outlives |assistant_client_|.
  assistant_client_->AddDisplayEventObserver(display_connection_.get());
}

void DisplayController::OnDestroyingAssistantClient(
    AssistantClient* assistant_client) {
  assistant_client_ = nullptr;
  display_connection_->SetAssistantClient(nullptr);
}

// Called from Libassistant thread.
void DisplayController::OnVerifyAndroidApp(
    const std::vector<assistant::AndroidAppInfo>& apps_info,
    const chromeos::assistant::InteractionInfo& interaction) {
  ENSURE_MOJOM_THREAD(&DisplayController::OnVerifyAndroidApp, apps_info,
                      interaction);

  std::vector<assistant::AndroidAppInfo> result_apps_info;
  for (auto& app_info : apps_info) {
    assistant::AndroidAppInfo result_app_info(app_info);
    auto app_status = GetAndroidAppStatus(app_info.package_name);
    result_app_info.status = app_status;
    result_apps_info.emplace_back(result_app_info);
  }

  auto interaction_proto =
      ash::libassistant::CreateVerifyProviderResponseInteraction(
          interaction.interaction_id, result_apps_info);

  ::assistant::api::VoicelessOptions options;
  options.set_obfuscated_gaia_id(interaction.user_id);
  // Set the request to be user initiated so that a new conversation will be
  // created to handle the client OPs in the response of this request.
  options.set_is_user_initiated(true);

  assistant_client_->SendVoicelessInteraction(
      interaction_proto, /*description=*/"verify_provider_response", options,
      base::DoNothing());
}

assistant::AppStatus DisplayController::GetAndroidAppStatus(
    const std::string& package_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& app_info : display_connection_->GetCachedAndroidAppList()) {
    if (app_info.package_name == package_name) {
      DVLOG(1) << "Assistant: App is available on the device.";
      return assistant::AppStatus::kAvailable;
    }
  }

  DVLOG(1) << "Assistant: App is unavailable on the device";
  return assistant::AppStatus::kUnavailable;
}

}  // namespace ash::libassistant
