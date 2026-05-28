// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/email_verifier_delegate.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_driver_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/strike_databases/email_verification_strike_database.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"

namespace autofill {

namespace {

using ::base::test::RunOnceCallback;
using ::content::webid::EmailVerifier;
using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;

class MockEmailVerifier : public EmailVerifier {
 public:
  MOCK_METHOD(void,
              CheckIfVerifiable,
              (const std::string&, IsVerifiableCallback),
              (override));
  MOCK_METHOD(void,
              Verify,
              (const Result&, const std::string&, OnEmailVerifiedCallback),
              (override));
};

class MockAutofillDriver : public TestContentAutofillDriver {
 public:
  using TestContentAutofillDriver::TestContentAutofillDriver;
  MOCK_METHOD(void,
              SendEmailVerificationToken,
              (FieldGlobalId email_field_id,
               const std::string& email,
               FieldGlobalId token_field_id,
               const std::string& presentation_token),
              (override));
};

class TestRuntimeFeatureStateContext
    : public blink::RuntimeFeatureStateContext {
 public:
  TestRuntimeFeatureStateContext() {
    feature_overrides_
        [blink::mojom::RuntimeFeature::kEmailVerificationProtocol] = true;
  }
};

}  // namespace

class MockAutofillClient : public TestContentAutofillClient {
 public:
  using TestContentAutofillClient::TestContentAutofillClient;
  MOCK_METHOD(void, ShowEmailVerifiedToast, (), (override));
  MOCK_METHOD(void,
              ShowEmailVerificationPopup,
              (const gfx::RectF&,
               const net::SchemefulSite&,
               const std::u16string&,
               base::OnceCallback<void(bool)>),
              (override));

  EmailVerifierDelegate& delegate() { return *delegate_; }

 private:
  std::unique_ptr<EmailVerifierDelegate> delegate_ =
      std::make_unique<EmailVerifierDelegate>(this);
};

class EmailVerifierDelegateTestBase
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://a.test/"));
    driver().SetLocalFrameToken(LocalFrameToken(*main_rfh()->GetFrameToken()));
    EmailVerifier::SetForFrameForTest(
        main_rfh(), std::make_unique<NiceMock<MockEmailVerifier>>());

    // Delete the default DocumentData created during NavigateAndCommit, and
    // replace it with our custom context where EmailVerificationProtocol is
    // enabled.
    if (content::RuntimeFeatureStateDocumentData::GetForCurrentDocument(
            main_rfh())) {
      content::RuntimeFeatureStateDocumentData::DeleteForCurrentDocument(
          main_rfh());
    }
    content::RuntimeFeatureStateDocumentData::CreateForCurrentDocument(
        main_rfh(), TestRuntimeFeatureStateContext());
  }

  MockAutofillClient& client() {
    return *autofill_client_injector_[web_contents()];
  }

  EmailVerifierDelegate& delegate() { return client().delegate(); }

  MockAutofillDriver& driver(content::RenderFrameHost* rfh = nullptr) {
    return *autofill_driver_injector_[rfh ? rfh : main_rfh()];
  }

  TestBrowserAutofillManager& manager(content::RenderFrameHost* rfh = nullptr) {
    return *autofill_manager_injector_[rfh ? rfh : main_rfh()];
  }

  MockEmailVerifier& email_verifier() {
    return static_cast<MockEmailVerifier&>(
        *EmailVerifier::GetOrCreateForFrame(main_rfh()));
  }

  FormData ValidForm() {
    return test::GetFormData(
        {.description_for_logging = "ValidForm",
         .fields =
             {
                 {.label = u"Email",
                  .name = u"email",
                  .nonce = u"test_nonce",
                  .value = u"Triggering field (filled)",
                  .form_control_type = FormControlType::kInputEmail},
                 {.label = u"Verification Token",
                  .name = u"verification_token",
                  .nonce = u"test_nonce",
                  .autocomplete_attribute = "email-verification-token",
                  .form_control_type =
                      FormControlType::kInputHiddenEmailVerification},
             },
         .host_frame = driver().GetFrameToken()});
  }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<MockAutofillClient> autofill_client_injector_;
  TestAutofillDriverInjector<MockAutofillDriver> autofill_driver_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
  std::unique_ptr<EmailVerifierDelegate> delegate_;
};

