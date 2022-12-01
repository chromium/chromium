// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_user_action_recorder.h"

namespace ash {
namespace phonehub {

FakeUserActionRecorder::FakeUserActionRecorder() = default;

FakeUserActionRecorder::~FakeUserActionRecorder() = default;

void FakeUserActionRecorder::RecordUiOpened() {
  ++num_ui_opened_events_;
}

void FakeUserActionRecorder::RecordTetherConnectionAttempt() {
  ++num_tether_attempts_;
}

void FakeUserActionRecorder::RecordDndAttempt() {
  ++num_dnd_attempts_;
}

void FakeUserActionRecorder::RecordFindMyDeviceAttempt() {
  ++num_find_my_device_attempts_;
}

void FakeUserActionRecorder::RecordBrowserTabOpened() {
  ++num_browser_tabs_opened_;
}

void FakeUserActionRecorder::RecordNotificationDismissAttempt() {
  ++num_notification_dismissals_;
}

void FakeUserActionRecorder::RecordNotificationReplyAttempt() {
  ++num_notification_replies_;
}

void FakeUserActionRecorder::RecordCameraRollDownloadAttempt() {
  ++num_camera_roll_downloads_;
}

void FakeUserActionRecorder::RecordAppStreamLauncherOpened() {
  ++app_stream_launcher_opened_;
}

}  // namespace phonehub
}  // namespace ash
