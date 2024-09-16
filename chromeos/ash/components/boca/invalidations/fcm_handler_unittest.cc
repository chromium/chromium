// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/invalidations/fcm_handler.h"

#include <set>
#include <string>
#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;
using instance_id::InstanceID;
using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::WithArg;

namespace ash::boca {
namespace {

const char kDefaultSenderId[] = "fake_sender_id";
const char kInvalidationsAppId[] = "com.google.chrome.boca.invalidations";

const int kTokenValidationPeriodMinutesDefault = 60 * 24;

class MockInstanceID : public InstanceID {
 public:
  MockInstanceID() : InstanceID("app_id", /*gcm_driver=*/nullptr) {}
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
  MOCK_METHOD(InstanceID*,
              GetInstanceID,
              (const std::string& app_id),
              (override));
  MOCK_METHOD(void, RemoveInstanceID, (const std::string& app_id), (override));
  MOCK_METHOD(bool,
              ExistsInstanceID,
              (const std::string& app_id),
              (const override));
};

class MockListener : public InvalidationsListener {
 public:
  MOCK_METHOD(void,
              OnInvalidationReceived,
              (const std::string& payload),
              (override));
};

class MockTokenObserver : public FCMRegistrationTokenObserver {
 public:
  MOCK_METHOD(void, OnFCMRegistrationTokenChanged, (), (override));
};

class FCMHandlerTest : public testing::Test {
 public:
  FCMHandlerTest()
      : fcm_handler_(&fake_gcm_driver_,
                     &mock_instance_id_driver_,
                     kDefaultSenderId,
                     kInvalidationsAppId) {
    // This is called in the FCMHandler.
    ON_CALL(mock_instance_id_driver_, GetInstanceID(kInvalidationsAppId))
        .WillByDefault(Return(&mock_instance_id_));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};

  gcm::FakeGCMDriver fake_gcm_driver_;
  NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  NiceMock<MockInstanceID> mock_instance_id_;

  FCMHandler fcm_handler_;
};

TEST_F(FCMHandlerTest, ShouldReturnValidToken) {
  // Check that the handler gets the token through GetToken.
  EXPECT_CALL(mock_instance_id_, GetToken)
      .WillOnce(RunOnceCallback<4>("token", InstanceID::Result::SUCCESS));

  fcm_handler_.StartListening();

  EXPECT_EQ("token", fcm_handler_.GetFCMRegistrationToken());
}

TEST_F(FCMHandlerTest, ShouldPropagatePayloadToListener) {
  const std::string kPayloadValue = "some_payload";
  NiceMock<MockListener> mock_listener;
  fcm_handler_.AddListener(&mock_listener);

  gcm::IncomingMessage gcm_message;
  gcm_message.raw_data = kPayloadValue;

  EXPECT_CALL(mock_listener, OnInvalidationReceived(kPayloadValue));
  fcm_handler_.OnMessage(kInvalidationsAppId, gcm_message);
  fcm_handler_.RemoveListener(&mock_listener);
}

TEST_F(FCMHandlerTest, ShouldNotifyOnTokenChange) {
  NiceMock<MockTokenObserver> mock_token_observer;
  fcm_handler_.AddTokenObserver(&mock_token_observer);

  // Check that the handler gets the token through GetToken.
  ON_CALL(mock_instance_id_, GetToken)
      .WillByDefault(
          RunOnceCallbackRepeatedly<4>("token", InstanceID::Result::SUCCESS));

  EXPECT_CALL(mock_token_observer, OnFCMRegistrationTokenChanged());
  fcm_handler_.StartListening();

  fcm_handler_.RemoveTokenObserver(&mock_token_observer);
}

TEST_F(FCMHandlerTest, ShouldScheduleTokenValidationAndActOnNewToken) {
  NiceMock<MockTokenObserver> mock_token_observer;
  fcm_handler_.AddTokenObserver(&mock_token_observer);

  // Check that the handler gets the token through GetToken and notifies the
  // observer.
  EXPECT_CALL(mock_instance_id_, GetToken)
      .WillOnce(RunOnceCallback<4>("token", InstanceID::Result::SUCCESS));
  EXPECT_CALL(mock_token_observer, OnFCMRegistrationTokenChanged()).Times(1);
  fcm_handler_.StartListening();

  // Adjust the time and check that validation will happen in time.
  // The old token is invalid, so token observer should be informed.
  task_environment_.FastForwardBy(
      base::Minutes(kTokenValidationPeriodMinutesDefault) - base::Seconds(1));
  // When it is time, validation happens.
  EXPECT_CALL(mock_instance_id_, GetToken)
      .WillOnce(RunOnceCallback<4>("new token", InstanceID::Result::SUCCESS));
  EXPECT_CALL(mock_token_observer, OnFCMRegistrationTokenChanged()).Times(1);
  task_environment_.FastForwardBy(base::Seconds(1));

  fcm_handler_.RemoveTokenObserver(&mock_token_observer);
}

TEST_F(FCMHandlerTest, ShouldScheduleTokenValidationAndNotActOnSameToken) {
  NiceMock<MockTokenObserver> mock_token_observer;
  fcm_handler_.AddTokenObserver(&mock_token_observer);

  // Check that the handler gets the token through GetToken and notifies the
  // observer.
  EXPECT_CALL(mock_instance_id_, GetToken)
      .WillOnce(RunOnceCallback<4>("token", InstanceID::Result::SUCCESS));
  EXPECT_CALL(mock_token_observer, OnFCMRegistrationTokenChanged()).Times(1);
  fcm_handler_.StartListening();

  // Adjust the time and check that validation will happen in time.
  // The old token is valid, so token observer should not be informed.
  task_environment_.FastForwardBy(
      base::Minutes(kTokenValidationPeriodMinutesDefault) - base::Seconds(1));
  // When it is time, validation happens.
  EXPECT_CALL(mock_instance_id_, GetToken)
      .WillOnce(RunOnceCallback<4>("token", InstanceID::Result::SUCCESS));
  EXPECT_CALL(mock_token_observer, OnFCMRegistrationTokenChanged()).Times(0);
  task_environment_.FastForwardBy(base::Seconds(1));

  fcm_handler_.RemoveTokenObserver(&mock_token_observer);
}

TEST_F(FCMHandlerTest, ShouldClearTokenOnStopListeningPermanently) {
  // Check that the handler gets the token through GetToken.
  EXPECT_CALL(mock_instance_id_, GetToken)
      .WillOnce(RunOnceCallback<4>("token", InstanceID::Result::SUCCESS));
  fcm_handler_.StartListening();

  NiceMock<MockTokenObserver> mock_token_observer;
  fcm_handler_.AddTokenObserver(&mock_token_observer);

  EXPECT_CALL(mock_instance_id_driver_, ExistsInstanceID(kInvalidationsAppId))
      .WillOnce(Return(true));
  // Token should be cleared when StopListeningPermanently() is called.
  EXPECT_CALL(mock_token_observer, OnFCMRegistrationTokenChanged());
  fcm_handler_.StopListeningPermanently();
  EXPECT_EQ(std::nullopt, fcm_handler_.GetFCMRegistrationToken());

  fcm_handler_.RemoveTokenObserver(&mock_token_observer);
}

}  // namespace
}  // namespace ash::boca
