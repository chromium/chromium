// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/invalidations/fcm_handler.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/boca/session_api/upload_token_request.h"
#include "components/account_id/account_id.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "google_apis/common/request_sender.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace ash::boca {

namespace {
constexpr char kTestEmail[] = "testemail";
constexpr char kGaiaId[] = "123";
constexpr int kTokenValidationPeriodMinutesDefault = 60 * 24;

class MockSessionClientImpl : public SessionClientImpl {
 public:
  explicit MockSessionClientImpl(
      std::unique_ptr<google_apis::RequestSender> sender)
      : SessionClientImpl(std::move(sender)) {}
  MOCK_METHOD(void,
              UploadToken,
              (std::unique_ptr<UploadTokenRequest>),
              (override));
};

class MockSessionManager : public BocaSessionManager {
 public:
  explicit MockSessionManager(SessionClientImpl* session_client_impl)
      : BocaSessionManager(
            session_client_impl,
            AccountId::FromUserEmailGaiaId(kTestEmail, kGaiaId)) {}
  MOCK_METHOD(void, LoadCurrentSession, (), (override));
  ~MockSessionManager() override = default;
};

class MockGCMDriver : public gcm::GCMDriver {
 public:
  MockGCMDriver(base::FilePath& path,
                scoped_refptr<base::SequencedTaskRunner>& sequence_runner)
      : GCMDriver(/*store_path=*/base::FilePath(),
                  /*blocking_task_runner=*/nullptr) {}
};

class MockInstanceID : public instance_id::InstanceID {
 public:
  MockInstanceID()
      : instance_id::InstanceID(InvalidationServiceImpl::kApplicationId,
                                /*gcm_driver=*/nullptr) {}
  ~MockInstanceID() override = default;
  MOCK_METHOD(void, GetID, (GetIDCallback callback), (override));
  MOCK_METHOD(void,
              GetCreationTime,
              (GetCreationTimeCallback callback),
              (override));
  MOCK_METHOD(void,
              GetToken,
              (const std::string& authorized_entity,
               const std::string& scope,
               base::TimeDelta time_to_live,
               std::set<Flags> flags,
               GetTokenCallback callback),
              (override));
  MOCK_METHOD(void,
              ValidateToken,
              (const std::string& authorized_entity,
               const std::string& scope,
               const std::string& token,
               ValidateTokenCallback callback),
              (override));

 protected:
  MOCK_METHOD(void,
              DeleteTokenImpl,
              (const std::string& authorized_entity,
               const std::string& scope,
               DeleteTokenCallback callback),
              (override));
  MOCK_METHOD(void, DeleteIDImpl, (DeleteIDCallback callback), (override));
};

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;
  MOCK_METHOD(instance_id::InstanceID*,
              GetInstanceID,
              (const std::string& app_id),
              (override));
  MOCK_METHOD(void, RemoveInstanceID, (const std::string& app_id), (override));
  MOCK_METHOD(bool,
              ExistsInstanceID,
              (const std::string& app_id),
              (const override));
};

class InvalidationServiceImplTest : public testing::Test {
 protected:
  InvalidationServiceImplTest() = default;
  ~InvalidationServiceImplTest() override = default;

  void SetUp() override {
    mock_instance_id_driver_ =
        std::make_unique<NiceMock<MockInstanceIDDriver>>();
    ON_CALL(*mock_instance_id_driver_,
            GetInstanceID(InvalidationServiceImpl::kApplicationId))
        .WillByDefault(Return(&mock_instance_id_));

    ON_CALL(mock_instance_id_, GetToken)
        .WillByDefault(RunOnceCallback<4>(
            "default_token", instance_id::InstanceID::Result::SUCCESS));

    session_client_impl_ =
        std::make_unique<NiceMock<MockSessionClientImpl>>(nullptr);
    ON_CALL(*session_client_impl_, UploadToken(_)).WillByDefault(Return());

    boca_session_manager_ = std::make_unique<NiceMock<MockSessionManager>>(
        session_client_impl_.get());
    invalidation_service_impl_ = std::make_unique<InvalidationServiceImpl>(
        &fake_gcm_driver_, mock_instance_id_driver_.get(),
        AccountId::FromUserEmailGaiaId(kTestEmail, kGaiaId),
        boca_session_manager_.get(), session_client_impl_.get());
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<MockInstanceID> mock_instance_id_;
  gcm::FakeGCMDriver fake_gcm_driver_;
  std::unique_ptr<NiceMock<MockInstanceIDDriver>> mock_instance_id_driver_;
  std::unique_ptr<NiceMock<MockSessionClientImpl>> session_client_impl_;
  std::unique_ptr<NiceMock<MockSessionManager>> boca_session_manager_;
  std::unique_ptr<InvalidationServiceImpl> invalidation_service_impl_;
};

TEST_F(InvalidationServiceImplTest, HandleInvalidation) {
  EXPECT_CALL(*boca_session_manager_, LoadCurrentSession()).Times(1);
  const std::string kPayloadValue = "payload_1";
  gcm::IncomingMessage gcm_message;
  gcm_message.raw_data = kPayloadValue;
  invalidation_service_impl_->fcm_handler()->OnMessage(
      InvalidationServiceImpl::kApplicationId, gcm_message);
}

TEST_F(InvalidationServiceImplTest, HandleTokenUpload) {
  // Check that the handler gets the token through GetToken.
  const char token[] = "token_2";
  EXPECT_CALL(mock_instance_id_, GetToken)
      .WillOnce(
          RunOnceCallback<4>(token, instance_id::InstanceID::Result::SUCCESS));
  std::unique_ptr<UploadTokenRequest> request;
  EXPECT_CALL(*session_client_impl_, UploadToken(_))
      .WillOnce([&](std::unique_ptr<UploadTokenRequest> request_1) {
        request = std::move(request_1);
      });
  // Adjust the time and check that validation will happen in time.
  // The old token is invalid, so token observer should be informed.
  task_environment_.FastForwardBy(
      base::Minutes(kTokenValidationPeriodMinutesDefault));

  EXPECT_EQ(kGaiaId, request->gaia_id());
  EXPECT_EQ(token, request->token());
}
}  // namespace
}  // namespace ash::boca
