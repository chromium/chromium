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
#include "components/password_manager/core/browser/password_form.h"
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
  form.match_type = PasswordForm::MatchType::kExact;
  return form;
}

std::vector<std::unique_ptr<PasswordForm>> CreateTestLogins() {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateEntry("Todd Tester", "S3cr3t",
                              GURL(u"https://example.com"),
                              PasswordForm::MatchType::kExact));
  forms.push_back(CreateEntry("Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
                              GURL(u"https://m.example.com"),
                              PasswordForm::MatchType::kPSL));
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

class PasswordStoreProxyBackendBaseTest : public testing::Test {
 protected:
  PasswordStoreProxyBackendBaseTest() {
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

TEST_F(PasswordStoreProxyBackendBaseTest, CallCompletionCallbackAfterInit) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;

  // Both backends need to be invoked for a successful completion call.
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(true);
          })));
  EXPECT_CALL(android_backend(), InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(true);
          })));
  EXPECT_CALL(completion_callback, Run(true));
  proxy_backend().InitBackend(nullptr, base::DoNothing(), base::DoNothing(),
                              completion_callback.Get());
}

TEST_F(PasswordStoreProxyBackendBaseTest,
       CallCompletionWithFailureForAnyError) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;

  // If one backend fails to initialize, the result of the second is irrelevant.
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(false);
          })));
  EXPECT_CALL(android_backend(), InitBackend)
      .Times(AtMost(1))
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(true);
          })));
  EXPECT_CALL(completion_callback, Run(false));
  proxy_backend().InitBackend(nullptr, base::DoNothing(), base::DoNothing(),
                              completion_callback.Get());
}

