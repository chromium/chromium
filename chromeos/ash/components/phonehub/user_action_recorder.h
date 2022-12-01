// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_USER_ACTION_RECORDER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_USER_ACTION_RECORDER_H_

namespace ash {
namespace phonehub {

// Records actions that a user may take via Phone Hub.
class UserActionRecorder {
 public:
  virtual ~UserActionRecorder() = default;

  // Records that the Phone Hub UI has been opened.
  virtual void RecordUiOpened() = 0;

  // Records that an Instant Tethering connection has been attempted via the
  // Phone Hub UI.
  virtual void RecordTetherConnectionAttempt() = 0;

  // Records that an attempt to change the Do Not Disturb status has been
  // attempted via the Phone Hub UI.
  virtual void RecordDndAttempt() = 0;

  // Records that an attempt to start or stop ringing the user's phone via the
  // Find My Device feature has been attempted via the Phone Hub UI.
  virtual void RecordFindMyDeviceAttempt() = 0;

  // Records that the user has opened a browser tab synced via the "task
  // continuation" feature.
  virtual void RecordBrowserTabOpened() = 0;

  // Records that an attempt to dismiss a notification generated via Phone Hub
  // has been attempted.
  virtual void RecordNotificationDismissAttempt() = 0;

  // Records that an attempt to reply to a notification generated via Phone Hub
  // has been attempted.
  virtual void RecordNotificationReplyAttempt() = 0;

  // Records that an attempt to download a camera roll item via Phone Hub has
  // been attempted.
  virtual void RecordCameraRollDownloadAttempt() = 0;

  // Records opening of app stream launher.
  virtual void RecordAppStreamLauncherOpened() = 0;

 protected:
  UserActionRecorder() = default;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_USER_ACTION_RECORDER_H_
