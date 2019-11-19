// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_gcm_manager_impl.h"

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

namespace chromeos {

namespace device_sync {

namespace {

const char kCryptAuthGCMAppId[] = "com.google.chrome.cryptauth";
const char kCryptAuthGCMSenderId[] = "381449029288";
const char kExistingGCMRegistrationId[] = "cirrus";
const char kNewGCMRegistrationId[] = "stratus";
const char kCryptAuthMessageCollapseKey[] =
    "collapse_cryptauth_sync_DEVICES_SYNC";
const char kSessionId1[] = "session_id_1";
const char kSessionId2[] = "session_id_2";
const CryptAuthFeatureType kFeatureType1 =
    CryptAuthFeatureType::kBetterTogetherHostEnabled;
const CryptAuthFeatureType kFeatureType2 =
    CryptAuthFeatureType::kEasyUnlockHostEnabled;

// Mock GCMDriver implementation for testing.
class MockGCMDriver : public gcm::FakeGCMDriver {
 public:
  MockGCMDriver() {}
  ~MockGCMDriver() override {}

  MOCK_METHOD2(AddAppHandler,
               void(const std::string& app_id, gcm::GCMAppHandler* handler));

  MOCK_METHOD2(RegisterImpl,
               void(const std::string& app_id,
                    const std::vector<std::string>& sender_ids));

