// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/remote_commands_invalidator.h"

#include <set>
#include <string>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using ::testing::_;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::StrictMock;

namespace policy {

// TODO(crbug.com/1319443): Remove mock and test the actual invalidator.
class MockRemoteCommandInvalidator : public RemoteCommandsInvalidator {
 public:
  MockRemoteCommandInvalidator()
      : RemoteCommandsInvalidator("RemoteCommands.Test",
                                  PolicyInvalidationScope::kDevice) {}
  MockRemoteCommandInvalidator(const MockRemoteCommandInvalidator&) = delete;
  MockRemoteCommandInvalidator& operator=(const MockRemoteCommandInvalidator&) =
      delete;

  MOCK_METHOD0(OnInitialize, void());
  MOCK_METHOD0(OnShutdown, void());
  MOCK_METHOD0(OnStart, void());
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD1(DoRemoteCommandsFetch, void(const invalidation::Invalidation&));
  MOCK_METHOD0(DoInitialRemoteCommandsFetch, void());

  void SetInvalidationTopic(const invalidation::Topic& topic) {
    // An initial remote command fetch must be triggered when subscribed to a
    // topic.
    EXPECT_CALL(*this, DoInitialRemoteCommandsFetch()).Times(1);

    em::PolicyData policy_data;
    policy_data.set_command_invalidation_topic(topic);
    ReloadPolicyData(&policy_data);

    // Reloading policy will make invalidator subscribe to topic. Pretend we got
    // signal for successful subscription to verify the behaviour upon
    // subscription.
    OnSuccessfullySubscribed(topic);

    Mock::VerifyAndClearExpectations(this);
  }

  void ClearInvalidationTopic() {
    const em::PolicyData policy_data;
    ReloadPolicyData(&policy_data);
  }
};

class RemoteCommandsInvalidatorTest : public testing::Test {
 public:
  RemoteCommandsInvalidatorTest()
      : kTestingTopic1("abcdef"), kTestingTopic2("defabc") {}
  RemoteCommandsInvalidatorTest(const RemoteCommandsInvalidatorTest&) = delete;
  RemoteCommandsInvalidatorTest& operator=(
      const RemoteCommandsInvalidatorTest&) = delete;

  void EnableInvalidationService() {
    invalidation_service_.SetInvalidatorState(
        invalidation::InvalidatorState::kEnabled);
  }

  void DisableInvalidationService() {
    invalidation_service_.SetInvalidatorState(
        invalidation::InvalidatorState::kDisabled);
  }

  invalidation::Invalidation CreateInvalidation(
      const invalidation::Topic& topic) {
    return invalidation::Invalidation(topic, /* version= */ 42,
                                      /*payload=*/"foo_bar");
  }

  // Will send invalidation to handler if `IsInvalidatorRegistered(topic) ==
  // true`.
  invalidation::Invalidation FireInvalidation(
      const invalidation::Topic& topic) {
    const invalidation::Invalidation invalidation = CreateInvalidation(topic);
    invalidation_service_.EmitInvalidationForTest(invalidation);
    return invalidation;
  }

  bool IsInvalidatorRegistered() const {
    return invalidation_service_.HasObserver(&invalidator_);
  }

  bool IsInvalidatorRegistered(const invalidation::Topic& topic) {
    return invalidation_service_.invalidator_registrar()
        .GetRegisteredTopics(&invalidator_)
        .contains(topic);
  }

  std::set<std::string> GetSubscribedTopics() {
    std::set<std::string> topics;
    for (const auto& topic : invalidation_service_.invalidator_registrar()
                                 .GetAllSubscribedTopics()) {
      topics.insert(topic.first);
    }

    return topics;
  }

  std::set<std::string> GetRegisteredTopics() {
    std::set<std::string> topics;
    for (const auto& topic :
         invalidation_service_.invalidator_registrar().GetRegisteredTopics(
             &invalidator_)) {
      topics.insert(topic.first);
    }

    return topics;
  }

  void VerifyExpectations() { Mock::VerifyAndClearExpectations(&invalidator_); }

 protected:
  // Initialize and start the invalidator.
  void InitializeAndStart() {
    EXPECT_CALL(invalidator_, OnInitialize()).Times(1);
    invalidator_.Initialize(&invalidation_service_);
    VerifyExpectations();

    EXPECT_CALL(invalidator_, OnStart()).Times(1);
    invalidator_.Start();

    VerifyExpectations();
  }

  // Stop and shutdown the invalidator.
  void StopAndShutdown() {
    EXPECT_CALL(invalidator_, OnStop()).Times(1);
    EXPECT_CALL(invalidator_, OnShutdown()).Times(1);
    invalidator_.Shutdown();

    VerifyExpectations();
  }

  // Test that the invalidator is not registered to `topic`.
  void VerifyInvalidationDisabled(const invalidation::Topic& topic) {
    EXPECT_FALSE(IsInvalidatorRegistered(topic));
  }

  // Test that the invalidator is enabled and registered to `topic`.
  void VerifyInvalidationEnabled(const invalidation::Topic& topic) {
    EXPECT_TRUE(invalidator_.invalidations_enabled());
    EXPECT_TRUE(IsInvalidatorRegistered(topic));
  }

