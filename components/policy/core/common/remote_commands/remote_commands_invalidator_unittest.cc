// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_invalidator.h"

#include <memory>
#include <string>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/test_support/fake_invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/remote_commands/remote_commands_factory.h"
#include "components/policy/core/common/remote_commands/remote_commands_fetch_reason.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using ::testing::_;
using ::testing::NiceMock;
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

class RemoteCommandsInvalidatorTest : public testing::Test {
 public:
  RemoteCommandsInvalidatorTest()
      : core_(dm_protocol::kChromeDevicePolicyType,
              /*settings_entity_id=*/std::string(),
              &mock_store_,
              task_environment_.GetMainThreadTaskRunner(),
              network::TestNetworkConnectionTracker::CreateGetter()) {}

  RemoteCommandsInvalidatorTest(const RemoteCommandsInvalidatorTest&) = delete;
  RemoteCommandsInvalidatorTest& operator=(
      const RemoteCommandsInvalidatorTest&) = delete;

  invalidation::DirectInvalidation CreateInvalidation(std::string type) {
    return invalidation::DirectInvalidation(std::move(type),
                                            /*version=*/42,
                                            /*payload=*/"foo_bar");
  }

  invalidation::DirectInvalidation FireInvalidation(std::string type) {
    const auto invalidation = CreateInvalidation(std::move(type));
    fake_invalidation_listener_.FireInvalidation(invalidation);
    return invalidation;
  }

  NiceMock<MockCloudPolicyClient>* PrepareCoreForRemoteCommands() {
    auto mock_client = std::make_unique<NiceMock<MockCloudPolicyClient>>();
    mock_client->SetDMToken("fake_token");
    auto* mock_client_ptr = mock_client.get();

    // Always report success on fetch commands request.
    ON_CALL(*mock_client_ptr, FetchRemoteCommands(_, _, _, _, _, _))
        .WillByDefault(
            WithArgs<5>([](CloudPolicyClient::RemoteCommandCallback callback) {
              std::move(callback).Run(DM_STATUS_SUCCESS, {});
            }));

    mock_store_.set_policy_data_for_testing(std::make_unique<em::PolicyData>());

    core_.ConnectForTesting(/*service=*/nullptr, std::move(mock_client));
    core_.StartRemoteCommandsService(
        std::make_unique<FakeNullRemoteCommandsFactory>(), kInvalidationScope);

    return mock_client_ptr;
  }

  void DisconnectCore() { core_.Disconnect(); }

 protected:
  std::unique_ptr<RemoteCommandsInvalidator> CreateInvalidator() {
    return std::make_unique<RemoteCommandsInvalidator>(
        &fake_invalidation_listener_, &core_, task_environment_.GetMockClock(),
        kInvalidationScope);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};

  NiceMock<MockCloudPolicyStore> mock_store_;
  CloudPolicyCore core_;

  invalidation::FakeInvalidationListener fake_invalidation_listener_;
};

// Tests that remote commands invalidator is correctly initialized and shut
// down according to `InvalidationListener` state.
TEST_F(RemoteCommandsInvalidatorTest, StartsWhenInvalidationListenerStarts) {
  PrepareCoreForRemoteCommands();

  auto invalidator = CreateInvalidator();

  EXPECT_TRUE(invalidator->IsRegistered());
  EXPECT_FALSE(invalidator->AreInvalidationsEnabled())
      << "Invalidator is enabled before invalidation listener is started";

  fake_invalidation_listener_.Start();

  EXPECT_TRUE(invalidator->AreInvalidationsEnabled());

  fake_invalidation_listener_.Shutdown();

  EXPECT_TRUE(invalidator->IsRegistered());
  EXPECT_FALSE(invalidator->AreInvalidationsEnabled())
      << "Invalidator is enabled after invalidation listener is shut down";

  fake_invalidation_listener_.Start();

  EXPECT_TRUE(invalidator->AreInvalidationsEnabled());
}

