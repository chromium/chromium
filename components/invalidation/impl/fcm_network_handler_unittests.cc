// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_network_handler.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/status.h"
#include "google_apis/gcm/engine/account_mapping.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::TestMockTimeTaskRunner;
using gcm::InstanceIDHandler;
using instance_id::InstanceID;
using instance_id::InstanceIDDriver;
using testing::_;
using testing::StrictMock;

namespace syncer {

namespace {

const char kInvalidationsAppId[] = "com.google.chrome.fcm.invalidations";
using TokenCallback = base::RepeatingCallback<void(const std::string& message)>;
using MessageCallback = base::RepeatingCallback<void(const std::string&,
                                                     const std::string&,
                                                     const std::string&,
                                                     const std::string&)>;

base::Time GetDummyNow() {
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromUTCString("2017-01-02T00:00:01Z", &out_time));
  return out_time;
}

gcm::IncomingMessage CreateValidMessage() {
  gcm::IncomingMessage message;
  message.data["payload"] = "payload";
  message.data["version"] = "version";
  message.sender_id = "private_topic";
  return message;
}

class MockInstanceID : public InstanceID {
 public:
  MockInstanceID() : InstanceID("app_id", /*gcm_driver=*/nullptr) {}
  ~MockInstanceID() override = default;

  MOCK_METHOD1(GetID, void(const GetIDCallback& callback));
  MOCK_METHOD1(GetCreationTime, void(const GetCreationTimeCallback& callback));
  void GetToken(const std::string& authorized_entity,
                const std::string& scope,
                const std::map<std::string, std::string>& options,
                std::set<Flags> flags,
                GetTokenCallback callback) override {
    GetToken_(authorized_entity, scope, options, std::move(flags), callback);
  }
  MOCK_METHOD5(GetToken_,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    const std::map<std::string, std::string>& options,
                    std::set<Flags> flags,
                    GetTokenCallback& callback));
  MOCK_METHOD4(ValidateToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    const std::string& token,
                    const ValidateTokenCallback& callback));

 protected:
  void DeleteTokenImpl(const std::string& authorized_entity,
                       const std::string& scope,
                       DeleteTokenCallback callback) override {
    DeleteTokenImpl_(authorized_entity, scope, callback);
  }
  MOCK_METHOD3(DeleteTokenImpl_,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    DeleteTokenCallback& callback));
  void DeleteIDImpl(DeleteIDCallback callback) override {
    DeleteIDImpl_(callback);
  }
  MOCK_METHOD1(DeleteIDImpl_, void(DeleteIDCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceID);
};

class MockGCMDriver : public gcm::GCMDriver {
 public:
  MockGCMDriver()
      : GCMDriver(
            /*store_path=*/base::FilePath(),
            /*blocking_task_runner=*/nullptr,
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}
  ~MockGCMDriver() override = default;

  MOCK_METHOD4(ValidateRegistration,
               void(const std::string& app_id,
                    const std::vector<std::string>& sender_ids,
                    const std::string& registration_id,
                    const ValidateRegistrationCallback& callback));
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
               void(const GetGCMStatisticsCallback& callback,
                    ClearActivityLogs clear_logs));
  MOCK_METHOD2(SetGCMRecording,
               void(const GetGCMStatisticsCallback& callback, bool recording));
  MOCK_METHOD1(SetAccountTokens,
               void(const std::vector<gcm::GCMClient::AccountTokenInfo>&
                        account_tokens));
  MOCK_METHOD1(UpdateAccountMapping,
               void(const gcm::AccountMapping& account_mapping));
  MOCK_METHOD1(RemoveAccountMapping, void(const CoreAccountId& account_id));
  MOCK_METHOD0(GetLastTokenFetchTime, base::Time());
  MOCK_METHOD1(SetLastTokenFetchTime, void(const base::Time& time));
  MOCK_METHOD1(WakeFromSuspendForHeartbeat, void(bool wake));
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

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockGCMDriver);
};

class MockInstanceIDDriver : public InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

  MOCK_METHOD1(GetInstanceID, InstanceID*(const std::string& app_id));
  MOCK_METHOD1(RemoveInstanceID, void(const std::string& app_id));
  MOCK_CONST_METHOD1(ExistsInstanceID, bool(const std::string& app_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceIDDriver);
};

class MockOnTokenCallback {
 public:
  // Workaround for gMock's lack of support for movable-only arguments.
  void WrappedRun(const std::string& token) { Run(token); }

  TokenCallback Get() {
    return base::BindRepeating(&MockOnTokenCallback::WrappedRun,
                               base::Unretained(this));
  }

  MOCK_METHOD1(Run, void(const std::string&));
};

class MockOnMessageCallback {
 public:
  // Workaround for gMock's lack of support for movable-only arguments.
  void WrappedRun(const std::string& payload,
                  const std::string& private_topic_name,
                  const std::string& public_topic_name,
                  const std::string& version) {
    Run(payload, private_topic_name, public_topic_name, version);
  }

  MessageCallback Get() {
    return base::BindRepeating(&MockOnMessageCallback::WrappedRun,
                               base::Unretained(this));
  }

