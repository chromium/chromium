// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_H_
#define COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_H_

namespace push_notification {

// Base class for the PushNotificationService. This class along with other
// classes in this directory are inherited from to create platform specific push
// notification services.
class PushNotificationService {
 public:
  PushNotificationService();
  virtual ~PushNotificationService();
};

}  // namespace push_notification

#endif  // COMPONENTS_PUSH_NOTIFICATION_PUSH_NOTIFICATION_SERVICE_H_
