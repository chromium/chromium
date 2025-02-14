// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/auth_factor_editor.h"

#include "base/test/task_environment.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class AuthFactorEditorTest : public testing::Test {
 public:
  AuthFactorEditorTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {
    CryptohomeMiscClient::InitializeFake();
    SystemSaltGetter::Initialize();
  }

  ~AuthFactorEditorTest() override {
    SystemSaltGetter::Shutdown();
    CryptohomeMiscClient::Shutdown();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ::testing::StrictMock<MockUserDataAuthClient> mock_client_;
};

}  // namespace ash
