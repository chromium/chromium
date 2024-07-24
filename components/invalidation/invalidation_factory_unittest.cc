// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/invalidation_factory.h"

#include <variant>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {
namespace {

constexpr char kDriveFcmSenderId[] = "947318989803";
constexpr char kFakeSenderId[] = "fake_sender_id";
constexpr char kFakeProjectId[] = "fake_project_id";

constexpr char kLogPrefix[] = "test log";

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

class MockInstanceID : public instance_id::InstanceID {
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

class InvalidationFactoryTestBase : public testing::Test {
 protected:
  InvalidationFactoryTestBase() {
    pref_service_.registry()->RegisterDictionaryPref(
        prefs::kInvalidationClientIDCache);
    InvalidatorRegistrarWithMemory::RegisterProfilePrefs(
        pref_service_.registry());
    PerUserTopicSubscriptionManager::RegisterPrefs(pref_service_.registry());
  }

  ~InvalidationFactoryTestBase() override = default;

  base::test::SingleThreadTaskEnvironment task_environment_;

  gcm::FakeGCMDriver fake_gcm_driver_;
  testing::NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  testing::NiceMock<MockInstanceID> mock_instance_id_;
  signin::IdentityTestEnvironment identity_test_env_;
  ProfileIdentityProvider identity_provider_{
      identity_test_env_.identity_manager()};
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingPrefServiceSimple pref_service_;
};

class InvalidationFactoryWithDirectMessagesDisabledTest
    : public InvalidationFactoryTestBase {
 protected:
  InvalidationFactoryWithDirectMessagesDisabledTest() {
    scoped_features_.InitAndDisableFeature(kInvalidationsWithDirectMessages);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(InvalidationFactoryWithDirectMessagesDisabledTest,
       CreatesInvalidationService) {
  auto service_or_listener = CreateInvalidationServiceOrListener(
      &identity_provider_, &fake_gcm_driver_, &mock_instance_id_driver_,
      test_url_loader_factory_.GetSafeWeakWrapper(), &pref_service_,
      kFakeSenderId, kFakeProjectId, kLogPrefix);

  ASSERT_TRUE(std::holds_alternative<std::unique_ptr<InvalidationService>>(
      service_or_listener));
  EXPECT_TRUE(
      std::get<std::unique_ptr<InvalidationService>>(service_or_listener));
}

TEST_F(InvalidationFactoryWithDirectMessagesDisabledTest,
       CreatesInvalidationServiceForDrive) {
  auto service_or_listener = CreateInvalidationServiceOrListener(
      &identity_provider_, &fake_gcm_driver_, &mock_instance_id_driver_,
      test_url_loader_factory_.GetSafeWeakWrapper(), &pref_service_,
      kDriveFcmSenderId, kFakeProjectId, kLogPrefix);

  ASSERT_TRUE(std::holds_alternative<std::unique_ptr<InvalidationService>>(
      service_or_listener));
  EXPECT_TRUE(
      std::get<std::unique_ptr<InvalidationService>>(service_or_listener));
}

class InvalidationFactoryWithDirectMessagesEnabledTest
    : public InvalidationFactoryTestBase {
 protected:
  InvalidationFactoryWithDirectMessagesEnabledTest() {
    scoped_features_.InitAndEnableFeature(kInvalidationsWithDirectMessages);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(InvalidationFactoryWithDirectMessagesEnabledTest,
       CreatesInvalidationListener) {
  auto service_or_listener = CreateInvalidationServiceOrListener(
      &identity_provider_, &fake_gcm_driver_, &mock_instance_id_driver_,
      test_url_loader_factory_.GetSafeWeakWrapper(), &pref_service_,
      kFakeSenderId, kFakeProjectId, kLogPrefix);

  ASSERT_TRUE(std::holds_alternative<std::unique_ptr<InvalidationListener>>(
      service_or_listener));
  EXPECT_TRUE(
      std::get<std::unique_ptr<InvalidationListener>>(service_or_listener));
}

TEST_F(InvalidationFactoryWithDirectMessagesEnabledTest,
       CreatesInvalidationServiceForDrive) {
  base::test::ScopedFeatureList scoped_features(
      kInvalidationsWithDirectMessages);

  auto service_or_listener = CreateInvalidationServiceOrListener(
      &identity_provider_, &fake_gcm_driver_, &mock_instance_id_driver_,
      test_url_loader_factory_.GetSafeWeakWrapper(), &pref_service_,
      kDriveFcmSenderId, kFakeProjectId, kLogPrefix);

  ASSERT_TRUE(std::holds_alternative<std::unique_ptr<InvalidationService>>(
      service_or_listener));
  EXPECT_TRUE(
      std::get<std::unique_ptr<InvalidationService>>(service_or_listener));
}

}  // namespace
}  // namespace invalidation
