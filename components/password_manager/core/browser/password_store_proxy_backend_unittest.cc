// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_proxy_backend.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
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
const PasswordStoreBackendError kUnspecifiedError = PasswordStoreBackendError(
    PasswordStoreBackendErrorType::kUncategorized,
    PasswordStoreBackendErrorRecoveryType::kUnspecified);
const PasswordStoreBackendError kRecoverableError = PasswordStoreBackendError(
    PasswordStoreBackendErrorType::kUncategorized,
    PasswordStoreBackendErrorRecoveryType::kRecoverable);
const PasswordStoreBackendError kRetriableError = PasswordStoreBackendError(
    PasswordStoreBackendErrorType::kUncategorized,
    PasswordStoreBackendErrorRecoveryType::kRetriable);

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
  EXPECT_CALL(android_backend(), GetAllLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  EnablePasswordSync();
  EXPECT_CALL(built_in_backend(), GetAllLoginsAsync).Times(0);
  proxy_backend().GetAllLoginsAsync(mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToGetAutofillableLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  EXPECT_CALL(android_backend(), GetAutofillableLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  EXPECT_CALL(built_in_backend(), GetAutofillableLoginsAsync).Times(0);
  EnablePasswordSync();
  proxy_backend().GetAutofillableLoginsAsync(mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToFillMatchingLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  EXPECT_CALL(android_backend(), FillMatchingLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  EnablePasswordSync();
  EXPECT_CALL(built_in_backend(), FillMatchingLoginsAsync).Times(0);
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
  EXPECT_CALL(built_in_backend(), AddLoginAsync).Times(0);
  EXPECT_CALL(android_backend(), AddLoginAsync(Eq(form), _))
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
  EXPECT_CALL(built_in_backend(), UpdateLoginAsync).Times(0);
  EXPECT_CALL(android_backend(), UpdateLoginAsync(Eq(form), _))
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
  // Removals are always mirrored on the built-in backend.
  EXPECT_CALL(built_in_backend(), RemoveLoginAsync(Eq(form), _));
  EXPECT_CALL(android_backend(), RemoveLoginAsync(Eq(form), _))
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
  // Removals are always mirrored on the built-in backend.
  EXPECT_CALL(built_in_backend(),
              RemoveLoginsByURLAndTimeAsync(_, Eq(kStart), Eq(kEnd), _, _));
  EXPECT_CALL(android_backend(),
              RemoveLoginsByURLAndTimeAsync(_, Eq(kStart), Eq(kEnd), _, _))
      .WillOnce(WithArg<4>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  proxy_backend().RemoveLoginsByURLAndTimeAsync(
      base::BindRepeating(&FilterNoUrl), kStart, kEnd, base::NullCallback(),
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
  // Removals are always mirrored on the built-in backend.
  EXPECT_CALL(built_in_backend(),
              RemoveLoginsCreatedBetweenAsync(Eq(kStart), Eq(kEnd), _));
  EXPECT_CALL(android_backend(),
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
  EXPECT_CALL(built_in_backend(), DisableAutoSignInForOriginsAsync).Times(0);
  EXPECT_CALL(android_backend(), DisableAutoSignInForOriginsAsync)
      .WillOnce(WithArg<1>(
          Invoke([](base::OnceClosure reply) { std::move(reply).Run(); })));
  proxy_backend().DisableAutoSignInForOriginsAsync(
      base::BindRepeating(&FilterNoUrl), mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToGetSmartBubbleStatsStore) {
  EXPECT_CALL(android_backend(), GetSmartBubbleStatsStore);
  proxy_backend().GetSmartBubbleStatsStore();
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToGetFieldInfoStore) {
  EXPECT_CALL(android_backend(), GetFieldInfoStore);
  proxy_backend().GetFieldInfoStore();
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowGetAllLoginsAsyncWithoutSyncOrMigration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid, {{"migration_version", "2"}});
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
      features::kUnifiedPasswordManagerAndroid, {{"migration_version", "2"}});
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
      features::kUnifiedPasswordManagerAndroid, {{"migration_version", "2"}});
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

  EXPECT_CALL(built_in_backend(), AddLoginAsync).Times(0);
  EXPECT_CALL(android_backend(), AddLoginAsync);
  proxy_backend().AddLoginAsync(CreateTestForm(),
                                /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowAddLoginAsyncWhenSyncDisabledAndInitialMigrationIncomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid, {{"migration_version", "2"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

  DisablePasswordSync();

  EXPECT_CALL(built_in_backend(), AddLoginAsync);
  EXPECT_CALL(android_backend(), AddLoginAsync).Times(0);
  proxy_backend().AddLoginAsync(CreateTestForm(),
                                /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest, NoShadowUpdateLoginAsyncWhenSyncEnabled) {
  EnablePasswordSync();

  EXPECT_CALL(built_in_backend(), UpdateLoginAsync).Times(0);
  EXPECT_CALL(android_backend(), UpdateLoginAsync);
  proxy_backend().UpdateLoginAsync(CreateTestForm(),
                                   /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowUpdateLoginAsyncWhenSyncDisabledAndInitialMigrationIncomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid, {{"migration_version", "2"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

  DisablePasswordSync();

  EXPECT_CALL(built_in_backend(), UpdateLoginAsync);
  EXPECT_CALL(android_backend(), UpdateLoginAsync).Times(0);
  proxy_backend().UpdateLoginAsync(CreateTestForm(),
                                   /*callback=*/base::DoNothing());
}

TEST_F(PasswordStoreProxyBackendTest,
       NoShadowRemoveLoginAsyncWhenSyncDisabledAndInitialMigrationIncomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid, {{"migration_version", "2"}});
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
      features::kUnifiedPasswordManagerAndroid, {{"migration_version", "1"}});

  EnablePasswordSync();

  EXPECT_CALL(built_in_backend(), RemoveLoginAsync);
  EXPECT_CALL(android_backend(), RemoveLoginAsync);
  proxy_backend().RemoveLoginAsync(CreateTestForm(),
                                   /*callback=*/base::DoNothing());
}

TEST_F(
    PasswordStoreProxyBackendTest,
    NoShadowRemoveLoginsByURLAndTimeAsyncWhenSyncDisabledAndInitialMigrationIncomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid, {{"migration_version", "2"}});
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
      features::kUnifiedPasswordManagerAndroid, {{"migration_version", "1"}});

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

TEST_F(
    PasswordStoreProxyBackendTest,
    NoShadowRemoveLoginsCreatedBetweenAsyncWhenSyncDisabledAndInitialMigrationIncomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid, {{"migration_version", "2"}});
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
      features::kUnifiedPasswordManagerAndroid, {{"migration_version", "1"}});

  EnablePasswordSync();

  EXPECT_CALL(built_in_backend(), RemoveLoginsCreatedBetweenAsync);
  EXPECT_CALL(android_backend(), RemoveLoginsCreatedBetweenAsync);
  proxy_backend().RemoveLoginsCreatedBetweenAsync(
      /*delete_begin=*/base::Time::FromTimeT(111111),
      /*delete_end=*/base::Time::FromTimeT(22222222),
      /*callback=*/base::DoNothing());
}

TEST_F(
    PasswordStoreProxyBackendTest,
    NoShadowDisableAutoSignInForOriginsAsyncWhenSyncDisabledAndInitialMigrationIncomplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid, {{"migration_version", "2"}});
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
       UsesBuiltInBackendAsMainBackendPasswordSyncDisabledInSettings) {
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
      public testing::WithParamInterface<FallbackParam> {};

TEST_P(PasswordStoreProxyBackendTestWithFallbackParam,
       RetriesAddLoginOnBuiltInBackend) {
  const FallbackParam& p = GetParam();

  base::HistogramTester histogram_tester;

  EnablePasswordSync();

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;

  EXPECT_CALL(android_backend(), AddLoginAsync)
      .WillOnce(WithArg<1>(
          Invoke([&p](auto reply) -> void { std::move(reply).Run(p.error); })));
  const PasswordStoreChangeList changes = {
      PasswordStoreChange(PasswordStoreChange::Type::ADD, CreateTestForm())};
  if (p.should_fallback) {
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

  if (p.should_fallback) {
    histogram_tester.ExpectUniqueSample(
        GetFallbackHistogramNameForMethodName("AddLoginAsync"), true, 1);
  }
}

TEST_P(PasswordStoreProxyBackendTestWithFallbackParam,
       RetriesUpdateLoginOnBuiltInBackend) {
  const FallbackParam& p = GetParam();

  base::HistogramTester histogram_tester;

  EnablePasswordSync();

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;

  EXPECT_CALL(android_backend(), UpdateLoginAsync)
      .WillOnce(WithArg<1>(
          Invoke([&p](auto reply) -> void { std::move(reply).Run(p.error); })));
  const PasswordStoreChangeList changes = {
      PasswordStoreChange(PasswordStoreChange::Type::ADD, CreateTestForm())};
  if (p.should_fallback) {
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

  if (p.should_fallback) {
    histogram_tester.ExpectUniqueSample(
        GetFallbackHistogramNameForMethodName("UpdateLoginAsync"), true, 1);
  }
}

TEST_P(PasswordStoreProxyBackendTestWithFallbackParam,
       RetriesFillMatchingLoginsOnBuiltInBackend) {
  const FallbackParam& p = GetParam();

  base::HistogramTester histogram_tester;

  EnablePasswordSync();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();

  EXPECT_CALL(android_backend(), FillMatchingLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([&p](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(p.error);
      })));
  if (p.should_fallback) {
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
  if (p.should_fallback) {
    histogram_tester.ExpectUniqueSample(
        GetFallbackHistogramNameForMethodName("FillMatchingLoginsAsync"), true,
        1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    PasswordStoreProxyBackendTest,
    PasswordStoreProxyBackendTestWithFallbackParam,
    testing::Values(
        FallbackParam{.error = kUnrecoverableError, .should_fallback = true},
        FallbackParam{.error = kUnspecifiedError, .should_fallback = true},
        FallbackParam{.error = kRecoverableError, .should_fallback = false},
        FallbackParam{.error = kRetriableError, .should_fallback = false}));

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

// Holds the active experiment stage and the expected outcome.
struct UpmVariationParam {
  bool is_sync_enabled = false;
  bool calls_android_backend = false;
  bool calls_built_in_backend = false;
};

class PasswordStoreProxyBackendTestForExperimentStages
    : public PasswordStoreProxyBackendTest,
      public testing::WithParamInterface<UpmVariationParam> {
 public:
  void SetUp() override {
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

INSTANTIATE_TEST_SUITE_P(PasswordStoreProxyBackendTest,
                         PasswordStoreProxyBackendTestForExperimentStages,
                         testing::Values(
                             UpmVariationParam{
                                 .is_sync_enabled = true,
                                 .calls_android_backend = true,
                                 .calls_built_in_backend = false,
                             },
                             UpmVariationParam{
                                 .is_sync_enabled = false,
                                 .calls_android_backend = false,
                                 .calls_built_in_backend = true,
                             }));
}  // namespace password_manager
