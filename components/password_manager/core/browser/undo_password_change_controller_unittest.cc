// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/undo_password_change_controller.h"

#include <optional>
#include <string>

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form_cache_impl.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#if BUILDFLAG(IS_ANDROID)
#include "components/webauthn/android/cred_man_support.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#endif  // BUILDFLAG(IS_ANDROID)

using base::test::RunOnceClosure;
using testing::NiceMock;
using testing::Return;
using ChangeRecoveryUkmEntry = ukm::builders::PasswordManager_ChangeRecovery;
namespace password_manager {
namespace {
const std::u16string kUsername = u"username";
const std::u16string kBackupPassword = u"backup_password";
constexpr int kUsernameFieldIndex = 0;
constexpr int kPasswordFieldIndex = 1;

autofill::PasswordAndMetadata GetPasswordAndMetadata(
    const std::u16string& username = kUsername,
    const std::optional<std::u16string>& backup_password = kBackupPassword) {
  autofill::PasswordAndMetadata credential;
  credential.username_value = username;
  credential.backup_password_value = backup_password;
  return credential;
}

autofill::Suggestion::PasswordSuggestionDetails GetSuggestionDetails(
    autofill::PasswordAndMetadata credential) {
  autofill::Suggestion::PasswordSuggestionDetails password_details;
  password_details.username = credential.username_value;
  password_details.signon_realm = credential.realm;
  return password_details;
}

const ukm::mojom::UkmEntry* GetUkmEntry(
    const ukm::TestAutoSetUkmRecorder& test_ukm_recorder) {
  auto ukm_entries =
      test_ukm_recorder.GetEntriesByName(ChangeRecoveryUkmEntry::kEntryName);
  CHECK_EQ(ukm_entries.size(), 1u);
  return ukm_entries[0];
}

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              TriggerPasswordRecoverySuggestions,
              (autofill::FieldRendererId),
              (override));
  MOCK_METHOD(PasswordManagerInterface*, GetPasswordManager, (), (override));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD(autofill::LogManager*, GetCurrentLogManager, (), (override));
};
}  // namespace

class UndoPasswordChangeControllerTest : public testing::Test {
 protected:
  UndoPasswordChangeControllerTest() = default;

  void SetUp() override {
    ON_CALL(driver_, GetPasswordManager)
        .WillByDefault(Return(&password_manager_));
    ON_CALL(password_manager_, GetPasswordFormCache)
        .WillByDefault(Return(&password_form_cache_));
    // Called after `PasswordFormManager` parses a form.
    ON_CALL(client_, GetCurrentLogManager).WillByDefault(Return(nullptr));

#if BUILDFLAG(IS_ANDROID)
    webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
        webauthn::CredManSupport::DISABLED);
#endif  // BUILDFLAG(IS_ANDROID)

    observed_form_.set_url(
        GURL("https://accounts.google.com/a/ServiceLoginAuth"));
    observed_form_.set_action(
        GURL("https://accounts.google.com/a/ServiceLogin"));
    observed_form_.set_name(u"sign-in");
    observed_form_.set_renderer_id(autofill::FormRendererId(1));

    autofill::FormFieldData field;
    field.set_name(u"username");
    field.set_id_attribute(field.name());
    field.set_name_attribute(field.name());
    field.set_form_control_type(autofill::FormControlType::kInputText);
    field.set_renderer_id(autofill::FieldRendererId(2));
    test_api(observed_form_).Append(field);

    field.set_name(u"password");
    field.set_id_attribute(field.name());
    field.set_name_attribute(field.name());
    field.set_form_control_type(autofill::FormControlType::kInputPassword);
    field.set_renderer_id(autofill::FieldRendererId(3));
    test_api(observed_form_).Append(field);

    failed_login_form_.form_data = observed_form_;
    failed_login_form_.username_value = kUsername;
    failed_login_form_.username_element =
        observed_form_.fields()[kUsernameFieldIndex].name();
    failed_login_form_.password_element =
        observed_form_.fields()[kPasswordFieldIndex].name();
    failed_login_form_.password_element_renderer_id =
        observed_form_.fields()[kPasswordFieldIndex].renderer_id();

