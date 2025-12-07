// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/boca/invalidations/fcm_handler.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_delegate.h"
#include "components/account_id/account_id.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace ash::boca {
namespace {

constexpr int kTokenValidationPeriodMinutesDefault = 60 * 24;

class MockDelegate : public InvalidationServiceDelegate {
 public:
  explicit MockDelegate() = default;
  ~MockDelegate() override = default;
  MOCK_METHOD(void,
              UploadToken,
              (const std::string&, base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              OnInvalidationReceived,
              (const std::string& payload),
              (override));
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
      : instance_id::InstanceID(/*app_id=*/"",
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
    const std::string kDefaultFcmToken = "default_token";
    mock_instance_id_driver_ =
        std::make_unique<NiceMock<MockInstanceIDDriver>>();
    ON_CALL(*mock_instance_id_driver_, GetInstanceID)
        .WillByDefault(Return(&mock_instance_id_));

    ON_CALL(mock_instance_id_, GetToken)
        .WillByDefault(RunOnceCallback<4>(
            kDefaultFcmToken, instance_id::InstanceID::Result::SUCCESS));

    ON_CALL(mock_delegate_, UploadToken(kDefaultFcmToken, _))
        .WillByDefault(Return());

    invalidation_service_impl_ = std::make_unique<InvalidationServiceImpl>(
        &fake_gcm_driver_, mock_instance_id_driver_.get(), &mock_delegate_);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<MockInstanceID> mock_instance_id_;
  gcm::FakeGCMDriver fake_gcm_driver_;
  std::unique_ptr<NiceMock<MockInstanceIDDriver>> mock_instance_id_driver_;
  NiceMock<MockDelegate> mock_delegate_;
  std::unique_ptr<InvalidationServiceImpl> invalidation_service_impl_;
};

TEST_F(InvalidationServiceImplTest, HandleInvalidation) {
  const std::string kPayloadValue = "payload_1";
  EXPECT_CALL(mock_delegate_, OnInvalidationReceived(kPayloadValue)).Times(1);
  gcm::IncomingMessage gcm_message;
  gcm_message.data.emplace("method", kPayloadValue);
  invalidation_service_impl_->fcm_handler()->OnMessage(
      invalidation_service_impl_->fcm_handler()->GetAppIdForTesting(),
      gcm_message);
}

TEST_F(InvalidationServiceImplTest, HandleTokenUpload) {
  // Check that the handler gets the token through GetToken.
  const std::string kNewToken = "token_2";
  EXPECT_CALL(mock_instance_id_, GetToken)
      .WillOnce(RunOnceCallback<4>(kNewToken,
                                   instance_id::InstanceID::Result::SUCCESS));
  EXPECT_CALL(mock_delegate_, UploadToken(kNewToken, _)).Times(1);
  // Adjust the time and check that validation will happen in time.
  // The old token is invalid, so token observer should be informed.
  task_environment_.FastForwardBy(
      base::Minutes(kTokenValidationPeriodMinutesDefault));
}

TEST_F(InvalidationServiceImplTest, HandleTokenUploadFailureWithBackoff) {
  // Check that the handler gets the token through GetToken.
  const std::string kNewToken = "token_2";
  EXPECT_CALL(mock_instance_id_, GetToken)
      .WillOnce(RunOnceCallback<4>(kNewToken,
                                   instance_id::InstanceID::Result::SUCCESS));
  EXPECT_CALL(mock_delegate_, UploadToken(kNewToken, _))
      .WillOnce([&](const std::string&,
                    base::OnceCallback<void(bool)> on_token_uploaded_cb) {
        std::move(on_token_uploaded_cb).Run(/*success=*/false);
      });
  // Adjust the time and check that validation will happen in time.
  // The old token is invalid, so token observer should be informed.
  task_environment_.FastForwardBy(
      base::Minutes(kTokenValidationPeriodMinutesDefault));

  // A retry and succeeded and stop uploading.
  EXPECT_CALL(mock_delegate_, UploadToken(kNewToken, _)).Times(1);
  task_environment_.FastForwardBy(base::Seconds(3));
}
}  // namespace
}  // namespace ash::boca
