// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manual_fallback_flow.h"

#include <vector>

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

using autofill::AutofillClient;
using autofill::AutofillPopupDelegate;
using autofill::AutofillSuggestionTriggerSource;
using autofill::FieldRendererId;
using autofill::PopupHidingReason;
using autofill::PopupItemId;
using autofill::Suggestion;
using autofill::TestAutofillClient;
using autofill::test::AutofillUnitTestEnvironment;
using autofill::test::MakeFieldRendererId;
using base::i18n::TextDirection;
using base::test::RunOnceCallback;
using testing::_;
using testing::AllOf;
using testing::ByMove;
using testing::Field;
using testing::NiceMock;
using testing::Return;

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  ~MockAutofillClient() override = default;
  MOCK_METHOD(void,
              ShowAutofillPopup,
              (const AutofillClient::PopupOpenArgs&,
               base::WeakPtr<AutofillPopupDelegate>),
              (override));
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason), (override));
};

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() = default;
  ~MockPasswordManagerDriver() override = default;
  MOCK_METHOD(void,
              PreviewField,
              (FieldRendererId, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              FillField,
              (FieldRendererId, const std::u16string&),
              (override));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;
  MOCK_METHOD(void,
              NavigateToManagePasswordsPage,
              (ManagePasswordsReferrer),
              (override));
  MOCK_METHOD(bool,
              CanUseBiometricAuthForFilling,
              (device_reauth::DeviceAuthenticator*),
              (override));
  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
};

class PasswordManualFallbackFlowTest : public ::testing::Test {
 public:
  PasswordManualFallbackFlowTest() {
    profile_password_store().Init(/*prefs=*/nullptr,
                                  /*affiliated_match_helper=*/nullptr);
    // Add 1 password form to the password store.
    PasswordForm form = CreateEntry("username@example.com", "password",
                                    GURL("https://google.com/"),
                                    PasswordForm::MatchType::kExact);
    profile_password_store().AddLogin(form);

    std::unique_ptr<SavedPasswordsPresenter> passwords_presenter =
        std::make_unique<SavedPasswordsPresenter>(
            &affiliation_service(), profile_password_store_,
            /*account_password_store_=*/nullptr);
    flow_ = std::make_unique<PasswordManualFallbackFlow>(
        &driver(), &autofill_client(), &password_manager_client(),
        std::move(passwords_presenter));
  }

  ~PasswordManualFallbackFlowTest() override {
    profile_password_store_->ShutdownOnUIThread();
  }

  PasswordManualFallbackFlow& flow() { return *flow_; }

  MockPasswordManagerDriver& driver() { return *driver_; }

  MockAutofillClient& autofill_client() { return *autofill_client_; }

  MockPasswordManagerClient& password_manager_client() {
    return *password_manager_client_;
  }

  affiliations::FakeAffiliationService& affiliation_service() {
    return *affiliation_service_;
  }

  TestPasswordStore& profile_password_store() {
    return *profile_password_store_;
  }

  // The test fixture relies on the fact that `TestPasswordStore` performs all
  // operation asynchronously.
  void ProcessPasswordStoreUpdates() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<NiceMock<MockPasswordManagerDriver>> driver_ =
      std::make_unique<NiceMock<MockPasswordManagerDriver>>();
  std::unique_ptr<NiceMock<MockAutofillClient>> autofill_client_ =
      std::make_unique<NiceMock<MockAutofillClient>>();
  std::unique_ptr<NiceMock<MockPasswordManagerClient>>
      password_manager_client_ =
          std::make_unique<NiceMock<MockPasswordManagerClient>>();
  std::unique_ptr<affiliations::FakeAffiliationService> affiliation_service_ =
      std::make_unique<affiliations::FakeAffiliationService>();
  scoped_refptr<TestPasswordStore> profile_password_store_ =
      base::MakeRefCounted<TestPasswordStore>();
  std::unique_ptr<PasswordManualFallbackFlow> flow_;
};

