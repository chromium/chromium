// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_driver_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/network/autofill_ai/mock_wallet_pass_access_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;

class TestAutofillClientForAccessManager : public TestAutofillClient {
 public:
  explicit TestAutofillClientForAccessManager(
      std::unique_ptr<device_reauth::MockDeviceAuthenticator>* authenticator)
      : authenticator_(authenticator) {}
  ~TestAutofillClientForAccessManager() override = default;

  std::unique_ptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator(
      std::string histogram) const override {
    if (authenticator_ && *authenticator_) {
      return std::move(*authenticator_);
    }
    return nullptr;
  }

 private:
  raw_ptr<std::unique_ptr<device_reauth::MockDeviceAuthenticator>>
      authenticator_;
};

class AutofillAiAccessManagerTest : public testing::Test {
 public:
  AutofillAiAccessManagerTest() : client_(&mock_authenticator_) {
    client_.set_entity_data_manager(std::make_unique<EntityDataManager>(
        client_.GetPrefs(), client_.GetIdentityManager(),
        client_.GetSyncService(), helper_.autofill_webdata_service(),
        /*history_service=*/nullptr,
        /*strike_database=*/nullptr,
        /*accessibility_annotator_service=*/nullptr,
        /*variation_country_code=*/GeoIpCountryCode("US")));
    client_.SetUpPrefsAndIdentityForAutofillAi();
    client_.set_wallet_pass_access_manager(
        std::make_unique<NiceMock<MockWalletPassAccessManager>>());

    driver_ = std::make_unique<TestAutofillDriver>(&client_);
    manager_ = std::make_unique<TestBrowserAutofillManager>(driver_.get());
  }
  ~AutofillAiAccessManagerTest() override {
    if (driver_) {
      test_api(*driver_).SetLifecycleState(
          AutofillDriver::LifecycleState::kPendingDeletion);
    }
  }

  void AddOrUpdateEntityInstance(EntityInstance entity) {
    edm().AddOrUpdateEntityInstance(std::move(entity));
    helper_.WaitUntilIdle();
  }

  TestAutofillClientForAccessManager& client() { return client_; }
  EntityDataManager& edm() { return *client_.GetEntityDataManager(); }
  MockWalletPassAccessManager& wallet_manager() {
    return static_cast<MockWalletPassAccessManager&>(
        *client_.GetWalletPassAccessManager());
  }
  AutofillAiAccessManager& access_manager() {
    return manager_->GetAutofillAiAccessManager();
  }

 protected:
  std::unique_ptr<device_reauth::MockDeviceAuthenticator> mock_authenticator_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAutofillAiWithDataSchema};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_environment_;
  TestAutofillClientForAccessManager client_;
  AutofillWebDataServiceTestHelper helper_{std::make_unique<EntityTable>()};
  std::unique_ptr<TestAutofillDriver> driver_;
  std::unique_ptr<TestBrowserAutofillManager> manager_;
};

// Tests that when no re-authentication is required, FetchEntityInstance
// immediately invokes the callback with the unmasked entity, and returns false
// (not async).
TEST_F(AutofillAiAccessManagerTest, NoReauthRequired_LocalEntity) {
  EntityInstance passport = test::GetPassportEntityInstance();
  AddOrUpdateEntityInstance(passport);

  base::MockCallback<AutofillAiAccessManager::OnEntityInstanceFetchedCallback>
      callback;

  EXPECT_CALL(
      callback,
      Run(base::expected<EntityInstance,
                         AutofillAiAccessManager::FailureReason>(passport),
          /*reauth_attempted=*/false));

  EXPECT_FALSE(access_manager().FetchEntityInstance(
      passport, /*will_fill_sensitive_info=*/false, callback.Get()));
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_IOS)
// Tests that when re-authentication is required and succeeds,
// FetchEntityInstance triggers re-auth, returns true (async), and fills the
// entity.
TEST_F(AutofillAiAccessManagerTest, ReauthRequired_ReauthAccepted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillAiReauthRequired);
  client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiReauthBeforeViewingSensitiveData, true);

  EntityInstance passport = test::GetPassportEntityInstance();
  AddOrUpdateEntityInstance(passport);

  mock_authenticator_ =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator_, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_authenticator_, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(true));
  test_api(access_manager())
      .SetDeviceAuthenticator(std::move(mock_authenticator_));

  base::MockCallback<AutofillAiAccessManager::OnEntityInstanceFetchedCallback>
      callback;

  EXPECT_CALL(
      callback,
      Run(base::expected<EntityInstance,
                         AutofillAiAccessManager::FailureReason>(passport),
          /*reauth_attempted=*/true));

  EXPECT_TRUE(access_manager().FetchEntityInstance(
      passport, /*will_fill_sensitive_info=*/true, callback.Get()));
}

