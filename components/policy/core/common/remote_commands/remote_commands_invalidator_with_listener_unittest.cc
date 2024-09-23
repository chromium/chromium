// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/test_support/fake_invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"
#include "components/policy/core/common/remote_commands/remote_commands_invalidator_impl.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using ::testing::_;
using ::testing::WithArgs;

namespace policy {

constexpr char kRemoteCommandsInvalidationType[] = "DEVICE_REMOTE_COMMAND";
constexpr PolicyInvalidationScope kInvalidationScope =
    PolicyInvalidationScope::kDevice;

class FakeNullRemoteCommandsFactory : public RemoteCommandsFactory {
 public:
  std::unique_ptr<RemoteCommandJob> BuildJobForType(
      enterprise_management::RemoteCommand_Type type,
      RemoteCommandsService* service) override {
    return nullptr;
  }
};

class RemoteCommandsInvalidatorWithInvalidationListenerTest
    : public testing::Test {
 public:
  RemoteCommandsInvalidatorWithInvalidationListenerTest()
      : core_(dm_protocol::kChromeDevicePolicyType,
              /*settings_entity_id=*/std::string(),
              &mock_store_,
              task_environment_.GetMainThreadTaskRunner(),
              network::TestNetworkConnectionTracker::CreateGetter()),
        invalidator_(&core_,
                     task_environment_.GetMockClock(),
                     kInvalidationScope) {}

  RemoteCommandsInvalidatorWithInvalidationListenerTest(
      const RemoteCommandsInvalidatorWithInvalidationListenerTest&) = delete;
  RemoteCommandsInvalidatorWithInvalidationListenerTest& operator=(
      const RemoteCommandsInvalidatorWithInvalidationListenerTest&) = delete;

  void EnableInvalidationService() { fake_invalidation_listener_.Start(); }

  void DisableInvalidationService() { fake_invalidation_listener_.Shutdown(); }

  invalidation::DirectInvalidation CreateInvalidation(std::string type) {
    return invalidation::DirectInvalidation(std::move(type),
                                            /*version=*/42,
                                            /*payload=*/"foo_bar");
  }

  invalidation::Invalidation FireInvalidation(std::string type) {
    const auto invalidation = CreateInvalidation(std::move(type));
    fake_invalidation_listener_.FireInvalidation(invalidation);
    return invalidation;
  }

  MockCloudPolicyClient* PrepareCoreForRemoteCommands() {
    auto mock_client = std::make_unique<MockCloudPolicyClient>();
    mock_client->SetDMToken("fake_token");
    auto* mock_client_ptr = mock_client.get();

    // Always report success on fetch commands request.
    ON_CALL(*mock_client_ptr, FetchRemoteCommands(_, _, _, _, _))
        .WillByDefault(
            WithArgs<4>([](CloudPolicyClient::RemoteCommandCallback callback) {
              std::move(callback).Run(DM_STATUS_SUCCESS, {});
            }));

    mock_store_.set_policy_data_for_testing(std::make_unique<em::PolicyData>());

    core_.ConnectForTesting(/*service=*/nullptr, std::move(mock_client));
    core_.StartRemoteCommandsService(
        std::make_unique<FakeNullRemoteCommandsFactory>(), kInvalidationScope);

    return mock_client_ptr;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};

  testing::NiceMock<MockCloudPolicyStore> mock_store_;
  CloudPolicyCore core_;

  invalidation::FakeInvalidationListener fake_invalidation_listener_;
  RemoteCommandsInvalidatorImpl invalidator_;
};

// Tests that remote commands invalidator is correctly initialized and shut
// down according to `InvalidationListener` state.
TEST_F(RemoteCommandsInvalidatorWithInvalidationListenerTest,
       StartsWhenInvalidationListenerStarts) {
  PrepareCoreForRemoteCommands();

  invalidator_.Initialize(&fake_invalidation_listener_);

  EXPECT_FALSE(invalidator_.invalidations_enabled())
      << "Invalidator is enabled before invalidation listener is started";

  fake_invalidation_listener_.Start();

  EXPECT_TRUE(invalidator_.invalidations_enabled());

  fake_invalidation_listener_.Shutdown();

  EXPECT_FALSE(invalidator_.invalidations_enabled())
      << "Invalidator is enabled after invalidation listener is shut down";

  fake_invalidation_listener_.Start();

  EXPECT_TRUE(invalidator_.invalidations_enabled());

  invalidator_.Shutdown();
}

// Tests that remote commands invalidator is correctly initialized and shut
// down according to `RemoteCommandsService` state.
TEST_F(RemoteCommandsInvalidatorWithInvalidationListenerTest,
       StartsWhenRemoteCommandsServiceStarts) {
  fake_invalidation_listener_.Start();

  invalidator_.Initialize(&fake_invalidation_listener_);

  EXPECT_FALSE(invalidator_.invalidations_enabled())
      << "Invalidator is enabled before remote commands service is started";

  PrepareCoreForRemoteCommands();

  EXPECT_TRUE(invalidator_.invalidations_enabled());

  invalidator_.Shutdown();
}

// Tests that remote commands invalidator receives invalidation and initiates
// commands fetch request.
TEST_F(RemoteCommandsInvalidatorWithInvalidationListenerTest,
       FetchesRemoteCommandsOnInvalidation) {
  auto* mock_client = PrepareCoreForRemoteCommands();

  // Expect initial fetch requests from invalidator on initialization.
  EXPECT_CALL(*mock_client, FetchRemoteCommands(_, _, _, _, _));

  invalidator_.Initialize(&fake_invalidation_listener_);
  fake_invalidation_listener_.Start();

  EXPECT_TRUE(invalidator_.invalidations_enabled());
  testing::Mock::VerifyAndClearExpectations(mock_client);

  // Fire two invalidations and check two fetch requests happened.
  EXPECT_CALL(*mock_client, FetchRemoteCommands(_, _, _, _, _)).Times(2);

  fake_invalidation_listener_.FireInvalidation(invalidation::DirectInvalidation(
      kRemoteCommandsInvalidationType, /*version=*/100,
      /*payload=*/"foo_bar"));

  fake_invalidation_listener_.FireInvalidation(invalidation::DirectInvalidation(
      kRemoteCommandsInvalidationType, /*version=*/100,
      /*payload=*/"foo_bar"));

  invalidator_.Shutdown();
}

}  // namespace policy
