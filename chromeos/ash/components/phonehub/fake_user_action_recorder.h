// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_USER_ACTION_RECORDER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_USER_ACTION_RECORDER_H_

#include <stddef.h>

#include "chromeos/ash/components/phonehub/user_action_recorder.h"

namespace ash {
namespace phonehub {

class FakeUserActionRecorder : public UserActionRecorder {
 public:
  FakeUserActionRecorder();
  ~FakeUserActionRecorder() override;

  size_t num_ui_opened_events() const { return num_ui_opened_events_; }
  size_t num_tether_attempts() const { return num_tether_attempts_; }
  size_t num_dnd_attempts() const { return num_dnd_attempts_; }
  size_t num_find_my_device_attempts() const {
    return num_find_my_device_attempts_;
  }
  size_t num_browser_tabs_opened() const { return num_browser_tabs_opened_; }
  size_t num_notification_dismissals() const {
    return num_notification_dismissals_;
  }
  size_t num_notification_replies() const { return num_notification_replies_; }
  size_t num_camera_roll_downloads() const {
    return num_camera_roll_downloads_;
  }

  size_t app_stream_launcher_opened() const {
    return app_stream_launcher_opened_;
  }

 private:
  // UserActionRecorder:
  void RecordUiOpened() override;
  void RecordTetherConnectionAttempt() override;
  void RecordDndAttempt() override;
  void RecordFindMyDeviceAttempt() override;
  void RecordBrowserTabOpened() override;
  void RecordNotificationDismissAttempt() override;
  void RecordNotificationReplyAttempt() override;
  void RecordCameraRollDownloadAttempt() override;
  void RecordAppStreamLauncherOpened() override;

  size_t num_ui_opened_events_ = 0u;
  size_t num_tether_attempts_ = 0u;
  size_t num_dnd_attempts_ = 0u;
  size_t num_find_my_device_attempts_ = 0u;
  size_t num_browser_tabs_opened_ = 0u;
  size_t num_notification_dismissals_ = 0u;
  size_t num_notification_replies_ = 0u;
  size_t num_camera_roll_downloads_ = 0u;
  size_t app_stream_launcher_opened_ = 0u;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_USER_ACTION_RECORDER_H_
