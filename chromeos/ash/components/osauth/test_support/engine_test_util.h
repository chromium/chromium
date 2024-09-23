// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_ENGINE_TEST_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_ENGINE_TEST_UTIL_H_

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/osauth/impl/cryptohome_core_impl.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class EngineTestBase : public ::testing::Test {
 protected:
  EngineTestBase();
  ~EngineTestBase() override;

  // Basic infrastructure for the test, including a real cryptohome core
  // instance based on a mock userdataauth.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ash::MockUserDataAuthClient mock_udac_;
  CryptohomeCoreImpl core_;
  TestingPrefServiceSimple prefs_;
  user_manager::FakeUserManager user_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::ScopedStubInstallAttributes install_attributes{
      ash::StubInstallAttributes::CreateConsumerOwned()};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_TEST_SUPPORT_ENGINE_TEST_UTIL_H_
