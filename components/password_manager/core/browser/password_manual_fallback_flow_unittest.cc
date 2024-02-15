// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manual_fallback_flow.h"

#include <vector>

#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

using autofill::AutofillClient;
using autofill::AutofillSuggestionTriggerSource;
using autofill::TestAutofillClient;
using base::i18n::TextDirection;
using testing::_;
using testing::AllOf;
using testing::Field;
using testing::NiceMock;

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  ~MockAutofillClient() override = default;
  MOCK_METHOD(void,
              ShowAutofillPopup,
              (const AutofillClient::PopupOpenArgs&,
               base::WeakPtr<autofill::AutofillPopupDelegate>),
              (override));
};

class PasswordManualFallbackFlowTest : public ::testing::Test {
 public:
  PasswordManualFallbackFlowTest() {
    profile_password_store().Init(/*prefs=*/nullptr,
                                  /*affiliated_match_helper=*/nullptr);
    // Add 1 password form to the password store.
    std::unique_ptr<PasswordForm> form = CreateEntry(
        "username@example.com", "password", GURL("https://google.com/"),
        PasswordForm::MatchType::kExact);
    profile_password_store().AddLogin(*form);

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

  StubPasswordManagerDriver& driver() { return *driver_; }

  MockAutofillClient& autofill_client() { return *autofill_client_; }

  StubPasswordManagerClient& password_manager_client() {
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
  std::unique_ptr<StubPasswordManagerDriver> driver_ =
      std::make_unique<StubPasswordManagerDriver>();
  std::unique_ptr<NiceMock<MockAutofillClient>> autofill_client_ =
      std::make_unique<NiceMock<MockAutofillClient>>();
  std::unique_ptr<StubPasswordManagerClient> password_manager_client_ =
      std::make_unique<StubPasswordManagerClient>();
  std::unique_ptr<affiliations::FakeAffiliationService> affiliation_service_ =
      std::make_unique<affiliations::FakeAffiliationService>();
  scoped_refptr<TestPasswordStore> profile_password_store_ =
      base::MakeRefCounted<TestPasswordStore>();
  std::unique_ptr<PasswordManualFallbackFlow> flow_;
};

// Test that no suggestions are shown before the passwords are read from disk.
TEST_F(PasswordManualFallbackFlowTest, RunFlow_NoSuggestionsReturned) {
  EXPECT_CALL(autofill_client(), ShowAutofillPopup).Times(0);

  flow().RunFlow(gfx::RectF{}, TextDirection::LEFT_TO_RIGHT);
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

  flow().RunFlow(bounds, TextDirection::LEFT_TO_RIGHT);
}

// Test that the suggestions are shown when the flow is invoked before the
// suggestions were read from disk.
TEST_F(PasswordManualFallbackFlowTest, InvokeFlow_ReturnSuggestions) {
  const gfx::RectF bounds(1, 1, 2, 2);
  flow().RunFlow(bounds, TextDirection::LEFT_TO_RIGHT);

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
  flow().RunFlow(bounds_1, TextDirection::LEFT_TO_RIGHT);
  flow().RunFlow(bounds_2, TextDirection::RIGHT_TO_LEFT);

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
  flow().RunFlow(bounds_1, TextDirection::LEFT_TO_RIGHT);

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
  flow().RunFlow(bounds_2, TextDirection::RIGHT_TO_LEFT);
}

}  // namespace password_manager