// Tests that remote commands invalidator is correctly initialized and shut
// down according to `RemoteCommandsService` state.
TEST_F(RemoteCommandsInvalidatorTest, StartsWhenRemoteCommandsServiceStarts) {
  fake_invalidation_listener_.Start();

  auto invalidator = CreateInvalidator();

  EXPECT_FALSE(invalidator->IsRegistered())
      << "Invalidartor is registered before remote commands service is started";
  EXPECT_FALSE(invalidator->AreInvalidationsEnabled())
      << "Invalidator is enabled before remote commands service is started";

  PrepareCoreForRemoteCommands();

  EXPECT_TRUE(invalidator->IsRegistered());
  EXPECT_TRUE(invalidator->AreInvalidationsEnabled());

  DisconnectCore();

  EXPECT_FALSE(invalidator->IsRegistered());
  EXPECT_FALSE(invalidator->AreInvalidationsEnabled());

  PrepareCoreForRemoteCommands();

  EXPECT_TRUE(invalidator->IsRegistered());
  EXPECT_TRUE(invalidator->AreInvalidationsEnabled());
}

// Tests that remote commands invalidtor does initial fetch request when
// InvalidationListener subscribes for invalidations.
TEST_F(RemoteCommandsInvalidatorTest,
       DoesInitialFetchWhenInvaldiationsAreEnabled) {
  auto* mock_client = PrepareCoreForRemoteCommands();
  fake_invalidation_listener_.Start();

  // Expect two startup fetches on InvalidationListener initial start and on
  // restart.
  EXPECT_CALL(
      *mock_client,
      FetchRemoteCommands(_, _, _, _, RemoteCommandsFetchReason::kStartup, _))
      .Times(2);

  auto invalidator = CreateInvalidator();

  fake_invalidation_listener_.Shutdown();
  fake_invalidation_listener_.Start();
}

// Tests that remote commands invalidator receives invalidation and initiates
// commands fetch request.
TEST_F(RemoteCommandsInvalidatorTest, FetchesRemoteCommandsOnInvalidation) {
  auto* mock_client = PrepareCoreForRemoteCommands();

  auto invalidator = CreateInvalidator();
  fake_invalidation_listener_.Start();

  EXPECT_TRUE(invalidator->AreInvalidationsEnabled());
  testing::Mock::VerifyAndClearExpectations(mock_client);

  // Fire two invalidations and check two fetch requests happened.
  EXPECT_CALL(*mock_client,
              FetchRemoteCommands(_, _, _, _,
                                  RemoteCommandsFetchReason::kInvalidation, _))
      .Times(2);

  fake_invalidation_listener_.FireInvalidation(invalidation::DirectInvalidation(
      kRemoteCommandsInvalidationType, /*version=*/100,
      /*payload=*/"foo_bar"));

  fake_invalidation_listener_.FireInvalidation(invalidation::DirectInvalidation(
      kRemoteCommandsInvalidationType, /*version=*/100,
      /*payload=*/"foo_bar"));
}

TEST_F(RemoteCommandsInvalidatorTest, HasCorrectInvalidationType) {
  RemoteCommandsInvalidator device_invalidator(
      &fake_invalidation_listener_, &core_, task_environment_.GetMockClock(),
      PolicyInvalidationScope::kDevice);
  RemoteCommandsInvalidator browser_invalidator(
      &fake_invalidation_listener_, &core_, task_environment_.GetMockClock(),
      PolicyInvalidationScope::kCBCM);
  RemoteCommandsInvalidator user_invalidator(
      &fake_invalidation_listener_, &core_, task_environment_.GetMockClock(),
      PolicyInvalidationScope::kUser);

  EXPECT_EQ(device_invalidator.GetType(), "DEVICE_REMOTE_COMMAND");
  EXPECT_EQ(browser_invalidator.GetType(), "BROWSER_REMOTE_COMMAND");
  EXPECT_EQ(user_invalidator.GetType(),
#if BUILDFLAG(IS_CHROMEOS)
            "CONSUMER_USER_REMOTE_COMMAND"
#else
            "PROFILE_REMOTE_COMMAND"
#endif

  );
}

}  // namespace policy