class EmailVerifierDelegateTest : public EmailVerifierDelegateTestBase {
 public:
  EmailVerifierDelegateTest() = default;

 private:
  base::test::ScopedFeatureList feature_list_{
      ::features::kEmailVerificationProtocol};
};

// Verifies that the success test case works as expected: the form conforms to
// all requirements, the user autofills an email field and the
// renderer is notified with the presentation token to dispatch an event.
TEST_F(EmailVerifierDelegateTest, VerificationTriggered) {
  FormData form_data = ValidForm();

  manager().AddSeenForm(form_data, {EMAIL_ADDRESS, UNKNOWN_TYPE});
  FormStructure* form =
      test_api(manager()).FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);
  form->field(0)->set_autofilled_type(EMAIL_ADDRESS);

  EmailVerifier::Result verifiable_result;
  verifiable_result.email = "johndoe@hades.com";
  verifiable_result.issuer_site =
      net::SchemefulSite(GURL("https://example.com"));

  EXPECT_CALL(email_verifier(), CheckIfVerifiable("johndoe@hades.com", _))
      .WillOnce(RunOnceCallback<1>(verifiable_result));

  EXPECT_CALL(email_verifier(), Verify(_, "test_nonce", _))
      .WillOnce(RunOnceCallback<2>("test_token"));

  base::RunLoop popup_shown_run_loop;
  EXPECT_CALL(client(), ShowEmailVerificationPopup)
      .WillOnce(
          DoAll(base::test::RunClosure(popup_shown_run_loop.QuitClosure()),
                RunOnceCallback<3>(true)));

  // This is only called on form submission, which we are not testing here.
  EXPECT_CALL(client(), ShowEmailVerifiedToast).Times(0);
  EXPECT_CALL(driver(), SendEmailVerificationToken(
                            form->field(0)->global_id(), "johndoe@hades.com",
                            form->field(1)->global_id(), "test_token"));

  AutofillProfile profile = test::GetFullProfile();

  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);

  popup_shown_run_loop.Run();
}

// Verifies that if the user declines the prompt, no verification is triggered.
TEST_F(EmailVerifierDelegateTest, VerificationDeclined) {
  FormData form_data = ValidForm();

  std::vector<FieldType> types = {EMAIL_ADDRESS, UNKNOWN_TYPE};
  manager().AddSeenForm(form_data, types);
  FormStructure* form =
      test_api(manager()).FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);
  form->field(0)->set_autofilled_type(EMAIL_ADDRESS);

  EmailVerifier::Result verifiable_result;
  verifiable_result.email = "johndoe@hades.com";
  verifiable_result.issuer_site =
      net::SchemefulSite(GURL("https://example.com"));

  EXPECT_CALL(email_verifier(), CheckIfVerifiable("johndoe@hades.com", _))
      .WillOnce(RunOnceCallback<1>(verifiable_result));

  EXPECT_CALL(email_verifier(), Verify).Times(0);

  base::RunLoop popup_shown_run_loop;
  EXPECT_CALL(client(), ShowEmailVerificationPopup)
      .WillOnce(
          DoAll(base::test::RunClosure(popup_shown_run_loop.QuitClosure()),
                RunOnceCallback<3>(false)));

  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);
  EXPECT_CALL(client(), ShowEmailVerifiedToast).Times(0);

  AutofillProfile profile = test::GetFullProfile();

  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id(), form->field(1)->global_id()};
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);

  popup_shown_run_loop.Run();
}

// Verifies that if the base feature is explicitly overridden to disabled,
// no verification is triggered even if the Blink-side Origin Trial is enabled.
TEST_F(EmailVerifierDelegateTest,
       FeatureOverriddenToDisabledButOriginTrialEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(::features::kEmailVerificationProtocol);

  FormData form_data = ValidForm();

  manager().AddSeenForm(form_data, {EMAIL_ADDRESS, UNKNOWN_TYPE});
  const FormStructure* form =
      manager().FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);

  EXPECT_CALL(email_verifier(), Verify).Times(0);
  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);
  EXPECT_CALL(client(), ShowEmailVerifiedToast).Times(0);
  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};
  AutofillProfile profile = test::GetFullProfile();
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);
}

