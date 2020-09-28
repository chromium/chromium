// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/fcm_handler.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/sync/invalidations/fcm_registration_token_observer.h"
#include "components/sync/invalidations/invalidations_listener.h"
#include "components/sync/invalidations/switches.h"
#include "google_apis/gcm/engine/account_mapping.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using instance_id::InstanceID;
using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::WithArg;

namespace syncer {
namespace {

const char kDefaultSenderId[] = "fake_sender_id";
const char kSyncInvalidationsAppId[] = "com.google.chrome.sync.invalidations";
const char kPayloadKey[] = "payload";

const int kTokenValidationPeriodMinutesDefault = 60 * 24;

class MockInstanceID : public InstanceID {
 public:
  MockInstanceID() : InstanceID("app_id", /*gcm_driver=*/nullptr) {}
  ~MockInstanceID() override = default;

  MOCK_METHOD1(GetID, void(GetIDCallback callback));
  MOCK_METHOD1(GetCreationTime, void(GetCreationTimeCallback callback));
  MOCK_METHOD6(GetToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    base::TimeDelta time_to_live,
                    const std::map<std::string, std::string>& options,
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

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

  MOCK_METHOD1(GetInstanceID, InstanceID*(const std::string& app_id));
  MOCK_METHOD1(RemoveInstanceID, void(const std::string& app_id));
  MOCK_CONST_METHOD1(ExistsInstanceID, bool(const std::string& app_id));
};

class MockListener : public InvalidationsListener {
 public:
  MOCK_METHOD1(OnInvalidationReceived, void(const std::string& payload));
};

class MockTokenObserver : public FCMRegistrationTokenObserver {
 public:
  MOCK_METHOD0(OnFCMRegistrationTokenChanged, void());
};

class FCMHandlerTest : public testing::Test {
 public:
  FCMHandlerTest()
      : fcm_handler_(&fake_gcm_driver_,
                     &mock_instance_id_driver_,
                     kDefaultSenderId,
                     kSyncInvalidationsAppId) {
    // This is called in the FCMHandler.
    ON_CALL(mock_instance_id_driver_, GetInstanceID(kSyncInvalidationsAppId))
        .WillByDefault(Return(&mock_instance_id_));
    override_features_.InitWithFeatures(
        /*enabled_features=*/{switches::kSyncSendInterestedDataTypes,
                              switches::kUseSyncInvalidations},
        /*disabled_features=*/{});
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList override_features_;

  gcm::FakeGCMDriver fake_gcm_driver_;
  NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  NiceMock<MockInstanceID> mock_instance_id_;

  FCMHandler fcm_handler_;
};

TEST_F(FCMHandlerTest, ShouldReturnValidToken) {
  // Check that the handler gets the token through GetToken.
  EXPECT_CALL(mock_instance_id_, GetToken(_, _, _, _, _, _))
      .WillOnce(WithArg<5>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token", InstanceID::Result::SUCCESS);
      })));

  fcm_handler_.StartListening();

  EXPECT_EQ("token", fcm_handler_.GetFCMRegistrationToken());
}

TEST_F(FCMHandlerTest, ShouldPropagatePayloadToListener) {
  const std::string kPayloadValue = "some_payload";
  NiceMock<MockListener> mock_listener;
  fcm_handler_.AddListener(&mock_listener);

  gcm::IncomingMessage gcm_message;
  gcm_message.data[kPayloadKey] = kPayloadValue;

  EXPECT_CALL(mock_listener, OnInvalidationReceived(kPayloadValue));
  fcm_handler_.OnMessage(kSyncInvalidationsAppId, gcm_message);
  fcm_handler_.RemoveListener(&mock_listener);
}

TEST_F(FCMHandlerTest, ShouldNotifyOnTokenChange) {
  NiceMock<MockTokenObserver> mock_token_observer;
  fcm_handler_.AddTokenObserver(&mock_token_observer);

  // Check that the handler gets the token through GetToken.
  ON_CALL(mock_instance_id_, GetToken(_, _, _, _, _, _))
      .WillByDefault(
          WithArg<5>(Invoke([](InstanceID::GetTokenCallback callback) {
            std::move(callback).Run("token", InstanceID::Result::SUCCESS);
          })));

  EXPECT_CALL(mock_token_observer, OnFCMRegistrationTokenChanged());
  fcm_handler_.StartListening();

  fcm_handler_.RemoveTokenObserver(&mock_token_observer);
}

TEST_F(FCMHandlerTest, ShouldScheduleTokenValidationAndActOnNewToken) {
  NiceMock<MockTokenObserver> mock_token_observer;
  fcm_handler_.AddTokenObserver(&mock_token_observer);

  // Check that the handler gets the token through GetToken and notifies the
  // observer.
  EXPECT_CALL(mock_instance_id_, GetToken(_, _, _, _, _, _))
      .WillOnce(WithArg<5>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token", InstanceID::Result::SUCCESS);
      })));
  EXPECT_CALL(mock_token_observer, OnFCMRegistrationTokenChanged()).Times(1);
  fcm_handler_.StartListening();

  // Adjust the time and check that validation will happen in time.
  // The old token is invalid, so token observer should be informed.
  task_environment_.FastForwardBy(
      base::TimeDelta::FromMinutes(kTokenValidationPeriodMinutesDefault) -
      base::TimeDelta::FromSeconds(1));
  // When it is time, validation happens.
  EXPECT_CALL(mock_instance_id_, GetToken(_, _, _, _, _, _))
      .WillOnce(WithArg<5>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("new token", InstanceID::Result::SUCCESS);
      })));
  EXPECT_CALL(mock_token_observer, OnFCMRegistrationTokenChanged()).Times(1);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  fcm_handler_.RemoveTokenObserver(&mock_token_observer);
}

TEST_F(FCMHandlerTest, ShouldScheduleTokenValidationAndNotActOnSameToken) {
  NiceMock<MockTokenObserver> mock_token_observer;
  fcm_handler_.AddTokenObserver(&mock_token_observer);

  // Check that the handler gets the token through GetToken and notifies the
  // observer.
  EXPECT_CALL(mock_instance_id_, GetToken(_, _, _, _, _, _))
      .WillOnce(WithArg<5>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token", InstanceID::Result::SUCCESS);
      })));
  EXPECT_CALL(mock_token_observer, OnFCMRegistrationTokenChanged()).Times(1);
  fcm_handler_.StartListening();

  // Adjust the time and check that validation will happen in time.
  // The old token is valid, so token observer should not be informed.
  task_environment_.FastForwardBy(
      base::TimeDelta::FromMinutes(kTokenValidationPeriodMinutesDefault) -
      base::TimeDelta::FromSeconds(1));
  // When it is time, validation happens.
  EXPECT_CALL(mock_instance_id_, GetToken(_, _, _, _, _, _))
      .WillOnce(WithArg<5>(Invoke([](InstanceID::GetTokenCallback callback) {
        std::move(callback).Run("token", InstanceID::Result::SUCCESS);
      })));
  EXPECT_CALL(mock_token_observer, OnFCMRegistrationTokenChanged()).Times(0);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  fcm_handler_.RemoveTokenObserver(&mock_token_observer);
}

}  // namespace
}  // namespace syncer