    best_match_form_.username_value = failed_login_form_.username_value;
    best_match_form_.match_type = PasswordForm::MatchType::kExact;
    controller_.OnNavigation(url::Origin::Create(GURL("https://example.com")),
                             ukm::UkmRecorder::GetNewSourceID());
  }

  // Creates a form manager and triggers parsing by calling
  // `form_fetcher_.NotifyFetchCompleted()`.
  std::unique_ptr<password_manager::PasswordFormManager> CreateFormManager(
      const PasswordForm& best_match) {
    auto form_manager = std::make_unique<password_manager::PasswordFormManager>(
        &client_, driver_.AsWeakPtr(), observed_form_, &form_fetcher_,
        std::make_unique<password_manager::PasswordSaveManagerImpl>(
            /*profile_form_saver=*/std::make_unique<StubFormSaver>(),
            /*account_form_saver=*/nullptr),
        /*metrics_recorder=*/nullptr);
    form_manager->DisableFillingServerPredictionsForTesting();
    form_fetcher_.SetNonFederated({best_match});
    form_fetcher_.SetBestMatches({best_match});
    form_fetcher_.NotifyFetchCompleted();

    return form_manager;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  autofill::FormData observed_form_;
  PasswordForm failed_login_form_;
  PasswordForm best_match_form_;
  MockPasswordManagerDriver driver_;
  FakeFormFetcher form_fetcher_;
  MockPasswordManager password_manager_;
  PasswordFormCacheImpl password_form_cache_;
  NiceMock<MockPasswordManagerClient> client_;
  UndoPasswordChangeController controller_;
};

TEST_F(UndoPasswordChangeControllerTest, EmptyState) {
  EXPECT_EQ(controller_.GetState(kUsername),
            PasswordRecoveryState::kRegularFlow);
}

TEST_F(UndoPasswordChangeControllerTest, OnSuggestionSelected) {
  const auto credential = GetPasswordAndMetadata();

  controller_.OnSuggestionSelected(credential);

  EXPECT_EQ(controller_.GetState(credential.username_value),
            PasswordRecoveryState::kTroubleSigningIn);
}

TEST_F(UndoPasswordChangeControllerTest,
       OnSuggestionSelectedNoBackupPasswordIgnored) {
  auto credential = GetPasswordAndMetadata(kUsername,
                                           /*backup_password=*/std::nullopt);

  controller_.OnSuggestionSelected(credential);

  EXPECT_EQ(controller_.GetState(credential.username_value),
            PasswordRecoveryState::kRegularFlow);
}

TEST_F(UndoPasswordChangeControllerTest,
       OnSuggestionSelectedNoBackupResetsFlow) {
  const auto credential_1 =
      GetPasswordAndMetadata(u"username_2", kBackupPassword);
  const auto credential_2 =
      GetPasswordAndMetadata(kUsername,
                             /*backup_password=*/std::nullopt);

  controller_.OnSuggestionSelected(credential_1);
  controller_.OnSuggestionSelected(credential_2);

  EXPECT_EQ(controller_.GetState(credential_1.username_value),
            PasswordRecoveryState::kRegularFlow);
  EXPECT_EQ(controller_.GetState(credential_2.username_value),
            PasswordRecoveryState::kRegularFlow);
}

TEST_F(UndoPasswordChangeControllerTest, OnSuggestionSelectedTwice) {
  auto credential = GetPasswordAndMetadata();

  controller_.OnSuggestionSelected(credential);
  controller_.OnSuggestionSelected(credential);

  EXPECT_EQ(controller_.GetState(credential.username_value),
            PasswordRecoveryState::kTroubleSigningIn);
}

TEST_F(UndoPasswordChangeControllerTest, OnTroubleSigningIn) {
  const auto credential = GetPasswordAndMetadata();
  const auto password_details = GetSuggestionDetails(credential);
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  const auto expected_metric_state = password_manager::metrics_util::
      PasswordChangeRecoveryFlowState::kTroubleSigningInClicked;

  controller_.OnSuggestionSelected(credential);
  controller_.OnTroubleSigningInClicked(password_details);

  EXPECT_EQ(controller_.GetState(credential.username_value),
            PasswordRecoveryState::kIncludeBackup);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeRecoveryFlow", expected_metric_state, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      ChangeRecoveryUkmEntry::kPasswordChangeRecoveryFlowName,
      static_cast<int>(expected_metric_state));
}

TEST_F(UndoPasswordChangeControllerTest, DifferentUsernameResetsFlow) {
  const auto credential_1 = GetPasswordAndMetadata();
  const auto password_details = GetSuggestionDetails(credential_1);
  const auto credential_2 =
      GetPasswordAndMetadata(u"username2", kBackupPassword);

  controller_.OnSuggestionSelected(credential_1);
  controller_.OnTroubleSigningInClicked(password_details);
  controller_.OnSuggestionSelected(credential_2);

  EXPECT_EQ(controller_.GetState(credential_1.username_value),
            PasswordRecoveryState::kRegularFlow);
  EXPECT_EQ(controller_.GetState(credential_2.username_value),
            PasswordRecoveryState::kTroubleSigningIn);
}

