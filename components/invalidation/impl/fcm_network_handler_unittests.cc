// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_network_handler.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/status.h"
#include "google_apis/gcm/engine/account_mapping.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::TestMockTimeTaskRunner;
using gcm::InstanceIDHandler;
using instance_id::InstanceID;
using instance_id::InstanceIDDriver;
using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArg;

namespace invalidation {

namespace {

const char kInvalidationsAppId[] = "com.google.chrome.fcm.invalidations";

base::Time GetDummyNow() {
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromUTCString("2017-01-02T00:00:01Z", &out_time));
  return out_time;
}

gcm::IncomingMessage CreateValidMessage() {
  gcm::IncomingMessage message;
  message.data["payload"] = "payload";
  message.data["version"] = "42";
  message.sender_id = "private_topic";
  return message;
}

class MockInstanceID : public InstanceID {
 public:
  MockInstanceID() : InstanceID("app_id", /*gcm_driver=*/nullptr) {}
  ~MockInstanceID() override = default;

  MOCK_METHOD1(GetID, void(GetIDCallback callback));
  MOCK_METHOD1(GetCreationTime, void(GetCreationTimeCallback callback));
  MOCK_METHOD5(GetToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    base::TimeDelta time_to_live,
                    std::set<Flags> flags,
                    GetTokenCallback callback));
  MOCK_METHOD4(ValidateToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    const std::string& token,
                    ValidateTokenCallback callback));

 protected:
  MOCK_METHOD3(DeleteTokenImpl,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    DeleteTokenCallback callback));
  MOCK_METHOD1(DeleteIDImpl, void(DeleteIDCallback callback));
};

class MockGCMDriver : public gcm::GCMDriver {
 public:
  MockGCMDriver()
      : GCMDriver(
            /*store_path=*/base::FilePath(),
            /*blocking_task_runner=*/nullptr) {}
  ~MockGCMDriver() override = default;

  MOCK_METHOD4(ValidateRegistration,
               void(const std::string& app_id,
                    const std::vector<std::string>& sender_ids,
                    const std::string& registration_id,
                    ValidateRegistrationCallback callback));
  MOCK_METHOD0(OnSignedIn, void());
  MOCK_METHOD0(OnSignedOut, void());
  MOCK_METHOD1(AddConnectionObserver,
               void(gcm::GCMConnectionObserver* observer));
  MOCK_METHOD1(RemoveConnectionObserver,
               void(gcm::GCMConnectionObserver* observer));
  MOCK_METHOD0(Enable, void());
  MOCK_METHOD0(Disable, void());
  MOCK_CONST_METHOD0(GetGCMClientForTesting, gcm::GCMClient*());
  MOCK_CONST_METHOD0(IsStarted, bool());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_METHOD2(GetGCMStatistics,
               void(GetGCMStatisticsCallback callback,
                    ClearActivityLogs clear_logs));
  MOCK_METHOD2(SetGCMRecording,
               void(const GCMStatisticsRecordingCallback& callback,
                    bool recording));
  MOCK_METHOD1(SetAccountTokens,
               void(const std::vector<gcm::GCMClient::AccountTokenInfo>&
                        account_tokens));
  MOCK_METHOD1(UpdateAccountMapping,
               void(const gcm::AccountMapping& account_mapping));
  MOCK_METHOD1(RemoveAccountMapping, void(const CoreAccountId& account_id));
  MOCK_METHOD0(GetLastTokenFetchTime, base::Time());
  MOCK_METHOD1(SetLastTokenFetchTime, void(const base::Time& time));
  MOCK_METHOD0(GetInstanceIDHandlerInternal, InstanceIDHandler*());
  MOCK_METHOD2(AddHeartbeatInterval,
               void(const std::string& scope, int interval_ms));
  MOCK_METHOD1(RemoveHeartbeatInterval, void(const std::string& scope));