// Test that no suggestions are shown before the passwords are read from disk.
TEST_F(PasswordManualFallbackFlowTest, RunFlow_NoSuggestionsReturned) {
  EXPECT_CALL(autofill_client(), ShowAutofillPopup).Times(0);

  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);
}

// Test that the suggestions are not shown when the `SavedPasswordsPresenter`
// reads the passwords from disk.
TEST_F(PasswordManualFallbackFlowTest, ReturnSuggestions_NoFlowInvocation) {
  EXPECT_CALL(autofill_client(), ShowAutofillPopup).Times(0);

  ProcessPasswordStoreUpdates();
}

// Test that the suggestions are shown when the flow is invoked after the
// suggestions were read from disk.
TEST_F(PasswordManualFallbackFlowTest, ReturnSuggestions_InvokeFlow) {
  ProcessPasswordStoreUpdates();

  const gfx::RectF bounds(1, 1, 2, 2);
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillPopup(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::LEFT_TO_RIGHT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords)),
          _));

  flow().RunFlow(MakeFieldRendererId(), bounds, TextDirection::LEFT_TO_RIGHT);
}

// Test that the suggestions are shown when the flow is invoked before the
// suggestions were read from disk.
TEST_F(PasswordManualFallbackFlowTest, InvokeFlow_ReturnSuggestions) {
  const gfx::RectF bounds(1, 1, 2, 2);
  flow().RunFlow(MakeFieldRendererId(), bounds, TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillPopup(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::LEFT_TO_RIGHT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords)),
          _));

  ProcessPasswordStoreUpdates();
}

// Test that the suggestions are shown using the last parameters passed to
// `RunFlow` when the suggestions are read from disk by the
// `SavedPasswordsPresenter`.
TEST_F(PasswordManualFallbackFlowTest, LastRunParametersAreUsed) {
  const gfx::RectF bounds_1(1, 1, 2, 2);
  const gfx::RectF bounds_2(2, 2, 4, 4);
  flow().RunFlow(MakeFieldRendererId(), bounds_1, TextDirection::LEFT_TO_RIGHT);
  flow().RunFlow(MakeFieldRendererId(), bounds_2, TextDirection::RIGHT_TO_LEFT);

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillPopup(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds_2),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::RIGHT_TO_LEFT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords)),
          _));
  ProcessPasswordStoreUpdates();
}

// Manual fallback flow can be invoked multiple times. This test verifies that
// the Autofill popup is opened multiple times in this case given that the
// passwords were retrieved from disk.
TEST_F(PasswordManualFallbackFlowTest, RunFlowMultipleTimes) {
  ProcessPasswordStoreUpdates();

  const gfx::RectF bounds_1(1, 1, 2, 2);
  const gfx::RectF bounds_2(2, 2, 4, 4);
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillPopup(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds_1),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::LEFT_TO_RIGHT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords)),
          _));
  flow().RunFlow(MakeFieldRendererId(), bounds_1, TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillPopup(
          AllOf(
              Field("element_bounds",
                    &AutofillClient::PopupOpenArgs::element_bounds, bounds_2),
              Field("text_direction",
                    &AutofillClient::PopupOpenArgs::text_direction,
                    TextDirection::RIGHT_TO_LEFT),
              Field("trigger_source",
                    &AutofillClient::PopupOpenArgs::trigger_source,
                    AutofillSuggestionTriggerSource::kManualFallbackPasswords)),
          _));
  flow().RunFlow(MakeFieldRendererId(), bounds_2, TextDirection::RIGHT_TO_LEFT);
}