// Tests that when re-authentication is required and rejected,
// FetchEntityInstance invokes the callback with FailureReason::kReauthFailed.
TEST_F(AutofillAiAccessManagerTest, ReauthRequired_ReauthRejected) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillAiReauthRequired);
  client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiReauthBeforeViewingSensitiveData, true);

  EntityInstance passport = test::GetPassportEntityInstance();
  AddOrUpdateEntityInstance(passport);

  mock_authenticator_ =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator_, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_authenticator_, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(false));
  test_api(access_manager())
      .SetDeviceAuthenticator(std::move(mock_authenticator_));

  base::MockCallback<AutofillAiAccessManager::OnEntityInstanceFetchedCallback>
      callback;

  EXPECT_CALL(
      callback,
      Run(base::expected<EntityInstance,
                         AutofillAiAccessManager::FailureReason>(
              base::unexpected(
                  AutofillAiAccessManager::FailureReason::kReauthFailed)),
          /*reauth_attempted=*/true));

  EXPECT_TRUE(access_manager().FetchEntityInstance(
      passport, /*will_fill_sensitive_info=*/true, callback.Get()));
}

// Tests that when re-authentication is required but the device does not support
// screen lock, FetchEntityInstance assumes success to avoid blocking the user.
TEST_F(AutofillAiAccessManagerTest, ReauthRequired_NoAuthenticator) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillAiReauthRequired);
  client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiReauthBeforeViewingSensitiveData, true);

  EntityInstance passport = test::GetPassportEntityInstance();
  AddOrUpdateEntityInstance(passport);

  mock_authenticator_ =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator_, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(false));
  test_api(access_manager())
      .SetDeviceAuthenticator(std::move(mock_authenticator_));

  base::MockCallback<AutofillAiAccessManager::OnEntityInstanceFetchedCallback>
      callback;

  EXPECT_CALL(
      callback,
      Run(base::expected<EntityInstance,
                         AutofillAiAccessManager::FailureReason>(passport),
          /*reauth_attempted=*/true));

  EXPECT_TRUE(access_manager().FetchEntityInstance(
      passport, /*will_fill_sensitive_info=*/true, callback.Get()));
}
#endif

// Tests that when unmasking a server entity successfully, the unmasked entity
// is passed to the callback.
TEST_F(AutofillAiAccessManagerTest, ServerFetch_Success) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kAutofillAiWalletPrivatePasses},
      {features::kAutofillAiReauthRequired});

  EntityInstance full_passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  EntityInstance masked_passport = test::MaskEntityInstance(full_passport);
  AddOrUpdateEntityInstance(masked_passport);

  EXPECT_CALL(wallet_manager(),
              GetUnmaskedWalletEntityInstance(masked_passport.guid(), _))
      .WillOnce(RunOnceCallback<1>(full_passport));

  base::MockCallback<AutofillAiAccessManager::OnEntityInstanceFetchedCallback>
      callback;

  EXPECT_CALL(
      callback,
      Run(base::expected<EntityInstance,
                         AutofillAiAccessManager::FailureReason>(full_passport),
          /*reauth_attempted=*/false));

  EXPECT_TRUE(access_manager().FetchEntityInstance(
      masked_passport, /*will_fill_sensitive_info=*/true, callback.Get()));
}

