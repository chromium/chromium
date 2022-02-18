// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_backend_migration_decorator.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/mock_password_store_backend.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::Return;
using ::testing::WithArg;
using ::testing::WithArgs;

}  // namespace

class PasswordStoreBackendMigrationDecoratorTest : public testing::Test {
 protected:
  PasswordStoreBackendMigrationDecoratorTest() {
    prefs_.registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);

    feature_list_.InitAndEnableFeatureWithParameters(
        /*enabled_feature=*/features::kUnifiedPasswordManagerMigration,
        {{"migration_version", "1"}});

    backend_migration_decorator_ =
        std::make_unique<PasswordStoreBackendMigrationDecorator>(
            CreateBuiltInBackend(), CreateAndroidBackend(), &prefs_,
            &sync_delegate_);
  }

  ~PasswordStoreBackendMigrationDecoratorTest() override {
    backend_migration_decorator()->Shutdown(base::DoNothing());
  }

  MockPasswordBackendSyncDelegate& sync_delegate() { return sync_delegate_; }
  PasswordStoreBackend* backend_migration_decorator() {
    return backend_migration_decorator_.get();
  }
  MockPasswordStoreBackend* built_in_backend() { return built_in_backend_; }
  MockPasswordStoreBackend* android_backend() { return android_backend_; }

  TestingPrefServiceSimple& prefs() { return prefs_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  std::unique_ptr<PasswordStoreBackend> CreateBuiltInBackend() {
    auto unique_backend = std::make_unique<MockPasswordStoreBackend>();
    built_in_backend_ = unique_backend.get();
    return unique_backend;
  }

  std::unique_ptr<PasswordStoreBackend> CreateAndroidBackend() {
    auto unique_backend = std::make_unique<MockPasswordStoreBackend>();
    android_backend_ = unique_backend.get();
    return unique_backend;
  }

  base::test::SingleThreadTaskEnvironment task_env_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
  MockPasswordBackendSyncDelegate sync_delegate_;
  raw_ptr<MockPasswordStoreBackend> built_in_backend_;
  raw_ptr<MockPasswordStoreBackend> android_backend_;

  std::unique_ptr<PasswordStoreBackendMigrationDecorator>
      backend_migration_decorator_;
};

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       MigrationPreferenceClearedWhenSyncDisabled) {
  base::MockCallback<base::OnceCallback<void(bool)>> mock_completion_callback;
  base::RepeatingClosure sync_status_changed_closure;

  // Set up pref to indicate that initial migration is finished.
  prefs().SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 2);

  EXPECT_CALL(mock_completion_callback, Run(/*success=*/true));

  EXPECT_CALL(*built_in_backend(), InitBackend)
      .WillOnce(WithArgs<1, 2>(
          [&sync_status_changed_closure](auto sync_status_changed,
                                         auto completion_callback) {
            std::move(completion_callback).Run(/*success=*/true);
            // Capture |sync_enabled_or_disabled_cb| passed to the
            // build_in_backend.
            sync_status_changed_closure = std::move(sync_status_changed);
          }));
  EXPECT_CALL(*android_backend(), InitBackend)
      .WillOnce(WithArg<2>([](auto completion_callback) {
        std::move(completion_callback).Run(/*success=*/true);
      }));

  backend_migration_decorator()->InitBackend(
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/mock_completion_callback.Get());

  // Invoke sync callback to simulate a change in sync status. Set expectation
  // for sync to be turned off.
  EXPECT_CALL(sync_delegate(), IsSyncingPasswordsEnabled)
      .WillOnce(Return(false));
  sync_status_changed_closure.Run();

  RunUntilIdle();

  // Since sync was disabled pref value for migration version should be reset.
  EXPECT_EQ(0, prefs().GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
}

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       LocalAndroidPasswordsClearedWhenSyncEnabled) {
  base::MockCallback<base::OnceCallback<void(bool)>> mock_completion_callback;
  base::RepeatingClosure sync_status_changed_closure;

  EXPECT_CALL(mock_completion_callback, Run(/*success=*/true));

  EXPECT_CALL(*built_in_backend(), InitBackend)
      .WillOnce(WithArgs<1, 2>(
          [&sync_status_changed_closure](auto sync_status_changed,
                                         auto completion_callback) {
            std::move(completion_callback).Run(/*success=*/true);
            // Capture |sync_enabled_or_disabled_cb| passed to the
            // build_in_backend.
            sync_status_changed_closure = std::move(sync_status_changed);
          }));
  EXPECT_CALL(*android_backend(), InitBackend)
      .WillOnce(WithArg<2>([](auto completion_callback) {
        std::move(completion_callback).Run(/*success=*/true);
      }));

  backend_migration_decorator()->InitBackend(
      /*remote_form_changes_received=*/base::DoNothing(),
      /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
      /*completion=*/mock_completion_callback.Get());

  // Invoke sync callback to simulate a change in sync status. Set expectation
  // for sync to be turned off.
  EXPECT_CALL(sync_delegate(), IsSyncingPasswordsEnabled)
      .WillOnce(Return(true));
  EXPECT_CALL(*android_backend(), ClearAllLocalPasswords);
  sync_status_changed_closure.Run();

  RunUntilIdle();
}

}  // namespace password_manager