// Verifies that if the action is not "fill", no verification is triggered.
TEST_F(EmailVerifierDelegateTest, NotFillAction) {
  base::test::ScopedFeatureList feature_list{
      ::features::kEmailVerificationProtocol};

  FormData form_data = ValidForm();

  manager().AddSeenForm(form_data, {EMAIL_ADDRESS, UNKNOWN_TYPE});
  const FormStructure* form =
      manager().FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);

  EXPECT_CALL(email_verifier(), Verify).Times(0);
  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);
  EXPECT_CALL(client(), ShowEmailVerifiedToast).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kPreview, filled_field_ids, &profile);
}

// Verifies that if the form isn't comformant (no nonce), no verification is
// triggered.
TEST_F(EmailVerifierDelegateTest, NoNonce) {
  base::test::ScopedFeatureList feature_list{
      ::features::kEmailVerificationProtocol};

  FormData form_data = test::GetFormData(
      {.fields = {
           {.role = EMAIL_ADDRESS,
            .label = u"Email",
            .name = u"email",
            .value = u"Triggering field (filled)",
            .form_control_type = FormControlType::kInputEmail},
       }});

  manager().AddSeenForm(form_data, {EMAIL_ADDRESS});
  const FormStructure* form =
      manager().FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);

  EXPECT_CALL(email_verifier(), Verify).Times(0);

  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);
  EXPECT_CALL(client(), ShowEmailVerifiedToast).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);
}

// Verifies that if the filled field is not an email field, no verification is
// triggered.
TEST_F(EmailVerifierDelegateTest, NotEmailField) {
  base::test::ScopedFeatureList feature_list{
      ::features::kEmailVerificationProtocol};

  FormData form_data =
      test::GetFormData({.fields = {
                             {.label = u"Email",
                              .name = u"email",
                              .nonce = u"test_nonce",
                              .value = u"Triggering field (filled)"},
                         }});

  manager().AddSeenForm(form_data, {NAME_FULL});
  const FormStructure* form =
      manager().FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);

  EXPECT_CALL(email_verifier(), Verify).Times(0);

  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);
  EXPECT_CALL(client(), ShowEmailVerifiedToast).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);
}

// Verifies that if the verification fails, no event is dispatched to the
// renderer.
TEST_F(EmailVerifierDelegateTest, VerificationFails) {
  base::test::ScopedFeatureList feature_list{
      ::features::kEmailVerificationProtocol};

  FormData form_data = ValidForm();

  manager().AddSeenForm(form_data, {EMAIL_ADDRESS, UNKNOWN_TYPE});
  FormStructure* form =
      test_api(manager()).FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);
  form->field(0)->set_autofilled_type(EMAIL_ADDRESS);

  EmailVerifier::Result verifiable_result;
  verifiable_result.email = "test@example.com";
  verifiable_result.issuer_site =
      net::SchemefulSite(GURL("https://example.com"));

  EXPECT_CALL(email_verifier(), CheckIfVerifiable("test@example.com", _))
      .WillOnce(RunOnceCallback<1>(verifiable_result));

  base::RunLoop verify_called_run_loop;
  EXPECT_CALL(email_verifier(), Verify)
      .WillOnce(
          DoAll(base::test::RunClosure(verify_called_run_loop.QuitClosure()),
                RunOnceCallback<2>(std::nullopt)));

  EXPECT_CALL(client(), ShowEmailVerificationPopup)
      .WillOnce([&](const gfx::RectF&, const net::SchemefulSite&,
                    const std::u16string&,
                    base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });

  // When the verification fails, the event is not dispatched.
  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);
  EXPECT_CALL(client(), ShowEmailVerifiedToast).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  profile.SetInfoWithVerificationStatus(EMAIL_ADDRESS, u"test@example.com",
                                        "en-US",
                                        VerificationStatus::kUserVerified);
  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id(), form->field(1)->global_id()};
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);

  verify_called_run_loop.Run();
}