// Test that username field-by-field suggestion is previewed into the correct
// field by the manual fallback flow.
TEST_F(PasswordManualFallbackFlowTest, SelectUsernameFieldByFieldSuggestion) {
  ProcessPasswordStoreUpdates();

  const FieldRendererId field_id = MakeFieldRendererId();
  flow().RunFlow(field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(driver(),
              PreviewField(field_id, std::u16string(u"username@example.com")));
  flow().DidSelectSuggestion(autofill::test::CreateAutofillSuggestion(
      PopupItemId::kPasswordFieldByFieldFilling, u"username@example.com"));
}

// Test that username field-by-field suggestion is filled into the correct field
// by the manual fallback flow.
TEST_F(PasswordManualFallbackFlowTest, AcceptUsernameFieldByFieldSuggestion) {
  ProcessPasswordStoreUpdates();

  const FieldRendererId field_id = MakeFieldRendererId();
  flow().RunFlow(field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(driver(),
              FillField(field_id, std::u16string(u"username@example.com")));
  EXPECT_CALL(autofill_client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  flow().DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          PopupItemId::kPasswordFieldByFieldFilling, u"username@example.com"),
      AutofillPopupDelegate::SuggestionPosition{.row = 0,
                                                .sub_popup_level = 1});
}

// Test that "Fill password" field-by-field suggestion is not previewed by the
// manual fallback flow.
TEST_F(PasswordManualFallbackFlowTest,
       SelectFillPasswordFieldByFieldSuggestion) {
  ProcessPasswordStoreUpdates();

  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(driver(), PreviewField).Times(0);
  flow().DidSelectSuggestion(autofill::test::CreateAutofillSuggestion(
      PopupItemId::kFillPassword, u"Fill password",
      Suggestion::ValueToFill(u"password")));
}

// Tests that the password value is filled if the authentication is not
// available. This can happen if it's not implemented for a particular platform.
TEST_F(PasswordManualFallbackFlowTest, FillsPasswordIfAuthNotAvailable) {
  ProcessPasswordStoreUpdates();

  FieldRendererId field_id = MakeFieldRendererId();
  flow().RunFlow(field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(password_manager_client(), CanUseBiometricAuthForFilling)
      .WillOnce(Return(false));
  EXPECT_CALL(driver(), FillField(field_id, std::u16string(u"password")));
  flow().DidAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                                 PopupItemId::kFillPassword, u"Fill password",
                                 Suggestion::ValueToFill(u"password")),
                             AutofillPopupDelegate::SuggestionPosition{
                                 .row = 0, .sub_popup_level = 1});
}

// Tests that password value if not filled if the authentication fails.
TEST_F(PasswordManualFallbackFlowTest, NoFillingIfAuthNotFails) {
  ProcessPasswordStoreUpdates();

  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/false));

  EXPECT_CALL(password_manager_client(), CanUseBiometricAuthForFilling)
      .WillOnce(Return(true));
  EXPECT_CALL(password_manager_client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  EXPECT_CALL(driver(), FillField).Times(0);
  base::HistogramTester histograms;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers_;
  flow().DidAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                                 PopupItemId::kFillPassword, u"Fill password",
                                 Suggestion::ValueToFill(u"password")),
                             AutofillPopupDelegate::SuggestionPosition{
                                 .row = 0, .sub_popup_level = 1});
  const int64_t kMockElapsedTime =
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds();
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationResult", false, 1);
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationTime", kMockElapsedTime,
      1);
}

// Tests that password value is filled if the authentication succeeds.
TEST_F(PasswordManualFallbackFlowTest, FillsPasswordIfAuthSucceeds) {
  ProcessPasswordStoreUpdates();

  FieldRendererId field_id = MakeFieldRendererId();
  flow().RunFlow(field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/true));

  EXPECT_CALL(password_manager_client(), CanUseBiometricAuthForFilling)
      .WillOnce(Return(true));
  EXPECT_CALL(password_manager_client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  EXPECT_CALL(driver(), FillField(field_id, std::u16string(u"password")));
  base::HistogramTester histograms;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers_;
  flow().DidAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                                 PopupItemId::kFillPassword, u"Fill password",
                                 Suggestion::ValueToFill(u"password")),
                             AutofillPopupDelegate::SuggestionPosition{
                                 .row = 0, .sub_popup_level = 1});
  const int64_t kMockElapsedTime =
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds();
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationResult", true, 1);
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordFilling.AuthenticationTime", kMockElapsedTime,
      1);
}

