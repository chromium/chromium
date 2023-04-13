// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider_impl.h"

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::nearby::presence {

class LocalDeviceDataProviderImplTest : public testing::Test {
 public:
  void SetUp() override {
    local_device_data_provider_ = std::make_unique<LocalDeviceDataProviderImpl>(
        &pref_service_, identity_test_env_.identity_manager());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider_;
};

TEST_F(LocalDeviceDataProviderImplTest, ObjectConstructionSuccess) {
  ASSERT_TRUE(local_device_data_provider_);
}

}  // namespace ash::nearby::presence
