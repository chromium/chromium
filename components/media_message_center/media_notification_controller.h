// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTROLLER_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "base/component_export.h"

template <typename T>
class scoped_refptr;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace media_message_center {

// MediaNotificationController does the actual hiding and showing of the media
// notification.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationController {
 public:
  // Shows/hides a notification with the given request id. Called by
  // MediaNotificationItem when the notification should be shown/hidden.
  virtual void ShowNotification(const std::string& id) = 0;
  virtual void HideNotification(const std::string& id) = 0;

  // Removes a notification item with the given request id. Called by
  // MediaNotificationItem when it should be destroyed.
  virtual void RemoveItem(const std::string& id) = 0;

  // Returns a task runner that the MediaNotificationItem should use.
  // It typically returns null except in tests.
  virtual scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const = 0;

  // Notifies the MediaNotificationController that a media button was pressed on
  // the MediaNotificationView.
  virtual void LogMediaSessionActionButtonPressed(const std::string& id) = 0;

 protected:
  virtual ~MediaNotificationController() = default;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTROLLER_H_
