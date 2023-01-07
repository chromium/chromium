// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_MOCK_SAFE_MODE_DELEGATE_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_MOCK_SAFE_MODE_DELEGATE_H_

#include <string>

#include "chromeos/ash/components/login/auth/safe_mode_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockSafeModeDelegate : public SafeModeDelegate {
 public:
  MockSafeModeDelegate();
  ~MockSafeModeDelegate() override;

  MOCK_METHOD(bool, IsSafeMode, (), (override));
  MOCK_METHOD(void,
              CheckSafeModeOwnership,
              (const std::string& user_id_hash, IsOwnerCallback callback),
              (override));
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_MOCK_SAFE_MODE_DELEGATE_H_