// Test that unfinished authentication is cancelled if the "Fill password"
// suggestion is accepted again.
TEST_F(PasswordManualFallbackFlowTest, CancelsAuthIfPreviousNotFinished) {
  ProcessPasswordStoreUpdates();

  FieldRendererId field_id = MakeFieldRendererId();
  flow().RunFlow(field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);

  auto authenticator1 =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto authenticator1_ptr = authenticator1.get();
  EXPECT_CALL(*authenticator1, AuthenticateWithMessage);

  auto authenticator2 =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator2, AuthenticateWithMessage);

  EXPECT_CALL(password_manager_client(), CanUseBiometricAuthForFilling)
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(password_manager_client(), GetDeviceAuthenticator)
      .Times(2)
      .WillOnce(Return(testing::ByMove(std::move(authenticator1))))
      .WillOnce(Return(testing::ByMove(std::move(authenticator2))));
  flow().DidAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                                 PopupItemId::kFillPassword, u"Fill password",
                                 Suggestion::ValueToFill(u"password")),
                             AutofillPopupDelegate::SuggestionPosition{
                                 .row = 0, .sub_popup_level = 1});

  EXPECT_CALL(*authenticator1_ptr, Cancel);
  flow().DidAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                                 PopupItemId::kFillPassword, u"Fill password",
                                 Suggestion::ValueToFill(u"password")),
                             AutofillPopupDelegate::SuggestionPosition{
                                 .row = 0, .sub_popup_level = 1});
}

// Test that unfinished authentication is cancelled if the flow object is
// destroyed.
TEST_F(PasswordManualFallbackFlowTest, CancelsAuthOnDestroy) {
  ProcessPasswordStoreUpdates();

  FieldRendererId field_id = MakeFieldRendererId();
  flow().RunFlow(field_id, gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto authenticator_ptr = authenticator.get();
  EXPECT_CALL(*authenticator, AuthenticateWithMessage);

  EXPECT_CALL(password_manager_client(), CanUseBiometricAuthForFilling)
      .WillOnce(Return(true));
  EXPECT_CALL(password_manager_client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));
  flow().DidAcceptSuggestion(autofill::test::CreateAutofillSuggestion(
                                 PopupItemId::kFillPassword, u"Fill password",
                                 Suggestion::ValueToFill(u"password")),
                             AutofillPopupDelegate::SuggestionPosition{
                                 .row = 0, .sub_popup_level = 1});

  EXPECT_CALL(*authenticator_ptr, Cancel);
}

// Test that selecting "Manage passwords" suggestion doesn't trigger navigation.
TEST_F(PasswordManualFallbackFlowTest, SelectManagePasswordsEntry) {
  ProcessPasswordStoreUpdates();
  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(password_manager_client(), NavigateToManagePasswordsPage)
      .Times(0);
  base::HistogramTester histograms;
  flow().DidSelectSuggestion(autofill::test::CreateAutofillSuggestion(
      PopupItemId::kAllSavedPasswordsEntry, u"Manage passwords"));
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordDropdownItemSelected",
      metrics_util::PasswordDropdownSelectedOption::kShowAll, 0);
}

// Test that selecting "Manage passwords" suggestion triggers page navigation
// and metric recording.
TEST_F(PasswordManualFallbackFlowTest, AcceptManagePasswordsEntry) {
  ProcessPasswordStoreUpdates();
  flow().RunFlow(MakeFieldRendererId(), gfx::RectF{},
                 TextDirection::LEFT_TO_RIGHT);

  EXPECT_CALL(password_manager_client(),
              NavigateToManagePasswordsPage(
                  ManagePasswordsReferrer::kPasswordDropdown));
  base::HistogramTester histograms;
  flow().DidAcceptSuggestion(
      autofill::test::CreateAutofillSuggestion(
          PopupItemId::kAllSavedPasswordsEntry, u"Manage passwords"),
      AutofillPopupDelegate::SuggestionPosition{.row = 1,
                                                .sub_popup_level = 0});
  histograms.ExpectUniqueSample(
      "PasswordManager.PasswordDropdownItemSelected",
      metrics_util::PasswordDropdownSelectedOption::kShowAll, 1);
}

}  // namespace password_manager