 protected:
  MOCK_METHOD1(EnsureStarted,
               gcm::GCMClient::Result(gcm::GCMClient::StartMode start_mode));
  MOCK_METHOD2(RegisterImpl,
               void(const std::string& app_id,
                    const std::vector<std::string>& sender_ids));
  MOCK_METHOD1(UnregisterImpl, void(const std::string& app_id));
  MOCK_METHOD3(SendImpl,
               void(const std::string& app_id,
                    const std::string& receiver_id,
                    const gcm::OutgoingMessage& message));
  MOCK_METHOD2(RecordDecryptionFailure,
               void(const std::string& app_id,
                    gcm::GCMDecryptionResult result));
};

class MockInstanceIDDriver : public InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

  MOCK_METHOD1(GetInstanceID, InstanceID*(const std::string& app_id));
  MOCK_METHOD1(RemoveInstanceID, void(const std::string& app_id));
  MOCK_CONST_METHOD1(ExistsInstanceID, bool(const std::string& app_id));
};

class MockOnTokenCallback {
 public:
  FCMSyncNetworkChannel::TokenCallback Get() {
    return base::BindRepeating(&MockOnTokenCallback::Run,
                               base::Unretained(this));
  }

  MOCK_METHOD1(Run, void(const std::string&));
};

class MockOnMessageCallback {
 public:
  FCMSyncNetworkChannel::MessageCallback Get() {
    return base::BindRepeating(&MockOnMessageCallback::Run,
                               base::Unretained(this));
  }

  MOCK_METHOD4(Run,
               void(const std::string&,
                    const std::string&,
                    const std::string&,
                    int64_t));
};

}  // namespace

class FCMNetworkHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    // Our app handler obtains InstanceID through InstanceIDDriver. We mock
    // InstanceIDDriver and return MockInstanceID through it.
    mock_instance_id_driver_ =
        std::make_unique<StrictMock<MockInstanceIDDriver>>();
    mock_instance_id_ = std::make_unique<StrictMock<MockInstanceID>>();
    mock_gcm_driver_ = std::make_unique<StrictMock<MockGCMDriver>>();

    // This is called in FCMNetworkHandler.
    EXPECT_CALL(*mock_instance_id_driver_, GetInstanceID(kInvalidationsAppId))
        .WillRepeatedly(Return(mock_instance_id_.get()));
  }

  std::unique_ptr<FCMNetworkHandler> MakeHandler(
      const std::string& sender_id = "fake_sender_id") {
    return std::make_unique<FCMNetworkHandler>(mock_gcm_driver_.get(),
                                               mock_instance_id_driver_.get(),
                                               sender_id, kInvalidationsAppId);
  }

  std::unique_ptr<FCMNetworkHandler> MakeHandlerReadyForMessage(
      FCMSyncNetworkChannel::MessageCallback mock_on_message_callback) {
    std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();
    handler->SetMessageReceiver(mock_on_message_callback);
    EXPECT_CALL(*mock_instance_id(), GetToken)
        .WillOnce(WithArg<4>(Invoke([](InstanceID::GetTokenCallback callback) {
          std::move(callback).Run("token", InstanceID::Result::SUCCESS);
        })));
    handler->StartListening();
    return handler;
  }

  StrictMock<MockInstanceID>* mock_instance_id() {
    return mock_instance_id_.get();
  }

  scoped_refptr<TestMockTimeTaskRunner> CreateFakeTaskRunnerAndInjectToHandler(
      std::unique_ptr<FCMNetworkHandler>& handler) {
    scoped_refptr<TestMockTimeTaskRunner> task_runner(
        new TestMockTimeTaskRunner(GetDummyNow(), base::TimeTicks::Now()));

    auto token_validation_timer =
        std::make_unique<base::OneShotTimer>(task_runner->GetMockTickClock());
    token_validation_timer->SetTaskRunner(task_runner);
    handler->SetTokenValidationTimerForTesting(
        std::move(token_validation_timer));
    return task_runner;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<StrictMock<MockGCMDriver>> mock_gcm_driver_;
  std::unique_ptr<StrictMock<MockInstanceIDDriver>> mock_instance_id_driver_;
  std::unique_ptr<StrictMock<MockInstanceID>> mock_instance_id_;
};

