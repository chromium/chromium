// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/display_controller.h"

#include <memory>

#include "chromeos/assistant/internal/cros_display_connection.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/libassistant/public/mojom/speech_recognition_observer.mojom.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"

namespace chromeos {
namespace libassistant {

namespace {

assistant::AndroidAppInfo ToAndroidAppInfo(
    const mojom::AndroidAppInfoPtr& app) {
  assistant::AndroidAppInfo result;
  result.package_name = app->package_name;
  result.version = app->version;
  result.localized_app_name = app->localized_app_name;
  return result;
}

std::vector<assistant::AndroidAppInfo> ToAndroidAppInfoList(
    const std::vector<mojom::AndroidAppInfoPtr>& apps) {
  std::vector<assistant::AndroidAppInfo> result;

  for (const auto& app : apps)
    result.push_back(ToAndroidAppInfo(app));

  return result;
}

}  // namespace

class DisplayController::EventObserver
    : public assistant::AssistantEventObserver {
 public:
  explicit EventObserver(DisplayController* parent) : parent_(parent) {}
  EventObserver(const EventObserver&) = delete;
  EventObserver& operator=(const EventObserver&) = delete;
  ~EventObserver() override = default;

  void OnSpeechLevelUpdated(const float speech_level) override {
    for (auto& observer : parent_->speech_recognition_observers_)
      observer->OnSpeechLevelUpdated(speech_level);
  }

 private:
  DisplayController* const parent_;
};

DisplayController::DisplayController(
    mojo::RemoteSet<mojom::SpeechRecognitionObserver>*
        speech_recognition_observers)
    : event_observer_(std::make_unique<EventObserver>(this)),
      display_connection_(std::make_unique<assistant::CrosDisplayConnection>(
          event_observer_.get(),
          /*feedback_ui_enabled=*/true,
          assistant::features::IsMediaSessionIntegrationEnabled())),
      speech_recognition_observers_(*speech_recognition_observers) {
  DCHECK(speech_recognition_observers);
}

DisplayController::~DisplayController() = default;

void DisplayController::Bind(
    mojo::PendingReceiver<mojom::DisplayController> receiver) {
  receiver_.Bind(std::move(receiver));
}

void DisplayController::SetArcPlayStoreEnabled(bool enabled) {
  display_connection_->SetArcPlayStoreEnabled(enabled);
}

void DisplayController::SetDeviceAppsEnabled(bool enabled) {
  display_connection_->SetDeviceAppsEnabled(enabled);
}

void DisplayController::SetRelatedInfoEnabled(bool enabled) {
  display_connection_->SetAssistantContextEnabled(enabled);
}

void DisplayController::SetAndroidAppList(
    std::vector<mojom::AndroidAppInfoPtr> apps) {
  display_connection_->OnAndroidAppListRefreshed(ToAndroidAppInfoList(apps));
}

void DisplayController::OnAssistantManagerCreated(
    assistant_client::AssistantManager* assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  assistant_manager_internal->SetDisplayConnection(display_connection_.get());
}

}  // namespace libassistant
}  // namespace chromeos
