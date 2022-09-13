// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_USER_PERMISSION_SERVICE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_USER_PERMISSION_SERVICE_H_

#include "base/callback.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

class MockUserPermissionService : public UserPermissionService {
 public:
  MockUserPermissionService();
  ~MockUserPermissionService() override;

  MOCK_METHOD(void,
              CanCollectSignals,
              (const UserContext&, CanCollectCallback),
              (override));
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_USER_PERMISSION_SERVICE_H_