class FCMNetworkHandlerTestWithTTL : public FCMNetworkHandlerTest {
 public:
  static constexpr int kTimeToLiveInSeconds = 100;

  FCMNetworkHandlerTestWithTTL() {
    base::FieldTrialParams feature_params = {
        {"time_to_live_seconds", base::NumberToString(kTimeToLiveInSeconds)}};
    override_features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{switches::kPolicyInstanceIDTokenTTL, feature_params}},
        /*disabled_features=*/{});
  }

  ~FCMNetworkHandlerTestWithTTL() override = default;

 private:
  base::test::ScopedFeatureList override_features_;
};

TEST_F(FCMNetworkHandlerTest, ShouldPassTheTokenOnceReceived) {
  std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();

  MockOnTokenCallback mock_on_token_callback;
  handler->SetTokenReceiver(mock_on_token_callback.Get());

  // Check that the handler gets the token through GetToken.
  EXPECT_CALL(*mock_instance_id(), GetToken)
      .WillOnce(WithArg<4>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token", InstanceID::Result::SUCCESS);
      })));
  EXPECT_CALL(mock_on_token_callback, Run("token")).Times(1);
  handler->StartListening();
}

TEST_F(FCMNetworkHandlerTest, ShouldPassTheTokenOnceSubscribed) {
  std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();

  MockOnTokenCallback mock_on_token_callback;

  // Check that the handler gets the token through GetToken.
  EXPECT_CALL(*mock_instance_id(), GetToken)
      .WillOnce(WithArg<4>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token", InstanceID::Result::SUCCESS);
      })));
  EXPECT_CALL(mock_on_token_callback, Run).Times(0);
  handler->StartListening();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(mock_on_token_callback, Run("token")).Times(1);
  handler->SetTokenReceiver(mock_on_token_callback.Get());
}

TEST_F(FCMNetworkHandlerTest, ShouldNotInvokeMessageCallbackOnEmptyMessage) {
  MockOnMessageCallback mock_on_message_callback;
  gcm::IncomingMessage message;

  std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();
  EXPECT_CALL(mock_on_message_callback, Run).Times(0);
  handler->SetMessageReceiver(mock_on_message_callback.Get());
  EXPECT_CALL(*mock_instance_id(), GetToken)
      .WillOnce(WithArg<4>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token", InstanceID::Result::SUCCESS);
      })));

  handler->StartListening();
  handler->OnMessage(kInvalidationsAppId, gcm::IncomingMessage());
}

TEST_F(FCMNetworkHandlerTest, ShouldInvokeMessageCallbackOnValidMessage) {
  base::HistogramTester histogram_tester;
  MockOnMessageCallback mock_on_message_callback;
  gcm::IncomingMessage message = CreateValidMessage();

  std::unique_ptr<FCMNetworkHandler> handler =
      MakeHandlerReadyForMessage(mock_on_message_callback.Get());
  EXPECT_CALL(mock_on_message_callback, Run("payload", "private_topic", "", 42))
      .Times(1);
  EXPECT_CALL(mock_on_message_callback,
              Run("payload", "private_topic", "public_topic", 42))
      .Times(1);
  handler->OnMessage(kInvalidationsAppId, message);
  message.data["external_name"] = "public_topic";
  handler->OnMessage(kInvalidationsAppId, message);

  EXPECT_THAT(
      histogram_tester.GetAllSamples("FCMInvalidations.FCMMessageStatus"),
      testing::ElementsAre(base::Bucket(
          static_cast<int>(InvalidationParsingStatus::kSuccess) /* min */,
          2 /* count */)));
}