TEST_F(PasswordStoreProxyBackendBaseTest, CallRemoteChangesOnlyForMainBackend) {
  base::MockCallback<RemoveChangesReceived> original_callback;

  // Both backends receive a callback that they trigger for new remote changes.
  RemoveChangesReceived built_in_remote_changes_callback;
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(SaveArg<1>(&built_in_remote_changes_callback));
  RemoveChangesReceived android_remote_changes_callback;
  EXPECT_CALL(android_backend(), InitBackend)
      .WillOnce(SaveArg<1>(&android_remote_changes_callback));
  proxy_backend().InitBackend(nullptr, original_callback.Get(),
                              base::DoNothing(), base::DoNothing());

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

TEST_F(PasswordStoreProxyBackendBaseTest,
       CallSyncCallbackOnlyForBuiltInBackend) {
  base::MockCallback<base::RepeatingClosure> original_callback;

  // Both backends receive a callback that they trigger for new remote changes.
  base::RepeatingClosure built_in_sync_callback;
  EXPECT_CALL(built_in_backend(), InitBackend)
      .WillOnce(SaveArg<2>(&built_in_sync_callback));
  base::RepeatingClosure android_sync_callback;
  EXPECT_CALL(android_backend(), InitBackend)
      .WillOnce(SaveArg<2>(&android_sync_callback));
  proxy_backend().InitBackend(nullptr, base::DoNothing(),
                              original_callback.Get(), base::DoNothing());

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

// Holds the conditions affecting UPM eligibility and the backends
// which should be used for each.
struct UpmVariationParam {
  bool is_sync_enabled = false;
  bool is_unenrolled = false;
  bool android_is_main_backend = false;
};

class PasswordStoreProxyBackendTest
    : public PasswordStoreProxyBackendBaseTest,
      public testing::WithParamInterface<UpmVariationParam> {
 public:
  void SetUp() override {
    if (GetParam().is_sync_enabled) {
      EnablePasswordSync();
    } else {
      DisablePasswordSync();
    }
    prefs()->SetBoolean(prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
                        GetParam().is_unenrolled);
  }

  MockPasswordStoreBackend& main_backend() {
    return GetParam().android_is_main_backend ? android_backend()
                                              : built_in_backend();
  }

  MockPasswordStoreBackend& shadow_backend() {
    return GetParam().android_is_main_backend ? built_in_backend()
                                              : android_backend();
  }
};

TEST_P(PasswordStoreProxyBackendTest, UseMainBackendToGetAllLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));

  EXPECT_CALL(main_backend(), GetAllLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  EXPECT_CALL(shadow_backend(), GetAllLoginsAsync).Times(0);

  proxy_backend().GetAllLoginsAsync(mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest,
       UseMainBackendToGetAutofillableLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));

  EXPECT_CALL(main_backend(), GetAutofillableLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  EXPECT_CALL(shadow_backend(), GetAutofillableLoginsAsync).Times(0);

  proxy_backend().GetAutofillableLoginsAsync(mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest, UseMainBackendToFillMatchingLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));

  EXPECT_CALL(main_backend(), FillMatchingLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  EXPECT_CALL(shadow_backend(), FillMatchingLoginsAsync).Times(0);

  proxy_backend().FillMatchingLoginsAsync(mock_reply.Get(),
                                          /*include_psl=*/false,
                                          std::vector<PasswordFormDigest>());
}

TEST_P(PasswordStoreProxyBackendTest, UseMainBackendToAddLoginAsync) {
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::ADD, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));

  EXPECT_CALL(main_backend(), AddLoginAsync(Eq(form), _))
      .WillOnce(WithArg<1>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  EXPECT_CALL(shadow_backend(), AddLoginAsync).Times(0);

  proxy_backend().AddLoginAsync(form, mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest, UseMainBackendToUpdateLoginAsync) {
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::UPDATE, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));

  EXPECT_CALL(main_backend(), UpdateLoginAsync(Eq(form), _))
      .WillOnce(WithArg<1>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  EXPECT_CALL(shadow_backend(), UpdateLoginAsync).Times(0);

  proxy_backend().UpdateLoginAsync(form, mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest, UseBothBackendsToRemoveLoginAsyncIfUPM) {
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::REMOVE, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));

  EXPECT_CALL(main_backend(), RemoveLoginAsync(Eq(form), _))
      .WillOnce(WithArg<1>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));

  // The shadow backend should only be called to remove logins if the main
  // backend is the android backend, to ensure the login db passwords are
  // also removed.
  EXPECT_CALL(shadow_backend(), RemoveLoginAsync(Eq(form), _))
      .Times(GetParam().android_is_main_backend ? 1 : 0);
  proxy_backend().RemoveLoginAsync(form, mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest,
       UseBothBackendsToRemoveLoginsByURLAndTimeAsyncIfUPM) {
  base::Time kStart = base::Time::FromTimeT(111111);
  base::Time kEnd = base::Time::FromTimeT(22222222);
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::REMOVE, form));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));

  EXPECT_CALL(main_backend(),
              RemoveLoginsByURLAndTimeAsync(_, Eq(kStart), Eq(kEnd), _, _))
      .WillOnce(WithArg<4>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));

  // The shadow backend should only be called to remove logins if the main
  // backend is the android backend, to ensure the login db passwords are
  // also removed.
  EXPECT_CALL(shadow_backend(),
              RemoveLoginsByURLAndTimeAsync(_, Eq(kStart), Eq(kEnd), _, _))
      .Times(GetParam().android_is_main_backend ? 1 : 0);
  proxy_backend().RemoveLoginsByURLAndTimeAsync(
      base::BindRepeating(&FilterNoUrl), kStart, kEnd, base::NullCallback(),
      mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest,
       UseBothBackendsToRemoveLoginsCreatedBetweenAsyncIfUPM) {
  base::Time kStart = base::Time::FromTimeT(111111);
  base::Time kEnd = base::Time::FromTimeT(22222222);
  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::REMOVE, CreateTestForm()));
  EXPECT_CALL(mock_reply,
              Run(VariantWith<PasswordChanges>(Optional(change_list))));

  EXPECT_CALL(main_backend(),
              RemoveLoginsCreatedBetweenAsync(Eq(kStart), Eq(kEnd), _))
      .WillOnce(WithArg<2>(
          Invoke([&change_list](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  EXPECT_CALL(shadow_backend(),
              RemoveLoginsCreatedBetweenAsync(Eq(kStart), Eq(kEnd), _))
      .Times(GetParam().android_is_main_backend ? 1 : 0);
  proxy_backend().RemoveLoginsCreatedBetweenAsync(kStart, kEnd,
                                                  mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest,
       UseMainBackendToDisableAutoSignInForOriginsAsync) {
  base::MockCallback<base::OnceClosure> mock_reply;
  EXPECT_CALL(mock_reply, Run);
  EXPECT_CALL(main_backend(), DisableAutoSignInForOriginsAsync)
      .WillOnce(WithArg<1>(
          Invoke([](base::OnceClosure reply) { std::move(reply).Run(); })));
  EXPECT_CALL(shadow_backend(), DisableAutoSignInForOriginsAsync).Times(0);
  proxy_backend().DisableAutoSignInForOriginsAsync(
      base::BindRepeating(&FilterNoUrl), mock_reply.Get());
}

TEST_P(PasswordStoreProxyBackendTest,
       UseMainBackendToGetSmartBubbleStatsStore) {
  EXPECT_CALL(main_backend(), GetSmartBubbleStatsStore);
  EXPECT_CALL(shadow_backend(), GetSmartBubbleStatsStore).Times(0);
  proxy_backend().GetSmartBubbleStatsStore();
}

TEST_P(PasswordStoreProxyBackendTest,
       OnSyncServiceInitializedPropagatedToAndroidBackend) {
  syncer::TestSyncService sync_service;
  EXPECT_CALL(android_backend(), OnSyncServiceInitialized(&sync_service));
  proxy_backend().OnSyncServiceInitialized(&sync_service);
}

INSTANTIATE_TEST_SUITE_P(
    PasswordStoreProxyBackendBaseTest,
    PasswordStoreProxyBackendTest,
    testing::Values(UpmVariationParam{.is_sync_enabled = true,
                                      .is_unenrolled = false,
                                      .android_is_main_backend = true},

                    UpmVariationParam{.is_sync_enabled = true,
                                      .is_unenrolled = true,
                                      .android_is_main_backend = false},

                    UpmVariationParam{.is_sync_enabled = false,
                                      .is_unenrolled = true,
                                      .android_is_main_backend = false},

                    UpmVariationParam{.is_sync_enabled = false,
                                      .is_unenrolled = false,
                                      .android_is_main_backend = false}),
    [](const ::testing::TestParamInfo<UpmVariationParam>& info) {
      std::string syncing =
          info.param.is_sync_enabled ? "Syncing" : "NotSyncing";
      std::string unenrolled =
          info.param.is_unenrolled ? "Unenrolled" : "Enrolled";
      return syncing + unenrolled;
    });

struct FallbackParam {
  PasswordStoreBackendError error;
  bool should_fallback;
};

class PasswordStoreProxyBackendTestWithErrorsForFallbacks
    : public PasswordStoreProxyBackendBaseTest,
      public testing::WithParamInterface<FallbackParam> {};

TEST_P(PasswordStoreProxyBackendTestWithErrorsForFallbacks,
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

TEST_P(PasswordStoreProxyBackendTestWithErrorsForFallbacks,
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

TEST_P(PasswordStoreProxyBackendTestWithErrorsForFallbacks,
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
    PasswordStoreProxyBackendBaseTest,
    PasswordStoreProxyBackendTestWithErrorsForFallbacks,
    testing::Values(
        FallbackParam{.error = kUnrecoverableError, .should_fallback = true},
        FallbackParam{.error = kUnspecifiedError, .should_fallback = true},
        FallbackParam{.error = kRecoverableError, .should_fallback = false},
        FallbackParam{.error = kRetriableError, .should_fallback = false}));

}  // namespace password_manager
