// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/push_notification/fake_push_notification_service.h"

#include "components/push_notification/push_notification_client_manager.h"

namespace push_notification {

FakePushNotificationService::FakePushNotificationService()
    : PushNotificationService() {}
FakePushNotificationService::~FakePushNotificationService() = default;

}  // namespace push_notification