TEST_F(FCMNetworkHandlerTest,
       ShouldNotInvokeMessageCallbackOnMessageWithEmptyVersion) {
  base::HistogramTester histogram_tester;
  MockOnMessageCallback mock_on_message_callback;
  gcm::IncomingMessage message = CreateValidMessage();
  // Clear version.
  auto it = message.data.find("version");
  message.data.erase(it);

  std::unique_ptr<FCMNetworkHandler> handler =
      MakeHandlerReadyForMessage(mock_on_message_callback.Get());
  EXPECT_CALL(mock_on_message_callback, Run).Times(0);
  handler->OnMessage(kInvalidationsAppId, message);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("FCMInvalidations.FCMMessageStatus"),
      testing::ElementsAre(base::Bucket(
          static_cast<int>(InvalidationParsingStatus::kVersionEmpty) /* min */,
          1 /* count */)));
}

TEST_F(FCMNetworkHandlerTest,
       ShouldNotInvokeMessageCallbackOnMessageWithInvalidVersion) {
  base::HistogramTester histogram_tester;
  MockOnMessageCallback mock_on_message_callback;
  gcm::IncomingMessage message = CreateValidMessage();
  // Set version to something that's not a valid number.
  message.data["version"] = "notanumber";

  std::unique_ptr<FCMNetworkHandler> handler =
      MakeHandlerReadyForMessage(mock_on_message_callback.Get());
  EXPECT_CALL(mock_on_message_callback, Run).Times(0);
  handler->OnMessage(kInvalidationsAppId, message);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("FCMInvalidations.FCMMessageStatus"),
      testing::ElementsAre(base::Bucket(
          /*min=*/static_cast<int>(InvalidationParsingStatus::kVersionInvalid),
          /*count=*/1)));
}

TEST_F(FCMNetworkHandlerTest,
       ShouldNotInvokeMessageCallbackOnMessageWithEmptyPrivateTopic) {
  base::HistogramTester histogram_tester;
  MockOnMessageCallback mock_on_message_callback;
  gcm::IncomingMessage message = CreateValidMessage();
  // Clear private topic.
  message.sender_id = std::string();

  std::unique_ptr<FCMNetworkHandler> handler =
      MakeHandlerReadyForMessage(mock_on_message_callback.Get());
  EXPECT_CALL(mock_on_message_callback, Run).Times(0);
  handler->OnMessage(kInvalidationsAppId, message);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("FCMInvalidations.FCMMessageStatus"),
      testing::ElementsAre(base::Bucket(
          static_cast<int>(
              InvalidationParsingStatus::kPrivateTopicEmpty) /* min */,
          1 /* count */)));
}

TEST_F(FCMNetworkHandlerTest, ShouldRequestTokenImmediatellyEvenIfSaved) {
  // Setting up network handler.
  MockOnTokenCallback mock_on_token_callback;
  auto handler = MakeHandler();
  auto task_runner = CreateFakeTaskRunnerAndInjectToHandler(handler);
  handler->SetTokenReceiver(mock_on_token_callback.Get());

  // Check that after StartListening we receive the token and store it.
  EXPECT_CALL(*mock_instance_id(), GetToken)
      .WillOnce(WithArg<4>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token", InstanceID::Result::SUCCESS);
      })));
  EXPECT_CALL(mock_on_token_callback, Run("token")).Times(1);
  handler->StartListening();
  handler->StopListening();
  task_runner->RunUntilIdle();

  // Setting up another network handler.
  auto handler2 = MakeHandler();
  auto task_runner2 = CreateFakeTaskRunnerAndInjectToHandler(handler2);
  handler2->SetTokenReceiver(mock_on_token_callback.Get());

  // Check that after StartListening the token will be requested, depite we have
  // saved token.
  EXPECT_CALL(*mock_instance_id(), GetToken)
      .WillOnce(WithArg<4>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token_new", InstanceID::Result::SUCCESS);
      })));
  EXPECT_CALL(mock_on_token_callback, Run("token_new")).Times(1);
  handler->StartListening();
  task_runner->RunUntilIdle();
}

