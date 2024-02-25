// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PUSH_NOTIFICATION_FAKE_PUSH_NOTIFICATION_SERVICE_H_
#define COMPONENTS_PUSH_NOTIFICATION_FAKE_PUSH_NOTIFICATION_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/push_notification/push_notification_service.h"

namespace push_notification {

class FakePushNotificationService : public PushNotificationService {
 public:
  FakePushNotificationService();
  FakePushNotificationService(const FakePushNotificationService&) = delete;
  FakePushNotificationService& operator=(const FakePushNotificationService&) =
      delete;
  ~FakePushNotificationService() override;
};

}  // namespace push_notification

#endif  // COMPONENTS_PUSH_NOTIFICATION_FAKE_PUSH_NOTIFICATION_SERVICE_H_
