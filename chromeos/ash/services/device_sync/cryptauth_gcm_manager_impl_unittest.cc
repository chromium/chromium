// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_gcm_manager_impl.h"

#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/services/device_sync/cryptauth_feature_type.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::SaveArg;
using ::testing::WithArg;

namespace ash {

namespace device_sync {

namespace {

const char kCryptAuthGCMAppId[] = "com.google.chrome.cryptauth";
const char kCryptAuthGCMSenderId[] = "381449029288";
const char kDeprecatedGCMRegistrationId[] =
    "APA91bEMJr4m6X8GGZ8ZaOfrD8Yiqr1Tu-r9EyQGL";
const char kExistingGCMRegistrationId[] =
    "mtAK6jy7mB9U:APA91bEMJr4m6X8GGZ8ZaOfrD8Yiqr1Tu-r9EyQGL";
const char kNewGCMRegistrationId[] =
    "eNSVPJ1dHHU:APA91bHRxfu0Cz0vts7IJX2KoIBw57J52Lnvnuy8mz-S";
const char kCryptAuthMessageCollapseKey[] =
    "collapse_cryptauth_sync_DEVICES_SYNC";
const char kSessionId1[] = "session_id_1";
const char kSessionId2[] = "session_id_2";
const CryptAuthFeatureType kFeatureType1 =
    CryptAuthFeatureType::kBetterTogetherHostEnabled;
const CryptAuthFeatureType kFeatureType2 =
    CryptAuthFeatureType::kEasyUnlockHostEnabled;

class MockInstanceID : public instance_id::InstanceID {
 public:
  explicit MockInstanceID(gcm::FakeGCMDriver* gcm_driver)
      : InstanceID(kCryptAuthGCMAppId, gcm_driver) {}
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

// Mock GCMDriver implementation for testing.
class MockGCMDriver : public gcm::FakeGCMDriver {
 public:
  MockGCMDriver() {}

  MockGCMDriver(const MockGCMDriver&) = delete;
  MockGCMDriver& operator=(const MockGCMDriver&) = delete;

  ~MockGCMDriver() override {}

  MOCK_METHOD2(AddAppHandler,
               void(const std::string& app_id, gcm::GCMAppHandler* handler));

  MOCK_METHOD2(RegisterImpl,
               void(const std::string& app_id,
                    const std::vector<std::string>& sender_ids));

