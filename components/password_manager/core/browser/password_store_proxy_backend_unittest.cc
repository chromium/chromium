// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_proxy_backend.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/mock_password_store_backend.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {
namespace {

using features::UpmExperimentVariation;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Optional;
using ::testing::Pointer;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::VariantWith;
using ::testing::WithArg;
using Type = PasswordStoreChange::Type;
using RemoveChangesReceived = PasswordStoreBackend::RemoteChangesReceived;

const PasswordStoreBackendError kUnrecoverableError = PasswordStoreBackendError(
    PasswordStoreBackendErrorType::kUncategorized,
    PasswordStoreBackendErrorRecoveryType::kUnrecoverable);
const PasswordStoreBackendError kRecoverableError = PasswordStoreBackendError(
    PasswordStoreBackendErrorType::kUncategorized,
    PasswordStoreBackendErrorRecoveryType::kRecoverable);

PasswordForm CreateTestForm() {
  PasswordForm form;
  form.username_value = u"Todd Tester";
  form.password_value = u"S3cr3t";
  form.url = GURL(u"https://example.com");
  form.is_public_suffix_match = false;
  form.is_affiliation_based_match = false;
  return form;
}

std::vector<std::unique_ptr<PasswordForm>> CreateTestLogins() {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateEntry("Todd Tester", "S3cr3t",
                              GURL(u"https://example.com"),
                              /*is_psl_match=*/false,
                              /*is_affiliation_based_match=*/false));
  forms.push_back(CreateEntry("Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
                              GURL(u"https://m.example.com"),
                              /*is_psl_match=*/true,
                              /*is_affiliation_based_match=*/false));
  return forms;
}

bool FilterNoUrl(const GURL& gurl) {
  return true;
}

MATCHER_P(PasswordChangesAre, expectations, "") {
  if (absl::holds_alternative<PasswordStoreBackendError>(arg)) {
    return false;
  }

  auto changes = absl::get<PasswordChanges>(arg);
  if (!changes.has_value()) {
    return false;
  }

  return changes.value() == expectations;
}

std::string GetFallbackHistogramNameForMethodName(std::string method_name) {
  return base::StrCat(
      {"PasswordManager.PasswordStoreProxyBackend.", method_name, ".Fallback"});
}

}  // namespace

class PasswordStoreProxyBackendTest : public testing::Test {
 protected:
  PasswordStoreProxyBackendTest() {
    proxy_backend_ = std::make_unique<PasswordStoreProxyBackend>(
        &built_in_backend_, &android_backend_, &prefs_);

    feature_list_.InitAndEnableFeatureWithParameters(
        features::kUnifiedPasswordManagerAndroid, {{"stage", "1"}});

    prefs_.registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kUnenrolledFromGoogleMobileServicesDueToErrors, false);

    // Initialize sync service.
    EXPECT_CALL(android_backend(), OnSyncServiceInitialized(&sync_service_));
    proxy_backend().OnSyncServiceInitialized(&sync_service_);
  }

  void TearDown() override {
    EXPECT_CALL(android_backend_, Shutdown(_));
    EXPECT_CALL(built_in_backend_, Shutdown(_));
    PasswordStoreBackend* backend = proxy_backend_.get();  // Will be destroyed.
    backend->Shutdown(base::DoNothing());
    proxy_backend_.reset();
  }

  void EnablePasswordSync() {
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, {syncer::UserSelectableType::kPasswords});
  }

  void DisablePasswordSync() {
    sync_service_.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, /*types=*/{});
  }

  PasswordStoreBackend& proxy_backend() { return *proxy_backend_; }
  MockPasswordStoreBackend& built_in_backend() { return built_in_backend_; }
  MockPasswordStoreBackend& android_backend() { return android_backend_; }
  TestingPrefServiceSimple* prefs() { return &prefs_; }
  syncer::TestSyncService* sync_service() { return &sync_service_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<PasswordStoreProxyBackend> proxy_backend_;
  StrictMock<MockPasswordStoreBackend> built_in_backend_;
  StrictMock<MockPasswordStoreBackend> android_backend_;
  syncer::TestSyncService sync_service_;
};

TEST_F(PasswordStoreProxyBackendTest, CallCompletionCallbackAfterInit) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;

  // Both backends need to be invoked for a successful completion call.
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(
          WithArg<2>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(true);
          })));
  EXPECT_CALL(android_backend(), InitBackend)
      .WillOnce(
          WithArg<2>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(true);
          })));
  EXPECT_CALL(completion_callback, Run(true));
  proxy_backend().InitBackend(base::DoNothing(), base::DoNothing(),
                              completion_callback.Get());
}

TEST_F(PasswordStoreProxyBackendTest, CallCompletionWithFailureForAnyError) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;

  // If one backend fails to initialize, the result of the second is irrelevant.
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(
          WithArg<2>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(false);
          })));
  EXPECT_CALL(android_backend(), InitBackend)
      .Times(AtMost(1))
      .WillOnce(
          WithArg<2>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(true);
          })));
  EXPECT_CALL(completion_callback, Run(false));
  proxy_backend().InitBackend(base::DoNothing(), base::DoNothing(),
                              completion_callback.Get());
}

TEST_F(PasswordStoreProxyBackendTest, CallRemoteChangesOnlyForMainBackend) {
  // Use the rollout stage which changes the main backend with the sync status.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid, {{"stage", "0"}});
  base::MockCallback<RemoveChangesReceived> original_callback;

  // Both backends receive a callback that they trigger for new remote changes.
  RemoveChangesReceived built_in_remote_changes_callback;
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(SaveArg<0>(&built_in_remote_changes_callback));
  RemoveChangesReceived android_remote_changes_callback;
  EXPECT_CALL(android_backend(), InitBackend)
      .WillOnce(SaveArg<0>(&android_remote_changes_callback));
  proxy_backend().InitBackend(original_callback.Get(), base::DoNothing(),
                              base::DoNothing());

  // With sync enabled, only the android backend calls the original callback.
  EnablePasswordSync();
  EXPECT_CALL(original_callback, Run);
  android_remote_changes_callback.Run(absl::nullopt);
  testing::Mock::VerifyAndClearExpectations(&original_callback);

  EXPECT_CALL(original_callback, Run).Times(0);
  built_in_remote_changes_callback.Run(absl::nullopt);
  testing::Mock::VerifyAndClearExpectations(&original_callback);

  // As soon as sync is disabled, only the built-in backend calls the original
  // callback. The callbacks are stable. No new Init call is necessary.
  DisablePasswordSync();

  EXPECT_CALL(original_callback, Run).Times(0);
  android_remote_changes_callback.Run(absl::nullopt);
  testing::Mock::VerifyAndClearExpectations(&original_callback);

  EXPECT_CALL(original_callback, Run);
  built_in_remote_changes_callback.Run(absl::nullopt);
}

