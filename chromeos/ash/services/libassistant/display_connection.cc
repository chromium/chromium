// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/display_connection.h"

#include <sstream>

#include "base/check.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/display_interface.pb.h"

namespace ash::libassistant {

DisplayConnection::DisplayConnection(DisplayConnectionObserver* observer,
                                     bool feedback_ui_enabled)
    : observer_(observer),
      feedback_ui_enabled_(feedback_ui_enabled),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK(observer_);
}

DisplayConnection::~DisplayConnection() = default;

void DisplayConnection::OnGrpcMessage(
    const ::assistant::api::OnAssistantDisplayEventRequest& request) {
  auto& assistant_display_event = request.event();

  DCHECK(assistant_display_event.has_on_assistant_event());
  DCHECK(
      assistant_display_event.on_assistant_event().has_assistant_event_bytes());
  DVLOG(1) << "AssistantDisplayEventObserver received GrpcMessage.";

  const std::string& assistant_event_bytes =
      assistant_display_event.on_assistant_event().assistant_event_bytes();

  ::assistant::display::AssistantEvent event;
  if (!event.ParseFromString(assistant_event_bytes)) {
    LOG(ERROR) << "Unable to parse assistant event";
    return;
  }

  if (event.has_speech_level_event()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DisplayConnectionObserver::OnSpeechLevelUpdated,
                       base::Unretained(observer_),
                       event.speech_level_event().speech_level()));
  }
}

void DisplayConnection::SetAssistantClient(AssistantClient* assistant_client) {
  assistant_client_ = assistant_client;
}

void DisplayConnection::SetArcPlayStoreEnabled(bool enabled) {
  arc_play_store_enabled_ = enabled;
  SendDisplayRequest();
}

void DisplayConnection::SetDeviceAppsEnabled(bool enabled) {
  // For now we don't handle disabling the device apps bit, since we do not have
  // a way to get the bit changes. We only sync the bit at Service start post
  // login.
  // If user accept the whole activity control flow later we update the bit to
  // true.
  DCHECK(enabled || !device_apps_enabled_);
  if (device_apps_enabled_ == enabled)
    return;
  device_apps_enabled_ = enabled;
  SendDisplayRequest();
}

void DisplayConnection::SetAssistantContextEnabled(bool enabled) {
  if (related_info_enabled_ == enabled)
    return;
  related_info_enabled_ = enabled;
  SendDisplayRequest();
}

void DisplayConnection::OnAndroidAppListRefreshed(
    const std::vector<assistant::AndroidAppInfo>& apps_info) {
  apps_info_ = apps_info;
  SendDisplayRequest();
}

void DisplayConnection::SendDisplayRequest() {
  if (!assistant_client_) {
    LOG(ERROR) << "Can't send DisplayRequest before assistant client is set.";
    return;
  }

  ::assistant::display::DisplayRequest display_request;
  FillDisplayRequest(display_request);

  ::assistant::api::OnDisplayRequestRequest request;
  request.set_display_request_bytes(display_request.SerializeAsString());
  assistant_client_->SendDisplayRequest(request);
}

void DisplayConnection::FillDisplayRequest(
    ::assistant::display::DisplayRequest& dr) {
  auto* set_capabilities_request = dr.mutable_set_capabilities_request();
  auto* screen_capabilities =
      set_capabilities_request->mutable_screen_capabilities_to_merge();
  auto* resolution = screen_capabilities->mutable_resolution();
  // TODO(b/111841376): Set actual chromeos screen resolution and reset it when
  // resolution is changed.
  resolution->set_width_px(300);
  resolution->set_height_px(200);
  set_capabilities_request->set_server_generated_feedback_chips_enabled(
      feedback_ui_enabled_);
  set_capabilities_request->set_web_browser_supported(true);
  set_capabilities_request->mutable_supported_provider_types()
      ->add_supported_types(
          ::assistant::api::core_types::ProviderType::WEB_PROVIDER);

  if (arc_play_store_enabled_ && device_apps_enabled_) {
    set_capabilities_request->mutable_supported_provider_types()
        ->add_supported_types(
            ::assistant::api::core_types::ProviderType::ANDROID_APP);

    for (auto& app_info : apps_info_) {
      auto* android_app_info = set_capabilities_request->add_app_capabilities()
                                   ->mutable_provider()
                                   ->mutable_android_app_info();
      android_app_info->set_package_name(app_info.package_name);
      android_app_info->set_app_version(app_info.version);
      android_app_info->set_localized_app_name(app_info.localized_app_name);
      // android_intent are not set here since server will exclude app info
      // with intent field set.
    }
  }

  set_capabilities_request->mutable_supported_features()
      ->set_media_session_detection(
          related_info_enabled_
              ? ::assistant::api::RELIABLE_MEDIA_SESSION_DETECTION
              : ::assistant::api::
                    MEDIA_SESSION_DETECTION_DISABLED_SCREEN_CONTEXT);
}

}  // namespace ash::libassistant
