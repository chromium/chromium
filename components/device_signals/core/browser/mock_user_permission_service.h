// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_USER_PERMISSION_SERVICE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_USER_PERMISSION_SERVICE_H_

#include "components/device_signals/core/browser/user_permission_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

class MockUserPermissionService : public UserPermissionService {
 public:
  MockUserPermissionService();
  ~MockUserPermissionService() override;

  MOCK_METHOD(bool, HasUserConsented, (), (const override));
  MOCK_METHOD(bool, ShouldCollectConsent, (), (const override));
  MOCK_METHOD(UserPermission,
              CanUserCollectSignals,
              (const UserContext&),
              (const override));

  MOCK_METHOD(UserPermission, CanCollectSignals, (), (const override));
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_USER_PERMISSION_SERVICE_H_