// Verifies that if the base feature is in its default state (enabled by
// default, not overridden) but the Blink-side Origin Trial is not enabled,
// no verification is triggered.
TEST_F(EmailVerifierDelegateTestBase, OriginTrialNotEnabledWithoutOverride) {
  // Replace the document data with the default context where the Origin Trial
  // is disabled.
  if (content::RuntimeFeatureStateDocumentData::GetForCurrentDocument(
          main_rfh())) {
    content::RuntimeFeatureStateDocumentData::DeleteForCurrentDocument(
        main_rfh());
  }
  content::RuntimeFeatureStateDocumentData::CreateForCurrentDocument(
      main_rfh(), blink::RuntimeFeatureStateContext());

  FormData form_data = ValidForm();

  manager().AddSeenForm(form_data, {EMAIL_ADDRESS, UNKNOWN_TYPE});
  FormStructure* form =
      test_api(manager()).FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);
  form->field(0)->set_autofilled_type(EMAIL_ADDRESS);

  // Verify that Verify and ShowEmailVerifiedToast are never called.
  EXPECT_CALL(email_verifier(), Verify).Times(0);
  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);
  EXPECT_CALL(client(), ShowEmailVerifiedToast).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(EMAIL_ADDRESS, u"test@example.com");

  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);
}

// Verifies that if the trigger field is NOT the email field, no verification is
// triggered.
TEST_F(EmailVerifierDelegateTest, NotEmailTriggerField) {
  base::test::ScopedFeatureList feature_list{
      ::features::kEmailVerificationProtocol};

  FormData form_data = ValidForm();

  manager().AddSeenForm(form_data, {EMAIL_ADDRESS, UNKNOWN_TYPE});
  FormStructure* form =
      test_api(manager()).FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);
  form->field(0)->set_autofilled_type(EMAIL_ADDRESS);

  // Since the trigger field is form->field(1) (which is NOT the email field),
  // Verify and SendEmailVerificationToken should not be called.
  EXPECT_CALL(email_verifier(), Verify).Times(0);
  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);
  EXPECT_CALL(client(), ShowEmailVerifiedToast).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(EMAIL_ADDRESS, u"test@example.com");

  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id(), form->field(1)->global_id()};
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(1)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);
}

// Verifies that if the base feature is explicitly overridden to enabled,
// verification is triggered even if the Blink-side Origin Trial is not
// enabled.
TEST_F(EmailVerifierDelegateTest,
       OriginTrialNotEnabledButFeatureOverriddenToEnabled) {
  base::test::ScopedFeatureList feature_list{
      ::features::kEmailVerificationProtocol};

  // Replace the document data with the default context where the Origin Trial
  // is disabled.
  if (content::RuntimeFeatureStateDocumentData::GetForCurrentDocument(
          main_rfh())) {
    content::RuntimeFeatureStateDocumentData::DeleteForCurrentDocument(
        main_rfh());
  }
  content::RuntimeFeatureStateDocumentData::CreateForCurrentDocument(
      main_rfh(), blink::RuntimeFeatureStateContext());

  FormData form_data = ValidForm();

  manager().AddSeenForm(form_data, {EMAIL_ADDRESS, UNKNOWN_TYPE});
  FormStructure* form =
      test_api(manager()).FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);
  form->field(0)->set_autofilled_type(EMAIL_ADDRESS);

  EmailVerifier::Result verifiable_result;
  verifiable_result.email = "test@example.com";
  verifiable_result.issuer_site =
      net::SchemefulSite(GURL("https://example.com"));

  EXPECT_CALL(email_verifier(), CheckIfVerifiable("test@example.com", _))
      .WillOnce(RunOnceCallback<1>(verifiable_result));

  EXPECT_CALL(email_verifier(), Verify(_, "test_nonce", _))
      .WillOnce(RunOnceCallback<2>("test_token"));

  base::RunLoop popup_shown_run_loop;
  EXPECT_CALL(client(), ShowEmailVerificationPopup)
      .WillOnce(
          DoAll(base::test::RunClosure(popup_shown_run_loop.QuitClosure()),
                RunOnceCallback<3>(true)));

  EXPECT_CALL(driver(), SendEmailVerificationToken(
                            form->field(0)->global_id(), "test@example.com",
                            form->field(1)->global_id(), "test_token"));

  AutofillProfile profile = test::GetFullProfile();
  profile.SetInfoWithVerificationStatus(EMAIL_ADDRESS, u"test@example.com",
                                        "en-US",
                                        VerificationStatus::kUserVerified);

  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id(), form->field(1)->global_id()};
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);

  popup_shown_run_loop.Run();
}