TEST_F(PasswordStoreProxyBackendTest, CallSyncCallbackOnlyForBuiltInBackend) {
  // Use the rollout stage which changes the main backend with the sync status.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid, {{"stage", "0"}});
  base::MockCallback<base::RepeatingClosure> original_callback;

  // Both backends receive a callback that they trigger for new remote changes.
  base::RepeatingClosure built_in_sync_callback;
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(SaveArg<1>(&built_in_sync_callback));
  base::RepeatingClosure android_sync_callback;
  EXPECT_CALL(android_backend(), InitBackend)
      .WillOnce(SaveArg<1>(&android_sync_callback));
  proxy_backend().InitBackend(base::DoNothing(), original_callback.Get(),
                              base::DoNothing());

  // With sync enabled, only the built-in backend calls the original callback.
  EnablePasswordSync();

  EXPECT_CALL(original_callback, Run).Times(0);
  android_sync_callback.Run();
  testing::Mock::VerifyAndClearExpectations(&original_callback);

  EXPECT_CALL(original_callback, Run);
  built_in_sync_callback.Run();
  testing::Mock::VerifyAndClearExpectations(&original_callback);

  // With sync is disabled, the built-in backend remains the only to call the
  // original callback.
  DisablePasswordSync();

  EXPECT_CALL(original_callback, Run).Times(0);
  android_sync_callback.Run();
  testing::Mock::VerifyAndClearExpectations(&original_callback);

  EXPECT_CALL(original_callback, Run);
  built_in_sync_callback.Run();
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToGetAllLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  EXPECT_CALL(built_in_backend(), GetAllLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  EnablePasswordSync();
  EXPECT_CALL(android_backend(), GetAllLoginsAsync);
  proxy_backend().GetAllLoginsAsync(mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToGetAutofillableLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  EXPECT_CALL(built_in_backend(), GetAutofillableLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  EnablePasswordSync();
  EXPECT_CALL(android_backend(), GetAutofillableLoginsAsync);
  proxy_backend().GetAutofillableLoginsAsync(mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToFillMatchingLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  EXPECT_CALL(built_in_backend(), FillMatchingLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  EnablePasswordSync();
  EXPECT_CALL(android_backend(), FillMatchingLoginsAsync);
  proxy_backend().FillMatchingLoginsAsync(mock_reply.Get(),
                                          /*include_psl=*/false,
                                          std::vector<PasswordFormDigest>());
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToAddLoginAsync) {
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::ADD, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));
  // This test doesn't care about the shadow backend.
  EXPECT_CALL(android_backend(), AddLoginAsync).Times(AnyNumber());
  EXPECT_CALL(built_in_backend(), AddLoginAsync(Eq(form), _))
      .WillOnce(WithArg<1>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  proxy_backend().AddLoginAsync(form, mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToUpdateLoginAsync) {
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::UPDATE, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));
  // This test doesn't care about the shadow backend.
  EXPECT_CALL(android_backend(), UpdateLoginAsync).Times(AnyNumber());
  EXPECT_CALL(built_in_backend(), UpdateLoginAsync(Eq(form), _))
      .WillOnce(WithArg<1>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  proxy_backend().UpdateLoginAsync(form, mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToRemoveLoginAsync) {
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::REMOVE, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));
  // This test doesn't care about the shadow backend.
  EXPECT_CALL(android_backend(), RemoveLoginAsync).Times(AnyNumber());
  EXPECT_CALL(built_in_backend(), RemoveLoginAsync(Eq(form), _))
      .WillOnce(WithArg<1>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  proxy_backend().RemoveLoginAsync(form, mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToRemoveLoginsByURLAndTimeAsync) {
  base::Time kStart = base::Time::FromTimeT(111111);
  base::Time kEnd = base::Time::FromTimeT(22222222);
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::REMOVE, CreateTestForm()));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));
  // This test doesn't care about the shadow backend.
  EXPECT_CALL(android_backend(), RemoveLoginsByURLAndTimeAsync)
      .Times(AnyNumber());
  EXPECT_CALL(built_in_backend(),
              RemoveLoginsByURLAndTimeAsync(_, Eq(kStart), Eq(kEnd), _, _))
      .WillOnce(WithArg<4>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  proxy_backend().RemoveLoginsByURLAndTimeAsync(
      base::BindRepeating(&FilterNoUrl), kStart, kEnd, base::DoNothing(),
      mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToRemoveLoginsCreatedBetweenAsync) {
  base::Time kStart = base::Time::FromTimeT(111111);
  base::Time kEnd = base::Time::FromTimeT(22222222);
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::REMOVE, CreateTestForm()));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));
  // This test doesn't care about the shadow backend.
  EXPECT_CALL(android_backend(), RemoveLoginsCreatedBetweenAsync)
      .Times(AnyNumber());
  EXPECT_CALL(built_in_backend(),
              RemoveLoginsCreatedBetweenAsync(Eq(kStart), Eq(kEnd), _))
      .WillOnce(WithArg<2>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  proxy_backend().RemoveLoginsCreatedBetweenAsync(kStart, kEnd,
                                                  mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToDisableAutoSignInForOriginsAsync) {
  base::MockCallback<base::OnceClosure> mock_reply;
  EXPECT_CALL(mock_reply, Run);
  // This test doesn't care about the shadow backend.
  EXPECT_CALL(android_backend(), DisableAutoSignInForOriginsAsync)
      .Times(AnyNumber());
  EXPECT_CALL(built_in_backend(), DisableAutoSignInForOriginsAsync)
      .WillOnce(WithArg<1>(
          Invoke([](base::OnceClosure reply) { std::move(reply).Run(); })));
  proxy_backend().DisableAutoSignInForOriginsAsync(
      base::BindRepeating(&FilterNoUrl), mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToGetSmartBubbleStatsStore) {
  EXPECT_CALL(built_in_backend(), GetSmartBubbleStatsStore);
  proxy_backend().GetSmartBubbleStatsStore();
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToGetFieldInfoStore) {
  EXPECT_CALL(built_in_backend(), GetFieldInfoStore);
  proxy_backend().GetFieldInfoStore();
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowGetAllLoginsAsyncWithoutSyncOrMigration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "2"}, {"stage", "1"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

  base::HistogramTester histogram_tester;
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  EXPECT_CALL(built_in_backend(), GetAllLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  DisablePasswordSync();
  EXPECT_CALL(android_backend(), GetAllLoginsAsync).Times(0);
  proxy_backend().GetAllLoginsAsync(mock_reply.Get());

  std::string prefix =
      "PasswordManager.PasswordStoreProxyBackend.GetAllLoginsAsync.";

  histogram_tester.ExpectTotalCount(prefix + "Diff.Abs", 0);
  histogram_tester.ExpectTotalCount(prefix + "MainMinusShadow.Abs", 0);
  histogram_tester.ExpectTotalCount(prefix + "ShadowMinusMain.Abs", 0);
  histogram_tester.ExpectTotalCount(prefix + "InconsistentPasswords.Abs", 0);
  histogram_tester.ExpectTotalCount(prefix + "Diff.Rel", 0);
  histogram_tester.ExpectTotalCount(prefix + "MainMinusShadow.Rel", 0);
  histogram_tester.ExpectTotalCount(prefix + "ShadowMinusMain.Rel", 0);
  histogram_tester.ExpectTotalCount(prefix + "InconsistentPasswords.Rel", 0);
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowGetAutofillableLoginsAsyncWithoutSyncOrMigration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "2"}, {"stage", "1"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);
  DisablePasswordSync();

  EXPECT_CALL(built_in_backend(), GetAutofillableLoginsAsync);
  EXPECT_CALL(android_backend(), GetAutofillableLoginsAsync).Times(0);
  proxy_backend().GetAutofillableLoginsAsync(/*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowFillMatchingLoginsAsyncWithoutSyncOrMigration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "2"}, {"stage", "1"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);
  DisablePasswordSync();

  EXPECT_CALL(built_in_backend(), FillMatchingLoginsAsync);
  EXPECT_CALL(android_backend(), FillMatchingLoginsAsync).Times(0);
  proxy_backend().FillMatchingLoginsAsync(/*callback=*/base::DoNothing(),
                                          /*include_psl=*/false,
                                          std::vector<PasswordFormDigest>());
}

TEST_F(PasswordStoreProxyBackendTest, NoShadowAddLoginAsyncWhenSyncEnabled) {
  EnablePasswordSync();

  EXPECT_CALL(built_in_backend(), AddLoginAsync);
  EXPECT_CALL(android_backend(), AddLoginAsync).Times(0);
  proxy_backend().AddLoginAsync(CreateTestForm(),
                                /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowAddLoginAsyncWhenSyncDisabledAndInitialMigrationIncomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "2"}, {"stage", "1"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

  DisablePasswordSync();

  EXPECT_CALL(built_in_backend(), AddLoginAsync);
  EXPECT_CALL(android_backend(), AddLoginAsync).Times(0);
  proxy_backend().AddLoginAsync(CreateTestForm(),
                                /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest, NoShadowUpdateLoginAsyncWhenSyncEnabled) {
  EnablePasswordSync();

  EXPECT_CALL(built_in_backend(), UpdateLoginAsync);
  EXPECT_CALL(android_backend(), UpdateLoginAsync).Times(0);
  proxy_backend().UpdateLoginAsync(CreateTestForm(),
                                   /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowUpdateLoginAsyncWhenSyncDisabledAndInitialMigrationIncomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "2"}, {"stage", "1"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

  DisablePasswordSync();

  EXPECT_CALL(built_in_backend(), UpdateLoginAsync);
  EXPECT_CALL(android_backend(), UpdateLoginAsync).Times(0);
  proxy_backend().UpdateLoginAsync(CreateTestForm(),
                                   /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest, NoShadowRemoveLoginAsyncWhenSyncEnabled) {
  EnablePasswordSync();

  EXPECT_CALL(built_in_backend(), RemoveLoginAsync);
  EXPECT_CALL(android_backend(), RemoveLoginAsync).Times(0);
  proxy_backend().RemoveLoginAsync(CreateTestForm(),
                                   /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowRemoveLoginAsyncWhenSyncDisabledAndInitialMigrationIncomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "2"}, {"stage", "1"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

  DisablePasswordSync();

  EXPECT_CALL(built_in_backend(), RemoveLoginAsync);
  EXPECT_CALL(android_backend(), RemoveLoginAsync).Times(0);
  proxy_backend().RemoveLoginAsync(CreateTestForm(),
                                   /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       ShadowRemoveLoginAsyncWhenSyncEnabledAtEnabledForSyncingUsersStage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});

  EnablePasswordSync();

  EXPECT_CALL(built_in_backend(), RemoveLoginAsync);
  EXPECT_CALL(android_backend(), RemoveLoginAsync);
  proxy_backend().RemoveLoginAsync(CreateTestForm(),
                                   /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowRemoveLoginsByURLAndTimeAsyncWhenSyncEnabled) {
  EnablePasswordSync();

  EXPECT_CALL(built_in_backend(), RemoveLoginsByURLAndTimeAsync);
  EXPECT_CALL(android_backend(), RemoveLoginsByURLAndTimeAsync).Times(0);
  proxy_backend().RemoveLoginsByURLAndTimeAsync(
      base::BindRepeating(&FilterNoUrl),
      /*delete_begin=*/base::Time::FromTimeT(111111),
      /*delete_end=*/base::Time::FromTimeT(22222222),
      /*sync_completion=*/base::OnceCallback<void(bool)>(),
      /*callback=*/base::DoNothing());
}

TEST_F(
    PasswordStoreProxyBackendTest,
    NoShadowRemoveLoginsByURLAndTimeAsyncWhenSyncDisabledAndInitialMigrationIncomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "2"}, {"stage", "1"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

  DisablePasswordSync();

  EXPECT_CALL(built_in_backend(), RemoveLoginsByURLAndTimeAsync);
  EXPECT_CALL(android_backend(), RemoveLoginsByURLAndTimeAsync).Times(0);
  proxy_backend().RemoveLoginsByURLAndTimeAsync(
      base::BindRepeating(&FilterNoUrl),
      /*delete_begin=*/base::Time::FromTimeT(111111),
      /*delete_end=*/base::Time::FromTimeT(22222222),
      /*sync_completion=*/base::OnceCallback<void(bool)>(),
      /*callback=*/base::DoNothing());
}

TEST_F(
    PasswordStoreProxyBackendTest,
    ShadowRemoveLoginsByURLAndTimeAsyncWhenSyncEnabledAtEnabledForSyncingUsersStage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});

  EnablePasswordSync();

  EXPECT_CALL(built_in_backend(), RemoveLoginsByURLAndTimeAsync);
  EXPECT_CALL(android_backend(), RemoveLoginsByURLAndTimeAsync);
  proxy_backend().RemoveLoginsByURLAndTimeAsync(
      base::BindRepeating(&FilterNoUrl),
      /*delete_begin=*/base::Time::FromTimeT(111111),
      /*delete_end=*/base::Time::FromTimeT(22222222),
      /*sync_completion=*/base::OnceCallback<void(bool)>(),
      /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowRemoveLoginsCreatedBetweenAsyncWhenSyncEnabled) {
  EnablePasswordSync();

  EXPECT_CALL(built_in_backend(), RemoveLoginsCreatedBetweenAsync);
  EXPECT_CALL(android_backend(), RemoveLoginsCreatedBetweenAsync).Times(0);
  proxy_backend().RemoveLoginsCreatedBetweenAsync(
      /*delete_begin=*/base::Time::FromTimeT(111111),
      /*delete_end=*/base::Time::FromTimeT(22222222),
      /*callback=*/base::DoNothing());
}

TEST_F(
    PasswordStoreProxyBackendTest,
    NoShadowRemoveLoginsCreatedBetweenAsyncWhenSyncDisabledAndInitialMigrationIncomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "2"}, {"stage", "1"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

  DisablePasswordSync();

  EXPECT_CALL(built_in_backend(), RemoveLoginsCreatedBetweenAsync);
  EXPECT_CALL(android_backend(), RemoveLoginsCreatedBetweenAsync).Times(0);
  proxy_backend().RemoveLoginsCreatedBetweenAsync(
      /*delete_begin=*/base::Time::FromTimeT(111111),
      /*delete_end=*/base::Time::FromTimeT(22222222),
      /*callback=*/base::DoNothing());
}

TEST_F(
    PasswordStoreProxyBackendTest,
    ShadowRemoveLoginsCreatedBetweenAsyncWhenSyncEnabledAtEnabledForSyncingUsersStage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});

  EnablePasswordSync();

  EXPECT_CALL(built_in_backend(), RemoveLoginsCreatedBetweenAsync);
  EXPECT_CALL(android_backend(), RemoveLoginsCreatedBetweenAsync);
  proxy_backend().RemoveLoginsCreatedBetweenAsync(
      /*delete_begin=*/base::Time::FromTimeT(111111),
      /*delete_end=*/base::Time::FromTimeT(22222222),
      /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowDisableAutoSignInForOriginsAsyncWhenSyncEnabled) {
  EnablePasswordSync();

  EXPECT_CALL(built_in_backend(), DisableAutoSignInForOriginsAsync);
  EXPECT_CALL(android_backend(), DisableAutoSignInForOriginsAsync).Times(0);
  proxy_backend().DisableAutoSignInForOriginsAsync(
      base::BindRepeating(&FilterNoUrl), /*completion=*/base::DoNothing());
}

TEST_F(
    PasswordStoreProxyBackendTest,
    NoShadowDisableAutoSignInForOriginsAsyncWhenSyncDisabledAndInitialMigrationIncomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "2"}, {"stage", "1"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

  DisablePasswordSync();

  EXPECT_CALL(built_in_backend(), DisableAutoSignInForOriginsAsync);
  EXPECT_CALL(android_backend(), DisableAutoSignInForOriginsAsync).Times(0);
  proxy_backend().DisableAutoSignInForOriginsAsync(
      base::BindRepeating(&FilterNoUrl), /*completion=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       OnSyncServiceInitializedPropagatedToAndroidBackend) {
  syncer::TestSyncService sync_service;
  EXPECT_CALL(android_backend(), OnSyncServiceInitialized(&sync_service));
  proxy_backend().OnSyncServiceInitialized(&sync_service);
}

TEST_F(PasswordStoreProxyBackendTest,
       UsesAndroidBackendAsMainBackendPasswordSyncDisabledInSettings) {
  base::test::ScopedFeatureList feature_list;
  // Enable UPM for syncing users only.
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid, {{"stage", "2"}});

  // Imitate password sync being disabled in settings.
  DisablePasswordSync();

  // Verify that android backend is not used.
  EXPECT_CALL(android_backend(), GetAllLoginsAsync).Times(0);
  EXPECT_CALL(built_in_backend(), GetAllLoginsAsync);
  proxy_backend().GetAllLoginsAsync(base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       UsesBuiltInBackendAsMainBackendToGetLoginsUserUnenrolledFromUPM) {
  base::test::ScopedFeatureList feature_list;
  // Enable UPM for syncing users only.
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid, {{"stage", "2"}});
  prefs()->SetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
                      true);

  EnablePasswordSync();

  // Logins should be retrieved from the built-in backend.
  EXPECT_CALL(built_in_backend(), GetAllLoginsAsync);
  // Verify that android backend is not used.
  EXPECT_CALL(android_backend(), GetAllLoginsAsync).Times(0);

  proxy_backend().GetAllLoginsAsync(base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       UsesBuiltInBackendAsMainBackendToAddLoginUserUnenrolledFromUPM) {
  base::test::ScopedFeatureList feature_list;
  // Enable UPM for syncing users only.
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid, {{"stage", "2"}});
  prefs()->SetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
                      true);

  EnablePasswordSync();

  // Logins should be added to the built-in backend.
  EXPECT_CALL(built_in_backend(), AddLoginAsync);
  // Verify that android backend is not used.
  EXPECT_CALL(android_backend(), AddLoginAsync).Times(0);

  proxy_backend().AddLoginAsync(CreateTestForm(), base::DoNothing());
}

struct FallbackParam {
  PasswordStoreBackendError error;
  bool should_fallback;
};

class PasswordStoreProxyBackendTestWithFallbackParam
    : public PasswordStoreProxyBackendTest,
      public testing::WithParamInterface<FallbackParam> {
 protected:
  constexpr bool ShouldFallbackOnParam(const FallbackParam& p) {
    return p.should_fallback && p.error == kUnrecoverableError;
  }

  void InitFeatureListWithFallbackEnableFeatureParam(
      base::test::ScopedFeatureList& feature_list,
      base::FeatureParam<bool> fallback_param) {
    feature_list.InitAndEnableFeatureWithParameters(
        password_manager::features::kUnifiedPasswordManagerAndroid,
        {{"stage", "3"},
         {fallback_param.name, GetParam().should_fallback ? "true" : "false"}});
  }
};

TEST_P(PasswordStoreProxyBackendTestWithFallbackParam,
       RetriesAddLoginOnBuiltInBackend) {
  const FallbackParam& p = GetParam();

  base::test::ScopedFeatureList feature_list;
  InitFeatureListWithFallbackEnableFeatureParam(
      feature_list, password_manager::features::kFallbackOnModifyingOperations);

  base::HistogramTester histogram_tester;

  EnablePasswordSync();

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;

  EXPECT_CALL(android_backend(), AddLoginAsync)
      .WillOnce(WithArg<1>(
          Invoke([&p](auto reply) -> void { std::move(reply).Run(p.error); })));
  const PasswordStoreChangeList changes = {
      PasswordStoreChange(PasswordStoreChange::Type::ADD, CreateTestForm())};
  if (ShouldFallbackOnParam(p)) {
    EXPECT_CALL(built_in_backend(), AddLoginAsync)
        .WillOnce(WithArg<1>(Invoke([&changes](auto reply) -> void {
          std::move(reply).Run(changes);
        })));
    // Check that caller doesn't receive an error from android backend.
    EXPECT_CALL(mock_reply, Run(PasswordChangesAre(changes)));
  } else {
    EXPECT_CALL(built_in_backend(), AddLoginAsync).Times(0);
    // Check that caller gets an error from android backend.
    EXPECT_CALL(mock_reply, Run(PasswordChangesOrError(p.error)));
  }
  proxy_backend().AddLoginAsync(CreateTestForm(), mock_reply.Get());

  if (ShouldFallbackOnParam(p)) {
    histogram_tester.ExpectUniqueSample(
        GetFallbackHistogramNameForMethodName("AddLoginAsync"), true, 1);
  }
}

TEST_P(PasswordStoreProxyBackendTestWithFallbackParam,
       RetriesUpdateLoginOnBuiltInBackend) {
  const FallbackParam& p = GetParam();

  base::test::ScopedFeatureList feature_list;
  InitFeatureListWithFallbackEnableFeatureParam(
      feature_list, password_manager::features::kFallbackOnModifyingOperations);

  base::HistogramTester histogram_tester;

  EnablePasswordSync();

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;

  EXPECT_CALL(android_backend(), UpdateLoginAsync)
      .WillOnce(WithArg<1>(
          Invoke([&p](auto reply) -> void { std::move(reply).Run(p.error); })));
  const PasswordStoreChangeList changes = {
      PasswordStoreChange(PasswordStoreChange::Type::ADD, CreateTestForm())};
  if (ShouldFallbackOnParam(p)) {
    EXPECT_CALL(built_in_backend(), UpdateLoginAsync)
        .WillOnce(WithArg<1>(Invoke([&changes](auto reply) -> void {
          std::move(reply).Run(changes);
        })));
    // Check that caller doesn't receive an error from android backend.
    EXPECT_CALL(mock_reply, Run(PasswordChangesAre(changes)));
  } else {
    EXPECT_CALL(built_in_backend(), UpdateLoginAsync).Times(0);
    // Check that caller gets an error from android backend.
    EXPECT_CALL(mock_reply, Run(PasswordChangesOrError(p.error)));
  }
  proxy_backend().UpdateLoginAsync(CreateTestForm(), mock_reply.Get());

  if (ShouldFallbackOnParam(p)) {
    histogram_tester.ExpectUniqueSample(
        GetFallbackHistogramNameForMethodName("UpdateLoginAsync"), true, 1);
  }
}

TEST_P(PasswordStoreProxyBackendTestWithFallbackParam,
       RetriesGetAllLoginsOnBuiltInBackend) {
  const FallbackParam& p = GetParam();

  base::test::ScopedFeatureList feature_list;
  InitFeatureListWithFallbackEnableFeatureParam(
      feature_list,
      password_manager::features::kFallbackOnNonUserAffectingReadOperations);

  base::HistogramTester histogram_tester;

  EnablePasswordSync();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();

  EXPECT_CALL(android_backend(), GetAllLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([&p](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(p.error);
      })));
  if (ShouldFallbackOnParam(p)) {
    EXPECT_CALL(built_in_backend(), GetAllLoginsAsync)
        .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
          std::move(reply).Run(CreateTestLogins());
        })));
    // Check that caller doesn't receive an error from android backend.
    EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  } else {
    EXPECT_CALL(built_in_backend(), GetAllLoginsAsync).Times(0);
    // Check that caller gets an error from android backend.
    EXPECT_CALL(mock_reply,
                Run(VariantWith<PasswordStoreBackendError>(p.error)));
  }
  proxy_backend().GetAllLoginsAsync(mock_reply.Get());

  if (ShouldFallbackOnParam(p)) {
    histogram_tester.ExpectUniqueSample(
        GetFallbackHistogramNameForMethodName("GetAllLoginsAsync"), true, 1);
  }
}

TEST_P(PasswordStoreProxyBackendTestWithFallbackParam,
       RetriesGetAutofillableLoginsOnBuiltInBackend) {
  const FallbackParam& p = GetParam();

  base::test::ScopedFeatureList feature_list;
  InitFeatureListWithFallbackEnableFeatureParam(
      feature_list,
      password_manager::features::kFallbackOnNonUserAffectingReadOperations);

  base::HistogramTester histogram_tester;

  EnablePasswordSync();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();

  EXPECT_CALL(android_backend(), GetAutofillableLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([&p](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(p.error);
      })));
  if (ShouldFallbackOnParam(p)) {
    EXPECT_CALL(built_in_backend(), GetAutofillableLoginsAsync)
        .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
          std::move(reply).Run(CreateTestLogins());
        })));
    // Check that caller doesn't receive an error from android backend.
    EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  } else {
    EXPECT_CALL(built_in_backend(), GetAutofillableLoginsAsync).Times(0);
    // Check that caller gets an error from android backend.
    EXPECT_CALL(mock_reply,
                Run(VariantWith<PasswordStoreBackendError>(p.error)));
  }
  proxy_backend().GetAutofillableLoginsAsync(mock_reply.Get());
  {
    if (ShouldFallbackOnParam(p))
      histogram_tester.ExpectUniqueSample(
          GetFallbackHistogramNameForMethodName("GetAutofillableLoginsAsync"),
          true, 1);
  }
}

TEST_P(PasswordStoreProxyBackendTestWithFallbackParam,
       RetriesFillMatchingLoginsOnBuiltInBackend) {
  const FallbackParam& p = GetParam();

  base::test::ScopedFeatureList feature_list;
  InitFeatureListWithFallbackEnableFeatureParam(
      feature_list,
      password_manager::features::kFallbackOnUserAffectingReadOperations);

  base::HistogramTester histogram_tester;

  EnablePasswordSync();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();

  EXPECT_CALL(android_backend(), FillMatchingLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([&p](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(p.error);
      })));
  if (ShouldFallbackOnParam(p)) {
    EXPECT_CALL(built_in_backend(), FillMatchingLoginsAsync)
        .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
          std::move(reply).Run(CreateTestLogins());
        })));
    // Check that caller doesn't receive an error from android backend.
    EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  } else {
    EXPECT_CALL(built_in_backend(), FillMatchingLoginsAsync).Times(0);
    // Check that caller gets an error from android backend.
    EXPECT_CALL(mock_reply,
                Run(VariantWith<PasswordStoreBackendError>(p.error)));
  }
  proxy_backend().FillMatchingLoginsAsync(mock_reply.Get(),

                                          /*include_psl=*/false,
                                          std::vector<PasswordFormDigest>());
  if (ShouldFallbackOnParam(p)) {
    histogram_tester.ExpectUniqueSample(
        GetFallbackHistogramNameForMethodName("FillMatchingLoginsAsync"), true,
        1);
  }
}

TEST_P(PasswordStoreProxyBackendTestWithFallbackParam,
       RetriesRemoveLoginOnBuiltInBackend) {
  const FallbackParam& p = GetParam();

  base::test::ScopedFeatureList feature_list;
  InitFeatureListWithFallbackEnableFeatureParam(
      feature_list, password_manager::features::kFallbackOnRemoveOperations);

  base::HistogramTester histogram_tester;

  EnablePasswordSync();

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.emplace_back(Type::REMOVE, form);

  EXPECT_CALL(android_backend(), RemoveLoginAsync)
      .WillOnce(
          WithArg<1>(Invoke([&p](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(p.error);
          })));

  if (ShouldFallbackOnParam(p)) {
    EXPECT_CALL(built_in_backend(), RemoveLoginAsync)
        .WillOnce(WithArg<1>(Invoke([&change_list](auto reply) -> void {
          std::move(reply).Run(change_list);
        })));
    // Check that caller doesn't receive an error from android backend.
    EXPECT_CALL(mock_reply,
                Run(VariantWith<PasswordChanges>(Optional(change_list))));
  } else {
    EXPECT_CALL(built_in_backend(), RemoveLoginAsync).Times(0);
    // Check that caller gets an error from android backend.
    EXPECT_CALL(mock_reply, Run(PasswordChangesOrError(p.error)));
  }
  proxy_backend().RemoveLoginAsync(form, mock_reply.Get());

  if (ShouldFallbackOnParam(p)) {
    histogram_tester.ExpectUniqueSample(
        GetFallbackHistogramNameForMethodName("RemoveLoginAsync"), true, 1);
  }
}

TEST_P(PasswordStoreProxyBackendTestWithFallbackParam,
       RetriesRemoveLoginByURLAndTimeOnBuiltInBackend) {
  const FallbackParam& p = GetParam();

  base::test::ScopedFeatureList feature_list;
  InitFeatureListWithFallbackEnableFeatureParam(
      feature_list, password_manager::features::kFallbackOnRemoveOperations);

  base::HistogramTester histogram_tester;

  EnablePasswordSync();

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.emplace_back(Type::REMOVE, form);

  base::Time kStart = base::Time::FromTimeT(111111);
  base::Time kEnd = base::Time::FromTimeT(22222222);

  EXPECT_CALL(android_backend(), RemoveLoginsByURLAndTimeAsync)
      .WillOnce(
          WithArg<4>(Invoke([&p](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(p.error);
          })));

  if (ShouldFallbackOnParam(p)) {
    EXPECT_CALL(built_in_backend(), RemoveLoginsByURLAndTimeAsync)
        .WillOnce(WithArg<4>(Invoke([&change_list](auto reply) -> void {
          std::move(reply).Run(change_list);
        })));
    // Check that caller doesn't receive an error from android backend.
    EXPECT_CALL(mock_reply,
                Run(VariantWith<PasswordChanges>(Optional(change_list))));
  } else {
    EXPECT_CALL(built_in_backend(), RemoveLoginsByURLAndTimeAsync).Times(0);
    // Check that caller gets an error from android backend.
    EXPECT_CALL(mock_reply, Run(PasswordChangesOrError(p.error)));
  }
  proxy_backend().RemoveLoginsByURLAndTimeAsync(
      base::BindRepeating(&FilterNoUrl), kStart, kEnd, base::DoNothing(),
      mock_reply.Get());

  if (ShouldFallbackOnParam(p)) {
    histogram_tester.ExpectUniqueSample(
        GetFallbackHistogramNameForMethodName("RemoveLoginsByURLAndTimeAsync"),
        true, 1);
  }
}

TEST_P(PasswordStoreProxyBackendTestWithFallbackParam,
       RetriesRemoveLoginCreatedBetweenOnBuiltInBackend) {
  const FallbackParam& p = GetParam();

  base::test::ScopedFeatureList feature_list;
  InitFeatureListWithFallbackEnableFeatureParam(
      feature_list, password_manager::features::kFallbackOnRemoveOperations);

  base::HistogramTester histogram_tester;

  EnablePasswordSync();

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.emplace_back(Type::REMOVE, form);

  base::Time kStart = base::Time::FromTimeT(111111);
  base::Time kEnd = base::Time::FromTimeT(22222222);

  EXPECT_CALL(android_backend(), RemoveLoginsCreatedBetweenAsync)
      .WillOnce(
          WithArg<2>(Invoke([&p](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(p.error);
          })));

  if (ShouldFallbackOnParam(p)) {
    EXPECT_CALL(built_in_backend(), RemoveLoginsCreatedBetweenAsync)
        .WillOnce(WithArg<2>(Invoke([&change_list](auto reply) -> void {
          std::move(reply).Run(change_list);
        })));
    // Check that caller doesn't receive an error from android backend.
    EXPECT_CALL(mock_reply,
                Run(VariantWith<PasswordChanges>(Optional(change_list))));
  } else {
    EXPECT_CALL(built_in_backend(), RemoveLoginsCreatedBetweenAsync).Times(0);
    // Check that caller gets an error from android backend.
    EXPECT_CALL(mock_reply, Run(PasswordChangesOrError(p.error)));
  }
  proxy_backend().RemoveLoginsCreatedBetweenAsync(kStart, kEnd,
                                                  mock_reply.Get());

  if (ShouldFallbackOnParam(p)) {
    histogram_tester.ExpectUniqueSample(GetFallbackHistogramNameForMethodName(
                                            "RemoveLoginsCreatedBetweenAsync"),
                                        true, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    PasswordStoreProxyBackendTest,
    PasswordStoreProxyBackendTestWithFallbackParam,
    testing::Values(
        FallbackParam{.error = kUnrecoverableError, .should_fallback = true},
        FallbackParam{.error = kRecoverableError, .should_fallback = true},
        FallbackParam{.error = kRecoverableError, .should_fallback = false},
        FallbackParam{.error = kUnrecoverableError, .should_fallback = false}));

// Holds the main and shadow backend's logins and the expected number of common
// and different logins.
struct LoginsMetricsParam {
  struct Entry {
    std::unique_ptr<PasswordForm> ToPasswordForm() const {
      return CreateEntry(username, password, GURL(origin),
                         /*is_psl_match=*/false,
                         /*is_affiliation_based_match=*/false);
    }

    std::string username;
    std::string password;
    std::string origin;
  };

  std::vector<std::unique_ptr<PasswordForm>> GetMainLogins() const {
    std::vector<std::unique_ptr<PasswordForm>> v;
    base::ranges::transform(main_logins, std::back_inserter(v),
                            &Entry::ToPasswordForm);
    return v;
  }

  std::vector<std::unique_ptr<PasswordForm>> GetShadowLogins() const {
    std::vector<std::unique_ptr<PasswordForm>> v;
    base::ranges::transform(shadow_logins, std::back_inserter(v),
                            &Entry::ToPasswordForm);
    return v;
  }

  std::vector<Entry> main_logins;
  std::vector<Entry> shadow_logins;
  size_t in_common;     // Cardinality of `main_logins ∩ shadow_logins`.
  size_t in_main;       // Cardinality of `main_logins \ shadow_logins`.
  size_t in_shadow;     // Cardinality of `shadow_logins \ main_logins`.
  size_t inconsistent;  // Number of common logins that diffen in the passwords.
};

class PasswordStoreProxyBackendTestWithLoginsParams
    : public PasswordStoreProxyBackendTest,
      public testing::WithParamInterface<LoginsMetricsParam> {};

// Tests the metrics of GetAllLoginsAsync().
TEST_P(PasswordStoreProxyBackendTestWithLoginsParams,
       GetAllLoginsAsyncMetrics) {
  const LoginsMetricsParam& p = GetParam();
  base::HistogramTester histogram_tester;
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  {
    EXPECT_CALL(mock_reply, Run(_));
    EXPECT_CALL(built_in_backend(), GetAllLoginsAsync)
        .WillOnce(WithArg<0>(Invoke([&p](LoginsOrErrorReply reply) -> void {
          std::move(reply).Run(p.GetMainLogins());
        })));
    EnablePasswordSync();
    EXPECT_CALL(android_backend(), GetAllLoginsAsync)
        .WillOnce(WithArg<0>(Invoke([&p](LoginsOrErrorReply reply) -> void {
          std::move(reply).Run(p.GetShadowLogins());
        })));
    proxy_backend().GetAllLoginsAsync(mock_reply.Get());
  }

  std::string prefix =
      "PasswordManager.PasswordStoreProxyBackend.GetAllLoginsAsync.";

  histogram_tester.ExpectUniqueSample(prefix + "Diff.Abs",
                                      p.in_main + p.in_shadow, 1);
  histogram_tester.ExpectUniqueSample(prefix + "MainMinusShadow.Abs", p.in_main,
                                      1);
  histogram_tester.ExpectUniqueSample(prefix + "ShadowMinusMain.Abs",
                                      p.in_shadow, 1);
  histogram_tester.ExpectUniqueSample(prefix + "InconsistentPasswords.Abs",
                                      p.inconsistent, 1);

  // Returns ⌈nominator/denominator⌉.
  auto percentage = [](size_t nominator, size_t denominator) {
    return static_cast<size_t>(std::ceil(static_cast<double>(nominator) /
                                         static_cast<double>(denominator) *
                                         100.0l));
  };

  size_t total = p.in_common + p.in_main + p.in_shadow;
  if (total != 0) {
    histogram_tester.ExpectUniqueSample(
        prefix + "Diff.Rel", percentage(p.in_main + p.in_shadow, total), 1);
    histogram_tester.ExpectUniqueSample(prefix + "MainMinusShadow.Rel",
                                        percentage(p.in_main, total), 1);
    histogram_tester.ExpectUniqueSample(prefix + "ShadowMinusMain.Rel",
                                        percentage(p.in_shadow, total), 1);
  } else {
    histogram_tester.ExpectTotalCount(prefix + "Diff.Rel", 0);
    histogram_tester.ExpectTotalCount(prefix + "MainMinusShadow.Rel", 0);
    histogram_tester.ExpectTotalCount(prefix + "ShadowMinusMain.Rel", 0);
  }

  if (p.in_common != 0) {
    histogram_tester.ExpectUniqueSample(prefix + "InconsistentPasswords.Rel",
                                        percentage(p.inconsistent, p.in_common),
                                        1);
  } else {
    histogram_tester.ExpectTotalCount(prefix + "InconsistentPasswords.Rel", 0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    PasswordStoreProxyBackendTest,
    PasswordStoreProxyBackendTestWithLoginsParams,
    testing::Values(LoginsMetricsParam{.main_logins = {},
                                       .shadow_logins = {},
                                       .in_common = 0,
                                       .in_main = 0,
                                       .in_shadow = 0,
                                       .inconsistent = 0},
                    LoginsMetricsParam{
                        .main_logins = {{"user1", "pswd1", "https://a.com/"}},
                        .shadow_logins = {{"user1", "pswd1", "https://a.com/"}},
                        .in_common = 1,
                        .in_main = 0,
                        .in_shadow = 0,
                        .inconsistent = 0},
                    LoginsMetricsParam{
                        .main_logins = {{"user1", "pswd1", "https://a.com/"}},
                        .shadow_logins = {{"user1", "pswdX", "https://a.com/"}},
                        .in_common = 1,
                        .in_main = 0,
                        .in_shadow = 0,
                        .inconsistent = 1},
                    LoginsMetricsParam{
                        .main_logins = {{"user1", "pswd1", "https://a.com/"}},
                        .shadow_logins = {},
                        .in_common = 0,
                        .in_main = 1,
                        .in_shadow = 0,
                        .inconsistent = 0},
                    LoginsMetricsParam{
                        .main_logins = {},
                        .shadow_logins = {{"user1", "pswd1", "https://a.com/"}},
                        .in_common = 0,
                        .in_main = 0,
                        .in_shadow = 1,
                        .inconsistent = 0},
                    LoginsMetricsParam{
                        .main_logins = {{"user4", "pswd4", "https://d.com/"},
                                        {"user2", "pswd2", "https://b.com/"},
                                        {"user1", "pswd1", "https://a.com/"}},
                        .shadow_logins = {{"user1", "pswd1", "https://a.com/"},
                                          {"user3", "pswd3", "https://c.com/"},
                                          {"user4", "pswdX", "https://d.com/"}},
                        .in_common = 2,
                        .in_main = 1,
                        .in_shadow = 1,
                        .inconsistent = 1}));

// Holds the active experiment stage and the expected outcome.
struct UpmVariationParam {
  UpmExperimentVariation variation =
      UpmExperimentVariation::kEnableForSyncingUsers;
  bool is_sync_enabled = false;
  bool calls_android_backend = false;
  bool calls_built_in_backend = false;
};

class PasswordStoreProxyBackendTestForExperimentStages
    : public PasswordStoreProxyBackendTest,
      public testing::WithParamInterface<UpmVariationParam> {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kUnifiedPasswordManagerAndroid,
        {{"stage",
          base::NumberToString(static_cast<int>(GetParam().variation))}});

    if (GetParam().is_sync_enabled) {
      EnablePasswordSync();
    } else {
      DisablePasswordSync();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(PasswordStoreProxyBackendTestForExperimentStages,
       UseMainBackendToCreateSyncControllerDelegate) {
  EXPECT_CALL(built_in_backend(), CreateSyncControllerDelegate);
  proxy_backend().CreateSyncControllerDelegate();
}

TEST_P(PasswordStoreProxyBackendTestForExperimentStages,
       CallsCorrectBackendForListCalls) {
  // With sync enabled, the main backend services all calls. Shadow traffic is
  // recorded for list calls.
  EXPECT_CALL(android_backend(), GetAllLoginsAsync)
      .Times(GetParam().calls_android_backend ? 1 : 0);
  EXPECT_CALL(built_in_backend(), GetAllLoginsAsync)
      .Times(GetParam().calls_built_in_backend ? 1 : 0);
  proxy_backend().GetAllLoginsAsync(base::DoNothing());
}

INSTANTIATE_TEST_SUITE_P(
    PasswordStoreProxyBackendTest,
    PasswordStoreProxyBackendTestForExperimentStages,
    testing::Values(
        UpmVariationParam{
            .variation = UpmExperimentVariation::kEnableForSyncingUsers,
            .is_sync_enabled = true,
            .calls_android_backend = true,
            .calls_built_in_backend = false,
        },
        UpmVariationParam{
            .variation = UpmExperimentVariation::kEnableForSyncingUsers,
            .is_sync_enabled = false,
            .calls_android_backend = false,
            .calls_built_in_backend = true,
        },
        UpmVariationParam{
            .variation = UpmExperimentVariation::kShadowSyncingUsers,
            .is_sync_enabled = true,
            .calls_android_backend = true,  // As shadow traffic.
            .calls_built_in_backend = true,
        },
        UpmVariationParam{
            .variation = UpmExperimentVariation::kShadowSyncingUsers,
            .is_sync_enabled = false,
            .calls_android_backend = false,  // No shadow traffic anymore!
            .calls_built_in_backend = true,
        },
        UpmVariationParam{
            .variation =
                UpmExperimentVariation::kEnableOnlyBackendForSyncingUsers,
            .is_sync_enabled = true,
            .calls_android_backend = true,
            .calls_built_in_backend = false,
        },
        UpmVariationParam{
            .variation =
                UpmExperimentVariation::kEnableOnlyBackendForSyncingUsers,
            .is_sync_enabled = false,
            .calls_android_backend = false,
            .calls_built_in_backend = true,
        }));
}  // namespace password_manager
