// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/email_verifier_delegate.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_driver_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class MockEmailVerifier : public content::webid::EmailVerifier {
 public:
  MOCK_METHOD(void,
              Verify,
              (const std::string&, const std::string&, OnEmailVerifiedCallback),
              (override));
};

class MockAutofillDriver : public TestAutofillDriver {
 public:
  using TestAutofillDriver::TestAutofillDriver;
  MOCK_METHOD(void,
              DispatchEmailVerifiedEvent,
              (FieldGlobalId field_id, const std::string& presentation_token),
              (override));
};

FormData ValidForm() {
  return test::GetFormData(
      {.fields = {
           {.label = u"Email",
            .name = u"email",
            .nonce = u"test_nonce",
            .value = u"Triggering field (filled)",
            .form_control_type = FormControlType::kInputEmail},
       }});
}

}  // namespace

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(void, ShowEmailVerifiedToast, (), (override));
};

class EmailVerifierDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    driver_ = std::make_unique<NiceMock<MockAutofillDriver>>(&client_);
    manager_ = std::make_unique<TestBrowserAutofillManager>(driver_.get());
    email_verifier_ = std::make_unique<NiceMock<MockEmailVerifier>>();
    delegate_ = std::make_unique<EmailVerifierDelegate>(
        &client_,
        base::BindRepeating([](content::webid::EmailVerifier* verifier,
                               AutofillManager&) { return verifier; },
                            email_verifier_.get()));
  }

  void TearDown() override {
    delegate_.reset();
    email_verifier_.reset();
    manager_.reset();
    AutofillDriverTestApi(driver_.get())
        .SetLifecycleState(AutofillDriver::LifecycleState::kPendingDeletion);
    driver_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;

  MockAutofillClient client_;
  std::unique_ptr<MockAutofillDriver> driver_;
  std::unique_ptr<TestBrowserAutofillManager> manager_;
  std::unique_ptr<MockEmailVerifier> email_verifier_;
  std::unique_ptr<EmailVerifierDelegate> delegate_;
};

// Verifies that the success test case works as expected: the form conforms to
// all requirements, the user autofills an email field and the
// renderer is notified with the presentation token to dispatch an event.
TEST_F(EmailVerifierDelegateTest, VerificationTriggered) {
  base::test::ScopedFeatureList feature_list{::features::kFedCmDelegation};

  FormData form_data = ValidForm();

  manager_->AddSeenForm(form_data, {EMAIL_ADDRESS});
  FormStructure* form = manager_->FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);
  form->field(0)->set_autofilled_type(EMAIL_ADDRESS);

  EXPECT_CALL(*email_verifier_, Verify("test@example.com", "test_nonce", _))
      .WillOnce(RunOnceCallback<2>("test_token"));

  EXPECT_CALL(*driver_, DispatchEmailVerifiedEvent(form->field(0)->global_id(),
                                                   "test_token"));
  EXPECT_CALL(client_, ShowEmailVerifiedToast);

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(EMAIL_ADDRESS, u"test@example.com");

  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};
  delegate_->OnFillOrPreviewForm(*manager_, form->global_id(),
                                 mojom::ActionPersistence::kFill,
                                 filled_field_ids, &profile);
}

// Verifies that if the feature is disabled, no verification is triggered.
TEST_F(EmailVerifierDelegateTest, FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(::features::kFedCmDelegation);

  FormData form_data = ValidForm();

  manager_->AddSeenForm(form_data, {EMAIL_ADDRESS});
  FormStructure* form = manager_->FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);

  EXPECT_CALL(*email_verifier_, Verify).Times(0);
  EXPECT_CALL(*driver_, DispatchEmailVerifiedEvent).Times(0);
  EXPECT_CALL(client_, ShowEmailVerifiedToast).Times(0);
  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};
  AutofillProfile profile = test::GetFullProfile();
  delegate_->OnFillOrPreviewForm(*manager_, form->global_id(),
                                 mojom::ActionPersistence::kFill,
                                 filled_field_ids, &profile);
}