TEST_F(UndoPasswordChangeControllerTest, DifferentUrlResetsFlow) {
  const auto credential = GetPasswordAndMetadata();
  const auto password_details = GetSuggestionDetails(credential);

  controller_.OnSuggestionSelected(credential);
  controller_.OnTroubleSigningInClicked(password_details);
  controller_.OnNavigation(url::Origin::Create(GURL("https://example2.com")),
                           ukm::UkmRecorder::GetNewSourceID());

  EXPECT_EQ(controller_.GetState(credential.username_value),
            PasswordRecoveryState::kRegularFlow);
}

TEST_F(UndoPasswordChangeControllerTest,
       CredentialWithIncludeBackupStateClicked) {
  const auto credential = GetPasswordAndMetadata();
  const auto password_details = GetSuggestionDetails(credential);

  controller_.OnSuggestionSelected(credential);
  controller_.OnTroubleSigningInClicked(password_details);
  controller_.OnSuggestionSelected(credential);

  EXPECT_EQ(controller_.GetState(credential.username_value),
            PasswordRecoveryState::kIncludeBackup);
}

TEST_F(UndoPasswordChangeControllerTest, FullFlowMultipleCredentials) {
  const auto credential = GetPasswordAndMetadata();
  const auto credential_2 = GetPasswordAndMetadata(u"username2");

  controller_.OnSuggestionSelected(credential);
  controller_.OnSuggestionSelected(credential_2);
  controller_.OnTroubleSigningInClicked(GetSuggestionDetails(credential_2));

  EXPECT_EQ(controller_.GetState(credential.username_value),
            PasswordRecoveryState::kRegularFlow);
  EXPECT_EQ(controller_.GetState(credential_2.username_value),
            PasswordRecoveryState::kIncludeBackup);
}

TEST_F(UndoPasswordChangeControllerTest, OnLoginPotentiallyFailedFlagOn) {
  base::test::ScopedFeatureList feature_list(features::kShowRecoveryPassword);
  best_match_form_.SetPasswordBackupNote(kBackupPassword);
  auto form_manager = CreateFormManager(best_match_form_);
  base::RunLoop run_loop;

  controller_.OnLoginPotentiallyFailed(&driver_, failed_login_form_);
  EXPECT_CALL(driver_, TriggerPasswordRecoverySuggestions(
                           failed_login_form_.password_element_renderer_id))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  static_cast<PasswordFormManagerObserver*>(&controller_)
      ->OnPasswordFormParsed(form_manager.get());
  run_loop.Run();

  EXPECT_EQ(controller_.GetState(kUsername),
            PasswordRecoveryState::kShowProactiveRecovery);
}

TEST_F(UndoPasswordChangeControllerTest, OnLoginPotentiallyFailedFlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kShowRecoveryPassword);
  best_match_form_.SetPasswordBackupNote(kBackupPassword);
  auto form_manager = CreateFormManager(best_match_form_);

  controller_.OnLoginPotentiallyFailed(&driver_, failed_login_form_);
  EXPECT_CALL(driver_, TriggerPasswordRecoverySuggestions(
                           failed_login_form_.password_element_renderer_id))
      .Times(0);
  static_cast<PasswordFormManagerObserver*>(&controller_)
      ->OnPasswordFormParsed(form_manager.get());

  EXPECT_EQ(controller_.GetState(kUsername),
            PasswordRecoveryState::kRegularFlow);
}

TEST_F(UndoPasswordChangeControllerTest,
       OnLoginPotentiallyFailed_UnfocusablePassword) {
  base::test::ScopedFeatureList feature_list(features::kShowRecoveryPassword);
  best_match_form_.SetPasswordBackupNote(kBackupPassword);
  auto form_manager = CreateFormManager(best_match_form_);
  test_api(failed_login_form_.form_data)
      .fields()[kPasswordFieldIndex]
      .set_is_focusable(false);

  controller_.OnLoginPotentiallyFailed(&driver_, failed_login_form_);

  EXPECT_FALSE(controller_.failed_login_form());
}

TEST_F(UndoPasswordChangeControllerTest,
       OnLoginPotentiallyFailedNoBackupIgnored) {
  base::test::ScopedFeatureList feature_list(features::kShowRecoveryPassword);
  auto form_manager = CreateFormManager(best_match_form_);

  controller_.OnLoginPotentiallyFailed(&driver_, failed_login_form_);
  EXPECT_CALL(driver_, TriggerPasswordRecoverySuggestions(
                           failed_login_form_.password_element_renderer_id))
      .Times(0);
  static_cast<PasswordFormManagerObserver*>(&controller_)
      ->OnPasswordFormParsed(form_manager.get());

  EXPECT_EQ(controller_.GetState(kUsername),
            PasswordRecoveryState::kRegularFlow);
}

