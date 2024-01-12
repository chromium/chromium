// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_DISPLAY_CONNECTION_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_DISPLAY_CONNECTION_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/grpc_services_observer.h"
#include "chromeos/ash/services/libassistant/public/cpp/android_app_info.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/assistant/display_connection.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"

namespace ash::libassistant {

class AssistantClient;

class DisplayConnectionObserver {
 public:
  virtual ~DisplayConnectionObserver() = default;
  virtual void OnSpeechLevelUpdated(const float speech_level) {}
};

// Implements |asssistant_client::DisplayConnection| to initialize surface
// configuration and listen assistant event.
class DisplayConnection
    : public GrpcServicesObserver<
          ::assistant::api::OnAssistantDisplayEventRequest> {
 public:
  DisplayConnection(DisplayConnectionObserver* observer,
                    bool feedback_ui_enabled);
  DisplayConnection(const DisplayConnection&) = delete;
  DisplayConnection& operator=(const DisplayConnection&) = delete;
  ~DisplayConnection() override;

  // GrpcServicesObserver:
  // Invoked when an Assistant display event has been received.
  void OnGrpcMessage(
      const ::assistant::api::OnAssistantDisplayEventRequest& request) override;

  void SetAssistantClient(AssistantClient* assistant_client);
  void SetArcPlayStoreEnabled(bool enabled);
  void SetDeviceAppsEnabled(bool enabled);
  void SetAssistantContextEnabled(bool enabled);
  void OnAndroidAppListRefreshed(
      const std::vector<assistant::AndroidAppInfo>& apps_info);

  const std::vector<assistant::AndroidAppInfo>& GetCachedAndroidAppList() {
    return apps_info_;
  }

 private:
  void SendDisplayRequest();

  void FillDisplayRequest(::assistant::display::DisplayRequest& dr);

  raw_ptr<AssistantClient> assistant_client_ = nullptr;

  // Owned by the parent which also owns `this`.
  const raw_ptr<DisplayConnectionObserver> observer_;

  // Whether Assistant feedback UI is enabled.
  const bool feedback_ui_enabled_;

  // Whether ARC++ is enabled.
  bool arc_play_store_enabled_ = false;

  // Whether device apps user data consent is granted.
  bool device_apps_enabled_ = false;

  // Whether related info setting is on.
  bool related_info_enabled_ = false;

  // Supported Android apps information.
  std::vector<assistant::AndroidAppInfo> apps_info_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_DISPLAY_CONNECTION_H_