// Verifies that if the action is not "fill", no verification is triggered.
TEST_F(EmailVerifierDelegateTest, NotFillAction) {
  base::test::ScopedFeatureList feature_list{::features::kFedCmDelegation};

  FormData form_data = ValidForm();

  manager_->AddSeenForm(form_data, {EMAIL_ADDRESS});
  FormStructure* form = manager_->FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);

  EXPECT_CALL(*email_verifier_, Verify).Times(0);
  EXPECT_CALL(*driver_, DispatchEmailVerifiedEvent).Times(0);
  EXPECT_CALL(client_, ShowEmailVerifiedToast).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};
  delegate_->OnFillOrPreviewForm(*manager_, form->global_id(),
                                 mojom::ActionPersistence::kPreview,
                                 filled_field_ids, &profile);
}

// Verifies that if the form isn't comformant (no nonce), no verification is
// triggered.
TEST_F(EmailVerifierDelegateTest, NoNonce) {
  base::test::ScopedFeatureList feature_list{::features::kFedCmDelegation};

  FormData form_data = test::GetFormData(
      {.fields = {
           {.role = EMAIL_ADDRESS,
            .label = u"Email",
            .name = u"email",
            .value = u"Triggering field (filled)",
            .form_control_type = FormControlType::kInputEmail},
       }});

  manager_->AddSeenForm(form_data, {EMAIL_ADDRESS});
  FormStructure* form = manager_->FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);

  EXPECT_CALL(*email_verifier_, Verify).Times(0);

  EXPECT_CALL(*driver_, DispatchEmailVerifiedEvent).Times(0);
  EXPECT_CALL(client_, ShowEmailVerifiedToast).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};
  delegate_->OnFillOrPreviewForm(*manager_, form->global_id(),
                                 mojom::ActionPersistence::kFill,
                                 filled_field_ids, &profile);
}

// Verifies that if the filled field is not an email field, no verification is
// triggered.
TEST_F(EmailVerifierDelegateTest, NotEmailField) {
  base::test::ScopedFeatureList feature_list{::features::kFedCmDelegation};

  FormData form_data =
      test::GetFormData({.fields = {
                             {.label = u"Email",
                              .name = u"email",
                              .nonce = u"test_nonce",
                              .value = u"Triggering field (filled)"},
                         }});

  manager_->AddSeenForm(form_data, {NAME_FULL});
  FormStructure* form = manager_->FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);

  EXPECT_CALL(*email_verifier_, Verify).Times(0);

  EXPECT_CALL(*driver_, DispatchEmailVerifiedEvent).Times(0);
  EXPECT_CALL(client_, ShowEmailVerifiedToast).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};
  delegate_->OnFillOrPreviewForm(*manager_, form->global_id(),
                                 mojom::ActionPersistence::kFill,
                                 filled_field_ids, &profile);
}

// Verifies that if the verification fails, no event is dispatched to the
// renderer.
TEST_F(EmailVerifierDelegateTest, VerificationFails) {
  base::test::ScopedFeatureList feature_list{::features::kFedCmDelegation};

  FormData form_data = ValidForm();

  manager_->AddSeenForm(form_data, {EMAIL_ADDRESS});
  FormStructure* form = manager_->FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);
  form->field(0)->set_autofilled_type(EMAIL_ADDRESS);

  EXPECT_CALL(*email_verifier_, Verify)
      .WillOnce(RunOnceCallback<2>(std::nullopt));

  // When the verification fails, the event is not dispatched.
  EXPECT_CALL(*driver_, DispatchEmailVerifiedEvent).Times(0);
  EXPECT_CALL(client_, ShowEmailVerifiedToast).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};
  delegate_->OnFillOrPreviewForm(*manager_, form->global_id(),
                                 mojom::ActionPersistence::kFill,
                                 filled_field_ids, &profile);
}

}  // namespace autofill