TEST_F(UndoPasswordChangeControllerTest,
       OnLoginPotentiallyFailed_BackupUsed_Ignored) {
  base::test::ScopedFeatureList feature_list(features::kShowRecoveryPassword);
  failed_login_form_.password_value = kBackupPassword;
  best_match_form_.SetPasswordBackupNote(kBackupPassword);
  auto form_manager = CreateFormManager(best_match_form_);

  controller_.OnLoginPotentiallyFailed(&driver_, failed_login_form_);
  EXPECT_CALL(driver_, TriggerPasswordRecoverySuggestions(
                           failed_login_form_.password_element_renderer_id))
      .Times(0);
  static_cast<PasswordFormManagerObserver*>(&controller_)
      ->OnPasswordFormParsed(form_manager.get());

  EXPECT_EQ(controller_.GetState(kUsername),
            PasswordRecoveryState::kRegularFlow);
}

TEST_F(UndoPasswordChangeControllerTest,
       FindLoginWithProactiveRecoveryStateMatch) {
  base::test::ScopedFeatureList feature_list(features::kShowRecoveryPassword);
  best_match_form_.SetPasswordBackupNote(kBackupPassword);
  auto form_manager = CreateFormManager(best_match_form_);
  const autofill::PasswordAndMetadata match = GetPasswordAndMetadata();
  autofill::PasswordFormFillData fill_data;
  // Not a match
  fill_data.preferred_login = GetPasswordAndMetadata(u"username2");
  // Match
  fill_data.additional_logins.push_back(match);
  base::RunLoop run_loop;

  controller_.OnLoginPotentiallyFailed(&driver_, failed_login_form_);
  EXPECT_CALL(driver_, TriggerPasswordRecoverySuggestions(
                           failed_login_form_.password_element_renderer_id))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  static_cast<PasswordFormManagerObserver*>(&controller_)
      ->OnPasswordFormParsed(form_manager.get());
  run_loop.Run();

  EXPECT_EQ(controller_.FindLoginWithProactiveRecoveryState(&fill_data), match);
}

TEST_F(UndoPasswordChangeControllerTest,
       FindLoginWithProactiveRecoveryStateNoMatch) {
  base::test::ScopedFeatureList feature_list(features::kShowRecoveryPassword);
  best_match_form_.SetPasswordBackupNote(kBackupPassword);
  auto form_manager = CreateFormManager(best_match_form_);
  autofill::PasswordFormFillData fill_data;
  // Not a match
  fill_data.preferred_login = GetPasswordAndMetadata(u"username2");
  base::RunLoop run_loop;

  controller_.OnLoginPotentiallyFailed(&driver_, failed_login_form_);
  EXPECT_CALL(driver_, TriggerPasswordRecoverySuggestions(
                           failed_login_form_.password_element_renderer_id))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  static_cast<PasswordFormManagerObserver*>(&controller_)
      ->OnPasswordFormParsed(form_manager.get());
  run_loop.Run();

  EXPECT_EQ(controller_.FindLoginWithProactiveRecoveryState(&fill_data),
            std::nullopt);
}

TEST_F(UndoPasswordChangeControllerTest, OnSuggestionsHidden) {
  base::test::ScopedFeatureList feature_list(features::kShowRecoveryPassword);
  best_match_form_.SetPasswordBackupNote(kBackupPassword);
  auto form_manager = CreateFormManager(best_match_form_);
  base::RunLoop run_loop;
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  const auto expected_metric_state = password_manager::metrics_util::
      PasswordChangeRecoveryFlowState::kProactiveRecoveryPopupShown;

  controller_.OnLoginPotentiallyFailed(&driver_, failed_login_form_);
  EXPECT_CALL(driver_, TriggerPasswordRecoverySuggestions(
                           failed_login_form_.password_element_renderer_id))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  static_cast<PasswordFormManagerObserver*>(&controller_)
      ->OnPasswordFormParsed(form_manager.get());
  run_loop.Run();
  controller_.OnSuggestionsHidden();

  EXPECT_EQ(controller_.GetState(kUsername),
            PasswordRecoveryState::kIncludeBackup);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeRecoveryFlow", expected_metric_state, 1);
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetUkmEntry(test_ukm_recorder),
      ChangeRecoveryUkmEntry::kPasswordChangeRecoveryFlowName,
      static_cast<int>(expected_metric_state));
}

}  // namespace password_manager