  using gcm::GCMDriver::RegisterFinished;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGCMDriver);
};

}  // namespace

class DeviceSyncCryptAuthGCMManagerImplTest
    : public testing::Test,
      public CryptAuthGCMManager::Observer {
 protected:
  DeviceSyncCryptAuthGCMManagerImplTest()
      : gcm_manager_(&gcm_driver_, &pref_service_) {}

  // testing::Test:
  void SetUp() override {
    CryptAuthGCMManager::RegisterPrefs(pref_service_.registry());
    gcm_manager_.AddObserver(this);
    EXPECT_CALL(gcm_driver_, AddAppHandler(kCryptAuthGCMAppId, &gcm_manager_));
    gcm_manager_.StartListening();
  }

  void TearDown() override { gcm_manager_.RemoveObserver(this); }

  void RegisterWithGCM(gcm::GCMClient::Result registration_result) {
    std::vector<std::string> sender_ids;
    EXPECT_CALL(gcm_driver_, RegisterImpl(kCryptAuthGCMAppId, _))
        .WillOnce(SaveArg<1>(&sender_ids));
    gcm_manager_.RegisterWithGCM();

    ASSERT_EQ(1u, sender_ids.size());
    EXPECT_EQ(kCryptAuthGCMSenderId, sender_ids[0]);

    bool success = (registration_result == gcm::GCMClient::SUCCESS);
    EXPECT_CALL(*this, OnGCMRegistrationResultProxy(success));
    gcm_driver_.RegisterFinished(kCryptAuthGCMAppId, kNewGCMRegistrationId,
                                 registration_result);
  }

  // CryptAuthGCMManager::Observer:
  void OnGCMRegistrationResult(bool success) override {
    OnGCMRegistrationResultProxy(success);
  }

  void OnReenrollMessage(
      const base::Optional<std::string>& session_id,
      const base::Optional<CryptAuthFeatureType>& feature_type) override {
    OnReenrollMessageProxy(session_id, feature_type);
  }

  void OnResyncMessage(
      const base::Optional<std::string>& session_id,
      const base::Optional<CryptAuthFeatureType>& feature_type) override {
    OnResyncMessageProxy(session_id, feature_type);
  }

  MOCK_METHOD1(OnGCMRegistrationResultProxy, void(bool));
  MOCK_METHOD2(OnReenrollMessageProxy,
               void(base::Optional<std::string> session_id,
                    base::Optional<CryptAuthFeatureType> feature_type));
  MOCK_METHOD2(OnResyncMessageProxy,
               void(base::Optional<std::string> session_id,
                    base::Optional<CryptAuthFeatureType> feature_type));

  testing::StrictMock<MockGCMDriver> gcm_driver_;

  TestingPrefServiceSimple pref_service_;

  CryptAuthGCMManagerImpl gcm_manager_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncCryptAuthGCMManagerImplTest);
};

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, RegisterPrefs) {
  TestingPrefServiceSimple pref_service;
  CryptAuthGCMManager::RegisterPrefs(pref_service.registry());
  EXPECT_TRUE(pref_service.FindPreference(prefs::kCryptAuthGCMRegistrationId));
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, RegistrationSucceeds) {
  EXPECT_EQ(std::string(), gcm_manager_.GetRegistrationId());
  RegisterWithGCM(gcm::GCMClient::SUCCESS);
  EXPECT_EQ(kNewGCMRegistrationId, gcm_manager_.GetRegistrationId());
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest,
       RegistrationSucceedsWithExistingRegistration) {
  pref_service_.SetString(prefs::kCryptAuthGCMRegistrationId,
                          kExistingGCMRegistrationId);
  EXPECT_EQ(kExistingGCMRegistrationId, gcm_manager_.GetRegistrationId());
  RegisterWithGCM(gcm::GCMClient::SUCCESS);
  EXPECT_EQ(kNewGCMRegistrationId, gcm_manager_.GetRegistrationId());
  EXPECT_EQ(kNewGCMRegistrationId,
            pref_service_.GetString(prefs::kCryptAuthGCMRegistrationId));
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, RegisterWithGCMFails) {
  EXPECT_EQ(std::string(), gcm_manager_.GetRegistrationId());
  RegisterWithGCM(gcm::GCMClient::SERVER_ERROR);
  EXPECT_EQ(std::string(), gcm_manager_.GetRegistrationId());
  EXPECT_EQ(std::string(),
            pref_service_.GetString(prefs::kCryptAuthGCMRegistrationId));
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest,
       RegisterWithGCMFailsWithExistingRegistration) {
  pref_service_.SetString(prefs::kCryptAuthGCMRegistrationId,
                          kExistingGCMRegistrationId);
  EXPECT_EQ(kExistingGCMRegistrationId, gcm_manager_.GetRegistrationId());
  RegisterWithGCM(gcm::GCMClient::SERVER_ERROR);
  EXPECT_EQ(kExistingGCMRegistrationId, gcm_manager_.GetRegistrationId());
  EXPECT_EQ(kExistingGCMRegistrationId,
            pref_service_.GetString(prefs::kCryptAuthGCMRegistrationId));
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, RegistrationFailsThenSucceeds) {
  EXPECT_EQ(std::string(), gcm_manager_.GetRegistrationId());
  RegisterWithGCM(gcm::GCMClient::NETWORK_ERROR);
  EXPECT_EQ(std::string(), gcm_manager_.GetRegistrationId());
  RegisterWithGCM(gcm::GCMClient::SUCCESS);
  EXPECT_EQ(kNewGCMRegistrationId, gcm_manager_.GetRegistrationId());
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, ConcurrentRegistrations) {
  // If multiple RegisterWithGCM() calls are made concurrently, only one
  // registration attempt should actually be made.
  EXPECT_CALL(gcm_driver_, RegisterImpl(kCryptAuthGCMAppId, _));
  gcm_manager_.RegisterWithGCM();
  gcm_manager_.RegisterWithGCM();
  gcm_manager_.RegisterWithGCM();

  EXPECT_CALL(*this, OnGCMRegistrationResultProxy(true));
  gcm_driver_.RegisterFinished(kCryptAuthGCMAppId, kNewGCMRegistrationId,
                               gcm::GCMClient::SUCCESS);
  EXPECT_EQ(kNewGCMRegistrationId, gcm_manager_.GetRegistrationId());
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest,
       ReenrollmentMessagesReceived_RegistrationTickleType) {
  EXPECT_CALL(*this,
              OnReenrollMessageProxy(
                  base::Optional<std::string>() /* session_id */,
                  base::Optional<CryptAuthFeatureType>() /* feature_type */))
      .Times(2);

  gcm::IncomingMessage message;
  message.data["registrationTickleType"] = "1";  // FORCE_ENROLLMENT
  message.collapse_key = kCryptAuthMessageCollapseKey;
  message.sender_id = kCryptAuthGCMSenderId;

  gcm::GCMAppHandler* gcm_app_handler =
      static_cast<gcm::GCMAppHandler*>(&gcm_manager_);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
  message.data["registrationTickleType"] = "2";  // UPDATE_ENROLLMENT
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest,
       ReenrollmentMessagesReceived_TargetService) {
  {
    ::testing::InSequence dummy;

    EXPECT_CALL(*this,
                OnReenrollMessageProxy(
                    base::Optional<std::string>(kSessionId1),
                    base::Optional<CryptAuthFeatureType>(kFeatureType1)));
    EXPECT_CALL(*this,
                OnReenrollMessageProxy(
                    base::Optional<std::string>(kSessionId2),
                    base::Optional<CryptAuthFeatureType>(kFeatureType2)));
  }

  gcm::IncomingMessage message;
  message.data["S"] =
      base::NumberToString(cryptauthv2::TargetService::ENROLLMENT);
  message.data["I"] = kSessionId1;
  message.data["F"] = CryptAuthFeatureTypeToGcmHash(kFeatureType1);
  message.data["K"] = CryptAuthKeyBundle::KeyBundleNameEnumToString(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether);
  message.collapse_key = kCryptAuthMessageCollapseKey;
  message.sender_id = kCryptAuthGCMSenderId;

  gcm::GCMAppHandler* gcm_app_handler =
      static_cast<gcm::GCMAppHandler*>(&gcm_manager_);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);

  message.data["I"] = kSessionId2;
  message.data["F"] = CryptAuthFeatureTypeToGcmHash(kFeatureType2);

  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest,
       ResyncMessagesReceived_RegistrationTickleType) {
  EXPECT_CALL(*this,
              OnResyncMessageProxy(
                  base::Optional<std::string>() /* session_id */,
                  base::Optional<CryptAuthFeatureType>() /* feature_type */))
      .Times(2);

  gcm::IncomingMessage message;
  message.data["registrationTickleType"] = "3";  // DEVICES_SYNC
  message.collapse_key = kCryptAuthMessageCollapseKey;
  message.sender_id = kCryptAuthGCMSenderId;

  gcm::GCMAppHandler* gcm_app_handler =
      static_cast<gcm::GCMAppHandler*>(&gcm_manager_);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest,
       ResyncMessagesReceived_TargetService) {
  {
    ::testing::InSequence dummy;

    EXPECT_CALL(*this,
                OnResyncMessageProxy(
                    base::Optional<std::string>(kSessionId1),
                    base::Optional<CryptAuthFeatureType>(kFeatureType1)));
    EXPECT_CALL(*this,
                OnResyncMessageProxy(
                    base::Optional<std::string>(kSessionId2),
                    base::Optional<CryptAuthFeatureType>(kFeatureType2)));
  }

  gcm::IncomingMessage message;
  message.data["S"] =
      base::NumberToString(cryptauthv2::TargetService::DEVICE_SYNC);
  message.data["I"] = kSessionId1;
  message.data["F"] = CryptAuthFeatureTypeToGcmHash(kFeatureType1);
  message.data["K"] = CryptAuthKeyBundle::KeyBundleNameEnumToString(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether);
  message.collapse_key = kCryptAuthMessageCollapseKey;
  message.sender_id = kCryptAuthGCMSenderId;

  gcm::GCMAppHandler* gcm_app_handler =
      static_cast<gcm::GCMAppHandler*>(&gcm_manager_);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);

  message.data["I"] = kSessionId2;
  message.data["F"] = CryptAuthFeatureTypeToGcmHash(kFeatureType2);

  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, InvalidRegistrationTickleType) {
  EXPECT_CALL(*this, OnReenrollMessageProxy(_, _)).Times(0);
  EXPECT_CALL(*this, OnResyncMessageProxy(_, _)).Times(0);

  gcm::IncomingMessage message;
  message.data["registrationTickleType"] = "invalid";
  message.collapse_key = kCryptAuthMessageCollapseKey;
  message.sender_id = kCryptAuthGCMSenderId;

  gcm::GCMAppHandler* gcm_app_handler =
      static_cast<gcm::GCMAppHandler*>(&gcm_manager_);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, InvalidTargetService) {
  EXPECT_CALL(*this, OnReenrollMessageProxy(_, _)).Times(0);
  EXPECT_CALL(*this, OnResyncMessageProxy(_, _)).Times(0);

  gcm::IncomingMessage message;
  message.data["S"] = "invalid";
  message.data["I"] = kSessionId1;
  message.data["F"] = CryptAuthFeatureTypeToGcmHash(kFeatureType1);
  message.data["K"] = CryptAuthKeyBundle::KeyBundleNameEnumToString(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether);
  message.collapse_key = kCryptAuthMessageCollapseKey;
  message.sender_id = kCryptAuthGCMSenderId;

  gcm::GCMAppHandler* gcm_app_handler =
      static_cast<gcm::GCMAppHandler*>(&gcm_manager_);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest,
       InvalidRegistrationTickleTypeAndTargetService) {
  EXPECT_CALL(*this, OnReenrollMessageProxy(_, _)).Times(0);
  EXPECT_CALL(*this, OnResyncMessageProxy(_, _)).Times(0);

  gcm::IncomingMessage message;
  message.data["registrationTickleType"] = "invalid";
  message.data["S"] = "invalid";
  message.data["I"] = kSessionId1;
  message.data["F"] = CryptAuthFeatureTypeToGcmHash(kFeatureType1);
  message.data["K"] = CryptAuthKeyBundle::KeyBundleNameEnumToString(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether);
  message.collapse_key = kCryptAuthMessageCollapseKey;
  message.sender_id = kCryptAuthGCMSenderId;

  gcm::GCMAppHandler* gcm_app_handler =
      static_cast<gcm::GCMAppHandler*>(&gcm_manager_);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, InvalidDeviceSyncGroupName) {
  EXPECT_CALL(*this, OnReenrollMessageProxy(_, _)).Times(0);
  EXPECT_CALL(*this, OnResyncMessageProxy(_, _)).Times(0);

  gcm::IncomingMessage message;
  message.data["S"] =
      base::NumberToString(cryptauthv2::TargetService::DEVICE_SYNC);
  message.data["I"] = kSessionId1;
  message.data["F"] = CryptAuthFeatureTypeToGcmHash(kFeatureType1);
  message.data["K"] = "invalid";
  message.collapse_key = kCryptAuthMessageCollapseKey;
  message.sender_id = kCryptAuthGCMSenderId;

  gcm::GCMAppHandler* gcm_app_handler =
      static_cast<gcm::GCMAppHandler*>(&gcm_manager_);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest,
       RegistrationTickleTypeAndTargetServiceSpecified_PreferTargetService) {
  // In practice, "registrationTickleType" and "S" keys should never be
  // contained in the same GCM message. If they are, a valid "S" value is
  // arbitrarily preferred.
  EXPECT_CALL(*this, OnReenrollMessageProxy(
                         base::Optional<std::string>(kSessionId1),
                         base::Optional<CryptAuthFeatureType>(kFeatureType1)));
  EXPECT_CALL(*this, OnResyncMessageProxy(_, _)).Times(0);

  gcm::IncomingMessage message;
  message.data["registrationTickleType"] = "3";  // DEVICE_SYNC
  message.data["S"] =
      base::NumberToString(cryptauthv2::TargetService::ENROLLMENT);
  message.data["I"] = kSessionId1;
  message.data["F"] = CryptAuthFeatureTypeToGcmHash(kFeatureType1);
  message.data["K"] = CryptAuthKeyBundle::KeyBundleNameEnumToString(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether);
  message.collapse_key = kCryptAuthMessageCollapseKey;
  message.sender_id = kCryptAuthGCMSenderId;

  gcm::GCMAppHandler* gcm_app_handler =
      static_cast<gcm::GCMAppHandler*>(&gcm_manager_);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest,
       RegistrationTickleTypeAndTargetServiceSpecified_InvalidTargetService) {
  // In practice, "registrationTickleType" and "S" keys should never be
  // contained in the same GCM message. If they are and the "S" value is
  // invalid,, try the "registrationTickleType" value.
  EXPECT_CALL(*this, OnReenrollMessageProxy(_, _)).Times(0);
  EXPECT_CALL(*this, OnResyncMessageProxy(
                         base::Optional<std::string>(kSessionId1),
                         base::Optional<CryptAuthFeatureType>(kFeatureType1)));

  gcm::IncomingMessage message;
  message.data["registrationTickleType"] = "3";  // DEVICE_SYNC
  message.data["S"] = "invalid";
  message.data["I"] = kSessionId1;
  message.data["F"] = CryptAuthFeatureTypeToGcmHash(kFeatureType1);
  message.data["K"] = CryptAuthKeyBundle::KeyBundleNameEnumToString(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether);
  message.collapse_key = kCryptAuthMessageCollapseKey;
  message.sender_id = kCryptAuthGCMSenderId;

  gcm::GCMAppHandler* gcm_app_handler =
      static_cast<gcm::GCMAppHandler*>(&gcm_manager_);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, MissingFeatureType) {
  EXPECT_CALL(*this, OnReenrollMessageProxy(_, _)).Times(0);
  EXPECT_CALL(*this,
              OnResyncMessageProxy(base::Optional<std::string>(kSessionId1),
                                   base::Optional<CryptAuthFeatureType>()));

  // Do not include feature type key "F" in the message.
  gcm::IncomingMessage message;
  message.data["S"] =
      base::NumberToString(cryptauthv2::TargetService::DEVICE_SYNC);
  message.data["I"] = kSessionId1;
  message.data["K"] = CryptAuthKeyBundle::KeyBundleNameEnumToString(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether);
  message.collapse_key = kCryptAuthMessageCollapseKey;
  message.sender_id = kCryptAuthGCMSenderId;

  gcm::GCMAppHandler* gcm_app_handler =
      static_cast<gcm::GCMAppHandler*>(&gcm_manager_);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, InvalidFeatureType) {
  EXPECT_CALL(*this, OnReenrollMessageProxy(_, _)).Times(0);
  EXPECT_CALL(*this,
              OnResyncMessageProxy(base::Optional<std::string>(kSessionId1),
                                   base::Optional<CryptAuthFeatureType>()));

  // Do not include feature type key "F" in the message.
  gcm::IncomingMessage message;
  message.data["S"] =
      base::NumberToString(cryptauthv2::TargetService::DEVICE_SYNC);
  message.data["I"] = kSessionId1;
  message.data["F"] = "invalid";
  message.data["K"] = CryptAuthKeyBundle::KeyBundleNameEnumToString(
      CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether);
  message.collapse_key = kCryptAuthMessageCollapseKey;
  message.sender_id = kCryptAuthGCMSenderId;

  gcm::GCMAppHandler* gcm_app_handler =
      static_cast<gcm::GCMAppHandler*>(&gcm_manager_);
  gcm_app_handler->OnMessage(kCryptAuthGCMAppId, message);
}

}  // namespace device_sync

}  // namespace chromeos
