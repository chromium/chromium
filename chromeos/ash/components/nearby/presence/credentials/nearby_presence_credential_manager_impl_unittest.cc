// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager_impl.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/nearby/presence/credentials/nearby_presence_credential_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::nearby::presence {

class NearbyPresenceCredentialManagerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    credential_manager_ = std::make_unique<NearbyPresenceCredentialManagerImpl>(
        &pref_service_, identity_test_env_.identity_manager());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<NearbyPresenceCredentialManager> credential_manager_;
};

TEST_F(NearbyPresenceCredentialManagerImplTest, ObjectConstructionSuccess) {
  ASSERT_TRUE(credential_manager_);
}

}  // namespace ash::nearby::presence