TEST_F(FCMNetworkHandlerTest, ShouldScheduleTokenValidationAndActOnNewToken) {
  // Setting up network handler.
  MockOnTokenCallback mock_on_token_callback;
  auto handler = MakeHandler();
  auto task_runner = CreateFakeTaskRunnerAndInjectToHandler(handler);
  handler->SetTokenReceiver(mock_on_token_callback.Get());

  // Checking that after start listening the token will be requested
  // and passed to the appropriate token receiver.
  EXPECT_CALL(*mock_instance_id(), GetToken)
      .WillOnce(WithArg<4>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token", InstanceID::Result::SUCCESS);
      })));
  EXPECT_CALL(*mock_instance_id(), ValidateToken).Times(0);
  EXPECT_CALL(mock_on_token_callback, Run("token")).Times(1);
  handler->StartListening();
  testing::Mock::VerifyAndClearExpectations(mock_instance_id());

  // Adjust timers and check that validation will happen in time.
  // The old token was invalid, so token listener should be informed.
  const base::TimeDelta time_to_validation = base::Hours(24);
  task_runner->FastForwardBy(time_to_validation - base::Seconds(1));
  // But when it is time, validation happens.
  EXPECT_CALL(*mock_instance_id(), GetToken)
      .WillOnce(WithArg<4>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token_new", InstanceID::Result::SUCCESS);
      })));
  EXPECT_CALL(mock_on_token_callback, Run("token_new")).Times(1);
  task_runner->FastForwardBy(base::Seconds(1));
}

TEST_F(FCMNetworkHandlerTest,
       ShouldScheduleTokenValidationAndDoNotActOnSameToken) {
  // Setting up network handler.
  MockOnTokenCallback mock_on_token_callback;
  std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();
  auto task_runner = CreateFakeTaskRunnerAndInjectToHandler(handler);
  handler->SetTokenReceiver(mock_on_token_callback.Get());

  // Checking that after start listening the token will be requested
  // and passed to the appropriate token receiver
  EXPECT_CALL(*mock_instance_id(), GetToken)
      .WillOnce(WithArg<4>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token", InstanceID::Result::SUCCESS);
      })));
  EXPECT_CALL(*mock_instance_id(), ValidateToken).Times(0);
  EXPECT_CALL(mock_on_token_callback, Run("token")).Times(1);
  handler->StartListening();
  testing::Mock::VerifyAndClearExpectations(mock_instance_id());

  // Adjust timers and check that validation will happen in time.
  // The old token is valid, so no token listener should be informed.
  const base::TimeDelta time_to_validation = base::Hours(24);
  task_runner->FastForwardBy(time_to_validation - base::Seconds(1));

  // But when it is time, validation happens.
  EXPECT_CALL(*mock_instance_id(), GetToken)
      .WillOnce(WithArg<4>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token", InstanceID::Result::SUCCESS);
      })));
  EXPECT_CALL(mock_on_token_callback, Run).Times(0);
  task_runner->FastForwardBy(base::Seconds(1));
}

TEST_F(FCMNetworkHandlerTestWithTTL, ShouldProvideTTLWithPolicySenderID) {
  EXPECT_CALL(*mock_instance_id(),
              GetToken(_, _, Eq(base::Seconds(kTimeToLiveInSeconds)), _, _));
  MakeHandler(/*sender_id=*/"1013309121859")->StartListening();
}

TEST_F(FCMNetworkHandlerTestWithTTL, ShouldNotProvideTTLWithFakeSenderID) {
  EXPECT_CALL(*mock_instance_id(), GetToken(_, _, Eq(base::TimeDelta()), _, _));
  MakeHandler(/*sender_id=*/"fake_sender_id")->StartListening();
}

}  // namespace invalidation
