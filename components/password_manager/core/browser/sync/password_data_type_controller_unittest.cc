// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_data_type_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/test/mock_data_type_controller_delegate.h"
#include "components/sync/test/mock_data_type_local_data_batch_uploader.h"
#include "components/sync/test/test_sync_service.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

class PasswordDataTypeControllerTest : public ::testing::Test {
 public:
  PasswordDataTypeControllerTest() {
#if BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOff));
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAccountStorageNoticeShown, false);
#endif

    auto full_sync_delegate =
        std::make_unique<syncer::MockDataTypeControllerDelegate>();
    full_sync_delegate_ = full_sync_delegate.get();
    auto transport_only_delegate =
        std::make_unique<syncer::MockDataTypeControllerDelegate>();
    transport_only_delegate_ = transport_only_delegate.get();
    controller_ = std::make_unique<PasswordDataTypeController>(
        std::move(full_sync_delegate), std::move(transport_only_delegate),
        std::make_unique<syncer::MockDataTypeLocalDataBatchUploader>(),
        &pref_service_, identity_test_env_.identity_manager(), &sync_service_);
  }

  PasswordDataTypeController* controller() { return controller_.get(); }

  PrefService* pref_service() { return &pref_service_; }

  syncer::MockDataTypeControllerDelegate* full_sync_delegate() {
    return full_sync_delegate_;
  }

  syncer::MockDataTypeControllerDelegate* transport_only_delegate() {
    return transport_only_delegate_;
  }

  void SignIn() {
    identity_test_env_.MakePrimaryAccountAvailable(
        "foo@gmail.com", signin::ConsentLevel::kSignin);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<PasswordDataTypeController> controller_;
  raw_ptr<syncer::MockDataTypeControllerDelegate> full_sync_delegate_;
  raw_ptr<syncer::MockDataTypeControllerDelegate> transport_only_delegate_;
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(PasswordDataTypeControllerTest, OverrideFullSyncModeIfUPMLocalOn) {
  pref_service()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
  // `transport_only_delegate` should be used, despite syncer::SyncMode::kFull
  // being passed below.
  EXPECT_CALL(*full_sync_delegate(), OnSyncStarting).Times(0);
  EXPECT_CALL(*transport_only_delegate(), OnSyncStarting);

  syncer::ConfigureContext context;
  context.authenticated_account_id = CoreAccountId::FromGaiaId("gaia");
  context.cache_guid = "cache_guid";
  context.sync_mode = syncer::SyncMode::kFull;
  context.reason = syncer::CONFIGURE_REASON_RECONFIGURATION;
  context.configuration_start_time = base::Time::Now();
  controller()->LoadModels(context, base::DoNothing());
}

TEST_F(PasswordDataTypeControllerTest,
       DoNotOverrideFullSyncModeIfUPMLocalOff) {
  // `full_sync_delegate` should be used for syncer::SyncMode::kFull, as
  // expected.
  EXPECT_CALL(*full_sync_delegate(), OnSyncStarting);
  EXPECT_CALL(*transport_only_delegate(), OnSyncStarting).Times(0);

  syncer::ConfigureContext context;
  context.authenticated_account_id = CoreAccountId::FromGaiaId("gaia");
  context.cache_guid = "cache_guid";
  context.sync_mode = syncer::SyncMode::kFull;
  context.reason = syncer::CONFIGURE_REASON_RECONFIGURATION;
  context.configuration_start_time = base::Time::Now();
  controller()->LoadModels(context, base::DoNothing());
}

TEST_F(PasswordDataTypeControllerTest,
       SetNoticePrefOnSigninIfSyncToSigninEnabled) {
  base::test::ScopedFeatureList enable_sync_to_signin(
      syncer::kReplaceSyncPromosWithSignInPromos);
  ASSERT_FALSE(pref_service()->GetBoolean(prefs::kAccountStorageNoticeShown));

  SignIn();

  EXPECT_TRUE(pref_service()->GetBoolean(prefs::kAccountStorageNoticeShown));
}

TEST_F(PasswordDataTypeControllerTest,
       DoNotSetNoticePrefOnSigninIfSyncToSigninDisabled) {
  base::test::ScopedFeatureList disable_sync_to_signin;
  disable_sync_to_signin.InitAndDisableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);
  ASSERT_FALSE(pref_service()->GetBoolean(prefs::kAccountStorageNoticeShown));

  SignIn();

  EXPECT_FALSE(pref_service()->GetBoolean(prefs::kAccountStorageNoticeShown));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace password_manager
