// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>

#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider_impl.h"

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider.h"
#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::nearby::presence {

class LocalDeviceDataProviderImplTest : public testing::Test {
 public:
  void SetUp() override {
    RegisterNearbyPresenceCredentialPrefs(pref_service_.registry());
  }

  void CreateDataProvider() {
    local_device_data_provider_ = std::make_unique<LocalDeviceDataProviderImpl>(
        &pref_service_, identity_test_env_.identity_manager());
  }

  void DestroyDataProvider() { local_device_data_provider_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider_;
};

TEST_F(LocalDeviceDataProviderImplTest, DeviceId) {
  CreateDataProvider();

  // A 10-character alphanumeric ID is automatically generated if one doesn't
  // already exist.
  std::string id = local_device_data_provider_->GetDeviceId();
  EXPECT_EQ(10u, id.size());
  for (const char c : id) {
    EXPECT_TRUE(std::isalnum(c));
  }

  // The ID is persisted.
  DestroyDataProvider();
  CreateDataProvider();
  EXPECT_EQ(id, local_device_data_provider_->GetDeviceId());
}

}  // namespace ash::nearby::presence
