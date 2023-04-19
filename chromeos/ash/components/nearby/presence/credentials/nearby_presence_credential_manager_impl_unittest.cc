// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager_impl.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/nearby/presence/credentials/fake_local_device_data_provider.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::nearby::presence {

class NearbyPresenceCredentialManagerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<LocalDeviceDataProvider> local_device_data_provider =
        std::make_unique<FakeLocalDeviceDataProvider>();
    fake_local_device_data_provider_ =
        static_cast<FakeLocalDeviceDataProvider*>(
            local_device_data_provider.get());

    credential_manager_ = std::make_unique<NearbyPresenceCredentialManagerImpl>(
        &pref_service_, identity_test_env_.identity_manager(),
        std::move(local_device_data_provider));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeLocalDeviceDataProvider* fake_local_device_data_provider_ = nullptr;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<NearbyPresenceCredentialManager> credential_manager_;
};

TEST_F(NearbyPresenceCredentialManagerImplTest, PresenceRegistered) {
  EXPECT_FALSE(credential_manager_->IsLocalDeviceRegistered());

  // Simulate the user information not saved in the `LocalDeviceDataProvider`.
  //
  // TODO(b/276307539): Instead of using the fake, change this test to
  // reflect `RegisterPresence` once implemented.
  fake_local_device_data_provider_->SetIsUserRegistrationInfoSaved(true);
  EXPECT_TRUE(credential_manager_->IsLocalDeviceRegistered());
}

}  // namespace ash::nearby::presence