TEST_F(EmailVerifierDelegateTest, BlockedByStrikes) {
  base::test::ScopedFeatureList feature_list{
      ::features::kEmailVerificationProtocol};

  FormData form_data = ValidForm();

  manager().AddSeenForm(form_data, {EMAIL_ADDRESS, UNKNOWN_TYPE});
  FormStructure* form =
      test_api(manager()).FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);
  form->field(0)->set_autofilled_type(EMAIL_ADDRESS);

  // Set up strike database.
  client().set_test_strike_database(std::make_unique<TestStrikeDatabase>());
  EmailVerificationStrikeDatabase strike_db(client().GetStrikeDatabase());
  strike_db.AddStrikes(
      3, EmailVerificationStrikeDatabase::GetId("test@example.com"));

  // Verify and ShowEmailVerificationPopup should NOT be called!
  EXPECT_CALL(email_verifier(), Verify).Times(0);
  EXPECT_CALL(client(), ShowEmailVerificationPopup).Times(0);
  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);
  EXPECT_CALL(client(), ShowEmailVerifiedToast).Times(0);

  AutofillProfile profile = test::GetFullProfile();
  profile.SetInfoWithVerificationStatus(EMAIL_ADDRESS, u"test@example.com",
                                        "en-US",
                                        VerificationStatus::kUserVerified);

  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};

  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);
}

TEST_F(EmailVerifierDelegateTest, ClearsStrikesOnAccept) {
  base::test::ScopedFeatureList feature_list{
      ::features::kEmailVerificationProtocol};

  FormData form_data = ValidForm();

  manager().AddSeenForm(form_data, {EMAIL_ADDRESS, UNKNOWN_TYPE});
  FormStructure* form =
      test_api(manager()).FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);
  form->field(0)->set_autofilled_type(EMAIL_ADDRESS);

  // Set up strike database with some strikes (less than limit).
  client().set_test_strike_database(std::make_unique<TestStrikeDatabase>());
  EmailVerificationStrikeDatabase strike_db(client().GetStrikeDatabase());
  std::string email_utf8 = "johndoe@hades.com";

  strike_db.AddStrikes(2, EmailVerificationStrikeDatabase::GetId(email_utf8));
  ASSERT_FALSE(strike_db.ShouldBlockFeature(
      EmailVerificationStrikeDatabase::GetId(email_utf8)));

  EmailVerifier::Result verifiable_result;
  verifiable_result.email = "johndoe@hades.com";
  verifiable_result.issuer_site =
      net::SchemefulSite(GURL("https://example.com"));

  EXPECT_CALL(email_verifier(), CheckIfVerifiable("johndoe@hades.com", _))
      .WillOnce(RunOnceCallback<1>(verifiable_result));

  EXPECT_CALL(email_verifier(), Verify(_, "test_nonce", _))
      .WillOnce(RunOnceCallback<2>("test_token"));

  base::RunLoop popup_shown_run_loop;
  EXPECT_CALL(client(), ShowEmailVerificationPopup)
      .WillOnce(
          DoAll(base::test::RunClosure(popup_shown_run_loop.QuitClosure()),
                RunOnceCallback<3>(true)));

  EXPECT_CALL(driver(), SendEmailVerificationToken(
                            form->field(0)->global_id(), "johndoe@hades.com",
                            form->field(1)->global_id(), "test_token"));

  AutofillProfile profile = test::GetFullProfile();
  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id()};

  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);

  popup_shown_run_loop.Run();

  // Verify that strikes are cleared.
  EXPECT_EQ(
      strike_db.GetStrikes(EmailVerificationStrikeDatabase::GetId(email_utf8)),
      0);
}

}  // namespace autofill
