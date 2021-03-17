// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_DISPLAY_CONNECTION_IMPL_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_DISPLAY_CONNECTION_IMPL_H_

#include <string>

#include "base/synchronization/lock.h"
#include "chromeos/services/libassistant/public/cpp/android_app_info.h"
#include "libassistant/display/proto/display_connection.pb.h"
#include "libassistant/shared/internal_api/display_connection.h"

namespace chromeos {
namespace libassistant {

class DisplayConnectionObserver {
 public:
  virtual ~DisplayConnectionObserver() = default;
  virtual void OnSpeechLevelUpdated(const float speech_level) {}
};

// Implements |asssistant_client::DisplayConnection| to initialize surface
// configuration and listen assistant event.
class DisplayConnectionImpl : public assistant_client::DisplayConnection {
 public:
  DisplayConnectionImpl(DisplayConnectionObserver* observer,
                        bool feedback_ui_enabled,
                        bool media_session_enabled);
  DisplayConnectionImpl(const DisplayConnectionImpl&) = delete;
  DisplayConnectionImpl& operator=(const DisplayConnectionImpl&) = delete;
  ~DisplayConnectionImpl() override;

  // assistant_client::DisplayConnection overrides:
  void SetDelegate(Delegate* delegate) override;
  void OnAssistantEvent(const std::string& assistant_event_bytes) override;

  void SetArcPlayStoreEnabled(bool enabled);
  void SetDeviceAppsEnabled(bool enabled);
  void SetAssistantContextEnabled(bool enabled);
  void OnAndroidAppListRefreshed(
      const std::vector<assistant::AndroidAppInfo>& apps_info);

  const std::vector<assistant::AndroidAppInfo>& GetCachedAndroidAppList() {
    return apps_info_;
  }

 private:
  void SendDisplayRequestLocked();

  void FillDisplayRequestLocked(::assistant::display::DisplayRequest& dr);

  Delegate* delegate_ GUARDED_BY(update_display_request_mutex_) = nullptr;

  DisplayConnectionObserver* const observer_;

  // Whether Assistant feedback UI is enabled.
  const bool feedback_ui_enabled_;

  // Whether Media Session support is enabled.
  const bool media_session_enabled_;

  // Whether ARC++ is enabled.
  bool arc_play_store_enabled_ GUARDED_BY(update_display_request_mutex_) =
      false;

  // Whether device apps user data consent is granted.
  bool device_apps_enabled_ GUARDED_BY(update_display_request_mutex_) = false;

  // Whether related info setting is on.
  bool related_info_enabled_ GUARDED_BY(update_display_request_mutex_) = false;

  // Supported Android apps information.
  std::vector<assistant::AndroidAppInfo> GUARDED_BY(
      update_display_request_mutex_) apps_info_;

  // Both LibAssistant and Chrome threads may update and send display request so
  // we always guard access with |update_display_request_mutex_|.
  base::Lock update_display_request_mutex_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_DISPLAY_CONNECTION_IMPL_H_
