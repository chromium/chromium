// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/display_connection_impl.h"

#include <sstream>

#include "base/check.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace chromeos {
namespace libassistant {

DisplayConnectionImpl::DisplayConnectionImpl(
    DisplayConnectionObserver* observer,
    bool feedback_ui_enabled)
    : observer_(observer),
      feedback_ui_enabled_(feedback_ui_enabled),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DCHECK(observer_);
}

DisplayConnectionImpl::~DisplayConnectionImpl() = default;

void DisplayConnectionImpl::SetDelegate(Delegate* delegate) {
  base::AutoLock lock(update_display_request_mutex_);
  delegate_ = delegate;
  SendDisplayRequestLocked();
}

void DisplayConnectionImpl::OnAssistantEvent(
    const std::string& assistant_event_bytes) {
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

void DisplayConnectionImpl::SetArcPlayStoreEnabled(bool enabled) {
  base::AutoLock lock(update_display_request_mutex_);
  arc_play_store_enabled_ = enabled;
  SendDisplayRequestLocked();
}

void DisplayConnectionImpl::SetDeviceAppsEnabled(bool enabled) {
  base::AutoLock lock(update_display_request_mutex_);
  // For now we don't handle disabling the device apps bit, since we do not have
  // a way to get the bit changes. We only sync the bit at Service start post
  // login.
  // If user accept the whole activity control flow later we update the bit to
  // true.
  DCHECK(enabled || !device_apps_enabled_);
  if (device_apps_enabled_ == enabled)
    return;
  device_apps_enabled_ = enabled;
  SendDisplayRequestLocked();
}

void DisplayConnectionImpl::SetAssistantContextEnabled(bool enabled) {
  base::AutoLock lock(update_display_request_mutex_);

  if (related_info_enabled_ == enabled)
    return;
  related_info_enabled_ = enabled;
  SendDisplayRequestLocked();
}

void DisplayConnectionImpl::OnAndroidAppListRefreshed(
    const std::vector<assistant::AndroidAppInfo>& apps_info) {
  base::AutoLock lock(update_display_request_mutex_);
  apps_info_ = apps_info;
  SendDisplayRequestLocked();
}

void DisplayConnectionImpl::SendDisplayRequestLocked() {
  ::assistant::display::DisplayRequest display_request;
  update_display_request_mutex_.AssertAcquired();
  FillDisplayRequestLocked(display_request);

  if (!delegate_) {
    LOG(ERROR) << "Can't send DisplayRequest before delegate is set.";
    return;
  }
  std::string s;
  display_request.SerializeToString(&s);
  delegate_->OnDisplayRequest(s);
}

void DisplayConnectionImpl::FillDisplayRequestLocked(
    ::assistant::display::DisplayRequest& dr) {
  update_display_request_mutex_.AssertAcquired();

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

}  // namespace libassistant
}  // namespace chromeos