  using gcm::GCMDriver::RegisterFinished;
};

}  // namespace

class DeviceSyncCryptAuthGCMManagerImplTest
    : public testing::Test,
      public CryptAuthGCMManager::Observer {
 public:
  DeviceSyncCryptAuthGCMManagerImplTest(
      const DeviceSyncCryptAuthGCMManagerImplTest&) = delete;
  DeviceSyncCryptAuthGCMManagerImplTest& operator=(
      const DeviceSyncCryptAuthGCMManagerImplTest&) = delete;

 protected:
  DeviceSyncCryptAuthGCMManagerImplTest()
      : gcm_manager_(&gcm_driver_, &mock_instance_id_driver_, &pref_service_) {}

  // testing::Test:
  void SetUp() override {
    CryptAuthGCMManager::RegisterPrefs(pref_service_.registry());
    gcm_manager_.AddObserver(this);
    ON_CALL(mock_instance_id_driver_, GetInstanceID(kCryptAuthGCMAppId))
        .WillByDefault(Return(&mock_instance_id_));
    EXPECT_CALL(gcm_driver_, AddAppHandler(kCryptAuthGCMAppId, &gcm_manager_));
    gcm_manager_.StartListening();
  }

  void TearDown() override { gcm_manager_.RemoveObserver(this); }

  void RegisterWithGCM(instance_id::InstanceID::Result registration_result) {
    EXPECT_CALL(mock_instance_id_, GetToken(kCryptAuthGCMSenderId, _, _, _, _))
        .WillOnce(WithArg<4>([registration_result](
                                 MockInstanceID::GetTokenCallback callback) {
          std::move(callback).Run(kNewGCMRegistrationId, registration_result);
        }));

    bool success = (registration_result == instance_id::InstanceID::SUCCESS);
    EXPECT_CALL(*this, OnGCMRegistrationResultProxy(success));

    gcm_manager_.RegisterWithGCM();
  }

  // CryptAuthGCMManager::Observer:
  void OnGCMRegistrationResult(bool success) override {
    OnGCMRegistrationResultProxy(success);
  }

  void OnReenrollMessage(
      const std::optional<std::string>& session_id,
      const std::optional<CryptAuthFeatureType>& feature_type) override {
    OnReenrollMessageProxy(session_id, feature_type);
  }

  void OnResyncMessage(
      const std::optional<std::string>& session_id,
      const std::optional<CryptAuthFeatureType>& feature_type) override {
    OnResyncMessageProxy(session_id, feature_type);
  }

  MOCK_METHOD1(OnGCMRegistrationResultProxy, void(bool));
  MOCK_METHOD2(OnReenrollMessageProxy,
               void(std::optional<std::string> session_id,
                    std::optional<CryptAuthFeatureType> feature_type));
  MOCK_METHOD2(OnResyncMessageProxy,
               void(std::optional<std::string> session_id,
                    std::optional<CryptAuthFeatureType> feature_type));

  testing::StrictMock<MockGCMDriver> gcm_driver_;
  testing::NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  testing::NiceMock<MockInstanceID> mock_instance_id_{&gcm_driver_};

  TestingPrefServiceSimple pref_service_;

  CryptAuthGCMManagerImpl gcm_manager_;
};

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, RegisterPrefs) {
  TestingPrefServiceSimple pref_service;
  CryptAuthGCMManager::RegisterPrefs(pref_service.registry());
  EXPECT_TRUE(pref_service.FindPreference(prefs::kCryptAuthGCMRegistrationId));
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, IsRegistrationIdDeprecated) {
  // Deprecated V3 tokens should return true.
  EXPECT_TRUE(CryptAuthGCMManager::IsRegistrationIdDeprecated(
      kDeprecatedGCMRegistrationId));
  EXPECT_TRUE(CryptAuthGCMManager::IsRegistrationIdDeprecated("test-token"));

  EXPECT_FALSE(CryptAuthGCMManager::IsRegistrationIdDeprecated(
      kExistingGCMRegistrationId));
  EXPECT_FALSE(
      CryptAuthGCMManager::IsRegistrationIdDeprecated(kNewGCMRegistrationId));
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, RegistrationSucceeds) {
  EXPECT_EQ(std::string(), gcm_manager_.GetRegistrationId());
  RegisterWithGCM(instance_id::InstanceID::SUCCESS);
  EXPECT_EQ(kNewGCMRegistrationId, gcm_manager_.GetRegistrationId());
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest,
       RegistrationSucceedsWithExistingRegistration) {
  pref_service_.SetString(prefs::kCryptAuthGCMRegistrationId,
                          kExistingGCMRegistrationId);
  EXPECT_EQ(kExistingGCMRegistrationId, gcm_manager_.GetRegistrationId());
  RegisterWithGCM(instance_id::InstanceID::SUCCESS);
  EXPECT_EQ(kNewGCMRegistrationId, gcm_manager_.GetRegistrationId());
  EXPECT_EQ(kNewGCMRegistrationId,
            pref_service_.GetString(prefs::kCryptAuthGCMRegistrationId));
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, RegisterWithGCMFails) {
  EXPECT_EQ(std::string(), gcm_manager_.GetRegistrationId());
  RegisterWithGCM(instance_id::InstanceID::SERVER_ERROR);
  EXPECT_EQ(std::string(), gcm_manager_.GetRegistrationId());
  EXPECT_EQ(std::string(),
            pref_service_.GetString(prefs::kCryptAuthGCMRegistrationId));
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest,
       RegisterWithGCMFailsWithExistingRegistration) {
  pref_service_.SetString(prefs::kCryptAuthGCMRegistrationId,
                          kExistingGCMRegistrationId);
  EXPECT_EQ(kExistingGCMRegistrationId, gcm_manager_.GetRegistrationId());
  RegisterWithGCM(instance_id::InstanceID::SERVER_ERROR);
  EXPECT_EQ(kExistingGCMRegistrationId, gcm_manager_.GetRegistrationId());
  EXPECT_EQ(kExistingGCMRegistrationId,
            pref_service_.GetString(prefs::kCryptAuthGCMRegistrationId));
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, RegistrationFailsThenSucceeds) {
  EXPECT_EQ(std::string(), gcm_manager_.GetRegistrationId());
  RegisterWithGCM(instance_id::InstanceID::NETWORK_ERROR);
  EXPECT_EQ(std::string(), gcm_manager_.GetRegistrationId());
  RegisterWithGCM(instance_id::InstanceID::SUCCESS);
  EXPECT_EQ(kNewGCMRegistrationId, gcm_manager_.GetRegistrationId());
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest, MultipleRegistrations) {
  RegisterWithGCM(instance_id::InstanceID::SUCCESS);
  RegisterWithGCM(instance_id::InstanceID::SUCCESS);
  RegisterWithGCM(instance_id::InstanceID::SUCCESS);

  EXPECT_EQ(kNewGCMRegistrationId, gcm_manager_.GetRegistrationId());
}

TEST_F(DeviceSyncCryptAuthGCMManagerImplTest,
       ReenrollmentMessagesReceived_RegistrationTickleType) {
  EXPECT_CALL(*this,
              OnReenrollMessageProxy(
                  std::optional<std::string>() /* session_id */,
                  std::optional<CryptAuthFeatureType>() /* feature_type */))
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

    EXPECT_CALL(*this, OnReenrollMessageProxy(
                           std::optional<std::string>(kSessionId1),
                           std::optional<CryptAuthFeatureType>(kFeatureType1)));
    EXPECT_CALL(*this, OnReenrollMessageProxy(
                           std::optional<std::string>(kSessionId2),
                           std::optional<CryptAuthFeatureType>(kFeatureType2)));
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
                  std::optional<std::string>() /* session_id */,
                  std::optional<CryptAuthFeatureType>() /* feature_type */))
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

    EXPECT_CALL(*this, OnResyncMessageProxy(
                           std::optional<std::string>(kSessionId1),
                           std::optional<CryptAuthFeatureType>(kFeatureType1)));
    EXPECT_CALL(*this, OnResyncMessageProxy(
                           std::optional<std::string>(kSessionId2),
                           std::optional<CryptAuthFeatureType>(kFeatureType2)));
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
                         std::optional<std::string>(kSessionId1),
                         std::optional<CryptAuthFeatureType>(kFeatureType1)));
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
                         std::optional<std::string>(kSessionId1),
                         std::optional<CryptAuthFeatureType>(kFeatureType1)));

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
              OnResyncMessageProxy(std::optional<std::string>(kSessionId1),
                                   std::optional<CryptAuthFeatureType>()));

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
              OnResyncMessageProxy(std::optional<std::string>(kSessionId1),
                                   std::optional<CryptAuthFeatureType>()));

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

}  // namespace ash
