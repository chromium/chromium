// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"

#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class MockMachineLevelUserCloudPolicyStore
    : public MachineLevelUserCloudPolicyStore {
 public:
  MockMachineLevelUserCloudPolicyStore()
      : MachineLevelUserCloudPolicyStore(
            DMToken::CreateEmptyToken(),
            std::string(),
            base::FilePath(),
            base::FilePath(),
            base::FilePath(),
            base::FilePath(),
            scoped_refptr<base::SequencedTaskRunner>()) {}

  MOCK_METHOD0(LoadImmediately, void(void));
};

class MachineLevelUserCloudPolicyManagerTest : public ::testing::Test {
 public:
  MachineLevelUserCloudPolicyManagerTest() = default;
  MachineLevelUserCloudPolicyManagerTest(
      const MachineLevelUserCloudPolicyManagerTest&) = delete;
  MachineLevelUserCloudPolicyManagerTest& operator=(
      const MachineLevelUserCloudPolicyManagerTest&) = delete;
  ~MachineLevelUserCloudPolicyManagerTest() override { manager_->Shutdown(); }

  void SetUp() override {
    auto store = std::make_unique<MockMachineLevelUserCloudPolicyStore>();
    store_ = store.get();
    manager_ = std::make_unique<MachineLevelUserCloudPolicyManager>(
        std::move(store), std::unique_ptr<CloudExternalDataManager>(),
        base::FilePath(), scoped_refptr<base::SequencedTaskRunner>(),
        network::TestNetworkConnectionTracker::CreateGetter());
  }

  SchemaRegistry schema_registry_;
  raw_ptr<MockMachineLevelUserCloudPolicyStore, DanglingUntriaged> store_ =
      nullptr;
  std::unique_ptr<MachineLevelUserCloudPolicyManager> manager_;
};

TEST_F(MachineLevelUserCloudPolicyManagerTest, InitManager) {
  EXPECT_CALL(*store_, LoadImmediately());
  manager_->Init(&schema_registry_);
  ::testing::Mock::VerifyAndClearExpectations(store_);
}

}  // namespace policy