  MOCK_METHOD4(Run,
               void(const std::string&,
                    const std::string&,
                    const std::string&,
                    const std::string&));
};

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  std::move(std::get<k>(args)).Run(p0);
}

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(p0, p1)) {
  std::move(std::get<k>(args)).Run(p0, p1);
}

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

  std::unique_ptr<FCMNetworkHandler> MakeHandler() {
    return std::make_unique<FCMNetworkHandler>(
        mock_gcm_driver_.get(), mock_instance_id_driver_.get(),
        "fake_sender_id", kInvalidationsAppId);
  }

  std::unique_ptr<FCMNetworkHandler> MakeHandlerReadyForMessage(
      MessageCallback mock_on_message_callback) {
    std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();
    handler->SetMessageReceiver(mock_on_message_callback);
    EXPECT_CALL(*mock_instance_id(), GetToken_(_, _, _, _, _))
        .WillOnce(
            InvokeCallbackArgument<4>("token", InstanceID::Result::SUCCESS));
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

TEST_F(FCMNetworkHandlerTest, ShouldPassTheTokenOnceRecieved) {
  std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();

  MockOnTokenCallback mock_on_token_callback;
  handler->SetTokenReceiver(mock_on_token_callback.Get());

  // Check that the handler gets the token through GetToken.
  EXPECT_CALL(*mock_instance_id(), GetToken_(_, _, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<4>("token", InstanceID::Result::SUCCESS));
  EXPECT_CALL(mock_on_token_callback, Run("token")).Times(1);
  handler->StartListening();
}

TEST_F(FCMNetworkHandlerTest, ShouldPassTheTokenOnceSubscribed) {
  std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();

  MockOnTokenCallback mock_on_token_callback;

  // Check that the handler gets the token through GetToken.
  EXPECT_CALL(*mock_instance_id(), GetToken_(_, _, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<4>("token", InstanceID::Result::SUCCESS));
  EXPECT_CALL(mock_on_token_callback, Run(_)).Times(0);
  handler->StartListening();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(mock_on_token_callback, Run("token")).Times(1);
  handler->SetTokenReceiver(mock_on_token_callback.Get());
}

TEST_F(FCMNetworkHandlerTest, ShouldNotInvokeMessageCallbackOnEmptyMessage) {
  MockOnMessageCallback mock_on_message_callback;
  gcm::IncomingMessage message;

  std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();
  EXPECT_CALL(mock_on_message_callback, Run(_, _, _, _)).Times(0);
  handler->SetMessageReceiver(mock_on_message_callback.Get());
  EXPECT_CALL(*mock_instance_id(), GetToken_(_, _, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<4>("token", InstanceID::Result::SUCCESS));

  handler->StartListening();
  handler->OnMessage(kInvalidationsAppId, gcm::IncomingMessage());
}

TEST_F(FCMNetworkHandlerTest, ShouldInvokeMessageCallbackOnValidMessage) {
  base::HistogramTester histogram_tester;
  MockOnMessageCallback mock_on_message_callback;
  gcm::IncomingMessage message = CreateValidMessage();

  std::unique_ptr<FCMNetworkHandler> handler =
      MakeHandlerReadyForMessage(mock_on_message_callback.Get());
  EXPECT_CALL(mock_on_message_callback,
              Run("payload", "private_topic", "", "version"))
      .Times(1);
  EXPECT_CALL(mock_on_message_callback,
              Run("payload", "private_topic", "public_topic", "version"))
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
  EXPECT_CALL(mock_on_message_callback, Run(_, _, _, _)).Times(0);
  handler->OnMessage(kInvalidationsAppId, message);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("FCMInvalidations.FCMMessageStatus"),
      testing::ElementsAre(base::Bucket(
          static_cast<int>(InvalidationParsingStatus::kVersionEmpty) /* min */,
          1 /* count */)));
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
  EXPECT_CALL(mock_on_message_callback, Run(_, _, _, _)).Times(0);
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
  EXPECT_CALL(*mock_instance_id(), GetToken_(_, _, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<4>("token", InstanceID::Result::SUCCESS));
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
  EXPECT_CALL(*mock_instance_id(), GetToken_(_, _, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<4>("token_new", InstanceID::Result::SUCCESS));
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
  EXPECT_CALL(*mock_instance_id(), GetToken_(_, _, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<4>("token", InstanceID::Result::SUCCESS));
  EXPECT_CALL(*mock_instance_id(), ValidateToken(_, _, _, _)).Times(0);
  EXPECT_CALL(mock_on_token_callback, Run("token")).Times(1);
  handler->StartListening();
  testing::Mock::VerifyAndClearExpectations(mock_instance_id());

  // Adjust timers and check that validation will happen in time.
  // The old token was invalid, so token listener shold be informed.
  const base::TimeDelta time_to_validation = base::TimeDelta::FromHours(24);
  task_runner->FastForwardBy(time_to_validation -
                             base::TimeDelta::FromSeconds(1));
  // But when it is time, validation happens.
  EXPECT_CALL(*mock_instance_id(), GetToken_(_, _, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<4>("token_new", InstanceID::Result::SUCCESS));
  EXPECT_CALL(mock_on_token_callback, Run("token_new")).Times(1);
  task_runner->FastForwardBy(base::TimeDelta::FromSeconds(1));
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
  EXPECT_CALL(*mock_instance_id(), GetToken_(_, _, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<4>("token", InstanceID::Result::SUCCESS));
  EXPECT_CALL(*mock_instance_id(), ValidateToken(_, _, _, _)).Times(0);
  EXPECT_CALL(mock_on_token_callback, Run("token")).Times(1);
  handler->StartListening();
  testing::Mock::VerifyAndClearExpectations(mock_instance_id());

  // Adjust timers and check that validation will happen in time.
  // The old token is valid, so no token listener shold be informed.
  const base::TimeDelta time_to_validation = base::TimeDelta::FromHours(24);
  task_runner->FastForwardBy(time_to_validation -
                             base::TimeDelta::FromSeconds(1));

  // But when it is time, validation happens.
  EXPECT_CALL(*mock_instance_id(), GetToken_(_, _, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<4>("token", InstanceID::Result::SUCCESS));
  EXPECT_CALL(mock_on_token_callback, Run(_)).Times(0);
  task_runner->FastForwardBy(base::TimeDelta::FromSeconds(1));
}

}  // namespace syncer