// Tests that when unmasking a server entity fails, FailureReason::kFetchFailed
// is passed to the callback.
TEST_F(AutofillAiAccessManagerTest, ServerFetch_Failure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kAutofillAiWalletPrivatePasses},
      {features::kAutofillAiReauthRequired});

  EntityInstance full_passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  EntityInstance masked_passport = test::MaskEntityInstance(full_passport);
  AddOrUpdateEntityInstance(masked_passport);

  EXPECT_CALL(wallet_manager(),
              GetUnmaskedWalletEntityInstance(masked_passport.guid(), _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  base::MockCallback<AutofillAiAccessManager::OnEntityInstanceFetchedCallback>
      callback;

  EXPECT_CALL(
      callback,
      Run(base::expected<EntityInstance,
                         AutofillAiAccessManager::FailureReason>(
              base::unexpected(
                  AutofillAiAccessManager::FailureReason::kFetchFailed)),
          /*reauth_attempted=*/false));

  EXPECT_TRUE(access_manager().FetchEntityInstance(
      masked_passport, /*will_fill_sensitive_info=*/true, callback.Get()));
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_IOS)
// Tests that when both re-authentication and unmasking are required and
// succeed, FetchEntityInstance runs both flows and invokes the callback with
// the final unmasked entity.
TEST_F(AutofillAiAccessManagerTest, ReauthAndServerFetch_Success) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kAutofillAiReauthRequired,
       features::kAutofillAiWalletPrivatePasses},
      {});
  client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiReauthBeforeViewingSensitiveData, true);

  EntityInstance full_passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  EntityInstance masked_passport = test::MaskEntityInstance(full_passport);
  AddOrUpdateEntityInstance(masked_passport);

  mock_authenticator_ =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator_, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_authenticator_, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(true));
  test_api(access_manager())
      .SetDeviceAuthenticator(std::move(mock_authenticator_));

  EXPECT_CALL(wallet_manager(),
              GetUnmaskedWalletEntityInstance(masked_passport.guid(), _))
      .WillOnce(RunOnceCallback<1>(full_passport));

  base::MockCallback<AutofillAiAccessManager::OnEntityInstanceFetchedCallback>
      callback;

  EXPECT_CALL(
      callback,
      Run(base::expected<EntityInstance,
                         AutofillAiAccessManager::FailureReason>(full_passport),
          /*reauth_attempted=*/true));

  EXPECT_TRUE(access_manager().FetchEntityInstance(
      masked_passport, /*will_fill_sensitive_info=*/true, callback.Get()));
}

// Tests that when both re-authentication and unmasking are required, and
// re-auth succeeds but server unmasking fails, FetchEntityInstance invokes the
// callback with FailureReason::kFetchFailed.
TEST_F(AutofillAiAccessManagerTest, ReauthAndServerFetch_ServerFetchFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kAutofillAiReauthRequired,
       features::kAutofillAiWalletPrivatePasses},
      {});
  client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiReauthBeforeViewingSensitiveData, true);

  EntityInstance full_passport = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  EntityInstance masked_passport = test::MaskEntityInstance(full_passport);
  AddOrUpdateEntityInstance(masked_passport);

  mock_authenticator_ =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator_, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_authenticator_, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(true));
  test_api(access_manager())
      .SetDeviceAuthenticator(std::move(mock_authenticator_));

  EXPECT_CALL(wallet_manager(),
              GetUnmaskedWalletEntityInstance(masked_passport.guid(), _))
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  base::MockCallback<AutofillAiAccessManager::OnEntityInstanceFetchedCallback>
      callback;

  EXPECT_CALL(
      callback,
      Run(base::expected<EntityInstance,
                         AutofillAiAccessManager::FailureReason>(
              base::unexpected(
                  AutofillAiAccessManager::FailureReason::kFetchFailed)),
          /*reauth_attempted=*/true));

  EXPECT_TRUE(access_manager().FetchEntityInstance(
      masked_passport, /*will_fill_sensitive_info=*/true, callback.Get()));
}

// Tests that calling Reset() cancels the pending authenticator and invalidates
// all pending callbacks.
TEST_F(AutofillAiAccessManagerTest, ResetCancelsPendingOperations) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillAiReauthRequired);
  client().GetPrefs()->SetBoolean(
      prefs::kAutofillAiReauthBeforeViewingSensitiveData, true);

  EntityInstance passport = test::GetPassportEntityInstance();
  AddOrUpdateEntityInstance(passport);

  mock_authenticator_ =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator_, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  // The authenticator is canceled during Reset().
  EXPECT_CALL(*mock_authenticator_, Cancel()).Times(2);
  test_api(access_manager())
      .SetDeviceAuthenticator(std::move(mock_authenticator_));

  base::MockCallback<AutofillAiAccessManager::OnEntityInstanceFetchedCallback>
      callback;

  // The callback should NEVER be run because it is invalidated on Reset().
  EXPECT_CALL(callback, Run).Times(0);

  EXPECT_TRUE(access_manager().FetchEntityInstance(
      passport, /*will_fill_sensitive_info=*/true, callback.Get()));

  access_manager().Reset();
}
#endif

}  // namespace
}  // namespace autofill