  invalidation::Topic kTestingTopic1;
  invalidation::Topic kTestingTopic2;

  base::test::SingleThreadTaskEnvironment task_environment_;

  invalidation::FakeInvalidationService invalidation_service_;
  StrictMock<MockRemoteCommandInvalidator> invalidator_;
};

// Verifies that only the fired invalidations will be received.
TEST_F(RemoteCommandsInvalidatorTest, FiredInvalidation) {
  InitializeAndStart();

  // Invalidator won't work at this point.
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  // Load the policy data, it should work now.
  invalidator_.SetInvalidationTopic(kTestingTopic1);
  EXPECT_TRUE(invalidator_.invalidations_enabled());

  base::RunLoop().RunUntilIdle();
  // No invalidation will be received if no invalidation is fired.
  VerifyExpectations();

  // Invalidator does not subscribe to other stuff.
  VerifyInvalidationDisabled(kTestingTopic2);

  // Fire the invalidation, it should be acknowledged and trigger a remote
  // commands fetch.
  EXPECT_CALL(invalidator_,
              DoRemoteCommandsFetch(Eq(CreateInvalidation(kTestingTopic1))))
      .Times(1);
      FireInvalidation(kTestingTopic1);

  base::RunLoop().RunUntilIdle();
  VerifyExpectations();

  StopAndShutdown();
}

// Verifies that no invalidation will be received when invalidator is shutdown.
TEST_F(RemoteCommandsInvalidatorTest, ShutDown) {
  EXPECT_FALSE(invalidator_.invalidations_enabled());
  EXPECT_FALSE(IsInvalidatorRegistered());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(invalidator_.invalidations_enabled());
}

// Verifies that no invalidation will be received when invalidator is stopped.
TEST_F(RemoteCommandsInvalidatorTest, Stopped) {
  EXPECT_CALL(invalidator_, OnInitialize()).Times(1);
  invalidator_.Initialize(&invalidation_service_);
  VerifyExpectations();

  EXPECT_FALSE(invalidator_.invalidations_enabled());
  EXPECT_FALSE(IsInvalidatorRegistered());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  EXPECT_CALL(invalidator_, OnShutdown()).Times(1);
  invalidator_.Shutdown();
}

// Verifies that stated/stopped state changes work as expected.
TEST_F(RemoteCommandsInvalidatorTest, StartedStateChange) {
  InitializeAndStart();

  // Invalidator requires topic to work.
  VerifyInvalidationDisabled(kTestingTopic1);
  EXPECT_FALSE(invalidator_.invalidations_enabled());
  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);
  EXPECT_EQ(GetSubscribedTopics(), std::set<std::string>{kTestingTopic1});
  EXPECT_EQ(GetRegisteredTopics(), std::set<std::string>{kTestingTopic1});

  // Stop and restart invalidator.
  EXPECT_CALL(invalidator_, OnStop()).Times(1);
  invalidator_.Stop();
  VerifyExpectations();

  EXPECT_FALSE(invalidator_.invalidations_enabled());
  EXPECT_EQ(GetSubscribedTopics(), std::set<std::string>{kTestingTopic1});
  EXPECT_EQ(GetRegisteredTopics(), std::set<std::string>{});

  EXPECT_CALL(invalidator_, OnStart()).Times(1);
  invalidator_.Start();
  VerifyExpectations();

  // Invalidator requires topic to work.
  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);
  EXPECT_EQ(GetSubscribedTopics(), std::set<std::string>{kTestingTopic1});
  EXPECT_EQ(GetRegisteredTopics(), std::set<std::string>{kTestingTopic1});

  StopAndShutdown();
}

// Verifies that registered state changes work as expected.
TEST_F(RemoteCommandsInvalidatorTest, RegistedStateChange) {
  InitializeAndStart();

  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);

  invalidator_.SetInvalidationTopic(kTestingTopic2);
  VerifyInvalidationEnabled(kTestingTopic2);
  VerifyInvalidationDisabled(kTestingTopic1);

  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);
  VerifyInvalidationDisabled(kTestingTopic2);

  invalidator_.ClearInvalidationTopic();
  VerifyInvalidationDisabled(kTestingTopic1);
  VerifyInvalidationDisabled(kTestingTopic2);
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  invalidator_.SetInvalidationTopic(kTestingTopic2);
  VerifyInvalidationEnabled(kTestingTopic2);
  VerifyInvalidationDisabled(kTestingTopic1);

  StopAndShutdown();
}

// Verifies that invalidation service enabled state changes work as expected.
TEST_F(RemoteCommandsInvalidatorTest, InvalidationServiceEnabledStateChanged) {
  InitializeAndStart();

  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);

  DisableInvalidationService();
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  EnableInvalidationService();
  VerifyInvalidationEnabled(kTestingTopic1);

  EnableInvalidationService();
  VerifyInvalidationEnabled(kTestingTopic1);

  DisableInvalidationService();
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  DisableInvalidationService();
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  StopAndShutdown();
}

}  // namespace policy
