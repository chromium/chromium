// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_PUSH_NOTIFICATION_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_PUSH_NOTIFICATION_MANAGER_H_

#include "components/optimization_guide/core/push_notification_manager.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

class MockPushNotificationManager : public PushNotificationManager {
 public:
  MockPushNotificationManager();
  ~MockPushNotificationManager() override;

  MOCK_METHOD(void, SetDelegate, (PushNotificationManager::Delegate*));
  MOCK_METHOD(void, OnDelegateReady, (), (override));
  MOCK_METHOD(void,
              OnNewPushNotification,
              (const proto::HintNotificationPayload&),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer));
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_PUSH_NOTIFICATION_MANAGER_H_
