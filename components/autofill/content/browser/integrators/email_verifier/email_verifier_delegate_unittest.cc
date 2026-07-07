// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/email_verifier/email_verifier_delegate.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
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
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/test_metrics_web_contents_observer_embedder.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace autofill {

namespace {

using ::base::test::RunOnceCallback;
using ::content::webid::EmailVerifier;
using ::testing::_;
using ::testing::AnyNumber;
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
  MOCK_METHOD(void,
              UpdateEmailVerificationState,
              (const FieldGlobalId& email_field_id,
               mojom::EmailVerificationState state),
              (override));
};

class MockEmailVerifierDelegateObserver
    : public EmailVerifierDelegate::Observer {
 public:
  MOCK_METHOD(void, OnFlowCompleted, (EvpAutofillFlowResult), (override));
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
  MOCK_METHOD(void, ShowEmailVerifiedToast, (const GURL&), (override));
  MOCK_METHOD(void,
              ShowEmailVerificationPopup,
              (const gfx::RectF&,
               const net::SchemefulSite&,
               const std::u16string&,
               base::OnceCallback<
                   void(AutofillClient::EmailVerificationPermissionUiResult)>),
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
    page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
        web_contents(),
        std::make_unique<
            page_load_metrics::TestMetricsWebContentsObserverEmbedder>());
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

  FormStructure* SetUpValidForm() {
    FormData form_data = ValidForm();
    manager().AddSeenForm(form_data, {EMAIL_ADDRESS, UNKNOWN_TYPE});
    FormStructure* form =
        test_api(manager()).FindCachedFormById(form_data.global_id());
    CHECK(form);
    form->field(0)->set_autofilled_type(EMAIL_ADDRESS);
    return form;
  }

  content::webid::EmailVerifier::Result CreateVerifiableResult(
      const std::string& email = "johndoe@hades.com") {
    content::webid::EmailVerifier::Result result;
    result.email = email;
    result.issuer_site = net::SchemefulSite(GURL("https://example.com"));
    return result;
  }

  void TriggerDefaultFormFill(const FormStructure& form) {
    AutofillProfile profile = test::GetFullProfile();
    base::flat_set<FieldGlobalId> filled_field_ids = {
        form.field(0)->global_id()};
    delegate().OnFillOrPreviewForm(
        manager(), form.global_id(), form.field(0)->global_id(),
        mojom::ActionPersistence::kFill, filled_field_ids, &profile);
  }

  void SetUpVerificationExpectations(
      const FormStructure& form,
      const std::string& email = "johndoe@hades.com",
      AutofillClient::EmailVerificationPermissionUiResult popup_result =
          AutofillClient::EmailVerificationPermissionUiResult::kAccepted) {
    EXPECT_CALL(email_verifier(), CheckIfVerifiable(email, _))
        .WillOnce(RunOnceCallback<1>(CreateVerifiableResult(email)));
    EXPECT_CALL(driver(), UpdateEmailVerificationState(
                              form.field(0)->global_id(),
                              mojom::EmailVerificationState::kLoading))
        .Times(AnyNumber());

    if (popup_result ==
        AutofillClient::EmailVerificationPermissionUiResult::kAccepted) {
      EXPECT_CALL(email_verifier(), Verify(_, "test_nonce", _))
          .WillOnce(
              RunOnceCallback<2>(std::optional<std::string>("test_token")));

      EXPECT_CALL(driver(), SendEmailVerificationToken(
                                form.field(0)->global_id(), email,
                                form.field(1)->global_id(), "test_token"));
      EXPECT_CALL(driver(), UpdateEmailVerificationState(
                                form.field(0)->global_id(),
                                mojom::EmailVerificationState::kVerified));
    } else {
      EXPECT_CALL(email_verifier(), Verify).Times(0);
      EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);
      EXPECT_CALL(driver(), UpdateEmailVerificationState(
                                form.field(0)->global_id(),
                                mojom::EmailVerificationState::kNone));
    }

    EXPECT_CALL(client(), ShowEmailVerificationPopup)
        .WillOnce(
            DoAll(base::test::RunClosure(popup_shown_run_loop_.QuitClosure()),
                  RunOnceCallback<3>(popup_result)));

    EXPECT_CALL(client(), ShowEmailVerifiedToast).Times(0);
  }

 protected:
  base::RunLoop popup_shown_run_loop_;

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
  base::HistogramTester histogram_tester;
  auto* observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents());
  ASSERT_TRUE(observer);
  auto* embedder =
      static_cast<page_load_metrics::TestMetricsWebContentsObserverEmbedder*>(
          observer->GetEmbedderInterfaceForTesting());
  ASSERT_TRUE(embedder);
  FormStructure* form = SetUpValidForm();

  SetUpVerificationExpectations(*form);

  TriggerDefaultFormFill(*form);

  popup_shown_run_loop_.Run();

  bool feature_observed = false;
  for (const blink::UseCounterFeature& feature :
       embedder->observed_features()) {
    if (feature.type() == blink::mojom::UseCounterFeatureType::kWebFeature &&
        feature.value() ==
            static_cast<uint32_t>(
                blink::mojom::WebFeature::kEmailVerificationProtocol)) {
      feature_observed = true;
      break;
    }
  }
  EXPECT_TRUE(feature_observed);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Autofill.FlowResult",
      EvpAutofillFlowResult::kTokenSentToRenderer, 1);
}

TEST_F(EmailVerifierDelegateTest, TokenSharedSuccess) {
  base::HistogramTester histogram_tester;
  FormStructure* form = SetUpValidForm();

  SetUpVerificationExpectations(*form);

  TriggerDefaultFormFill(*form);

  popup_shown_run_loop_.Run();

  histogram_tester.ExpectBucketCount(
      "Blink.Evp.Autofill.FlowResult",
      EvpAutofillFlowResult::kTokenSentToRenderer, 1);
  histogram_tester.ExpectBucketCount("Blink.Evp.Autofill.FlowResult",
                                     EvpAutofillFlowResult::kSuccess, 0);

  // Clear expectations on client to avoid conflict with ShowEmailVerifiedToast.
  testing::Mock::VerifyAndClearExpectations(&client());

  EXPECT_CALL(client(), ShowEmailVerifiedToast(GURL("https://example.com")));
  delegate().OnBeforeFormWithEmailVerificationTokenSubmitted(
      manager(), form->ToFormData(), form->field(1)->global_id());

  histogram_tester.ExpectBucketCount("Blink.Evp.Autofill.FlowResult",
                                     EvpAutofillFlowResult::kSuccess, 1);
}

TEST_F(EmailVerifierDelegateTest, ObserverNotified) {
  FormStructure* form = SetUpValidForm();
  SetUpVerificationExpectations(*form);

  NiceMock<MockEmailVerifierDelegateObserver> observer;
  delegate().AddObserver(&observer);

  EXPECT_CALL(observer,
              OnFlowCompleted(EvpAutofillFlowResult::kTokenSentToRenderer));

  TriggerDefaultFormFill(*form);

  popup_shown_run_loop_.Run();

  delegate().RemoveObserver(&observer);
}

// Verifies that if the user declines the prompt, no verification is triggered.
TEST_F(EmailVerifierDelegateTest, VerificationDeclined) {
  base::HistogramTester histogram_tester;
  FormStructure* form = SetUpValidForm();

  SetUpVerificationExpectations(
      *form, "johndoe@hades.com",
      AutofillClient::EmailVerificationPermissionUiResult::kDeclined);

  client().set_test_strike_database(std::make_unique<TestStrikeDatabase>());
  EmailVerificationStrikeDatabase strike_db(client().GetStrikeDatabase());
  std::string email_utf8 = "johndoe@hades.com";

  AutofillProfile profile = test::GetFullProfile();

  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id(), form->field(1)->global_id()};
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);

  popup_shown_run_loop_.Run();

  // Verify that 1 strike was added.
  EXPECT_EQ(
      strike_db.GetStrikes(EmailVerificationStrikeDatabase::GetId(email_utf8)),
      1);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Autofill.FlowResult",
      EvpAutofillFlowResult::kUserDeclinedPermissionPrompt, 1);
}

// Verifies that if the prompt is dismissed (not declined), no strikes are
// added.
TEST_F(EmailVerifierDelegateTest, VerificationDismissed) {
  base::HistogramTester histogram_tester;
  FormStructure* form = SetUpValidForm();

  SetUpVerificationExpectations(
      *form, "johndoe@hades.com",
      AutofillClient::EmailVerificationPermissionUiResult::kIgnored);

  client().set_test_strike_database(std::make_unique<TestStrikeDatabase>());
  EmailVerificationStrikeDatabase strike_db(client().GetStrikeDatabase());
  std::string email_utf8 = "johndoe@hades.com";

  AutofillProfile profile = test::GetFullProfile();

  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id(), form->field(1)->global_id()};
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);

  popup_shown_run_loop_.Run();

  // Verify that no strike was added.
  EXPECT_EQ(
      strike_db.GetStrikes(EmailVerificationStrikeDatabase::GetId(email_utf8)),
      0);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Autofill.FlowResult",
      EvpAutofillFlowResult::kUserIgnoredPermissionPrompt, 1);
}

// Verifies that if the base feature is explicitly overridden to disabled,
// no verification is triggered even if the Blink-side Origin Trial is enabled.
TEST_F(EmailVerifierDelegateTest,
       FeatureOverriddenToDisabledButOriginTrialEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(::features::kEmailVerificationProtocol);

  auto* observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents());
  ASSERT_TRUE(observer);
  auto* embedder =
      static_cast<page_load_metrics::TestMetricsWebContentsObserverEmbedder*>(
          observer->GetEmbedderInterfaceForTesting());
  ASSERT_TRUE(embedder);

  FormStructure* form = SetUpValidForm();

  EXPECT_CALL(email_verifier(), Verify).Times(0);
  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);
  EXPECT_CALL(client(), ShowEmailVerifiedToast).Times(0);
  TriggerDefaultFormFill(*form);

  bool feature_observed = false;
  for (const blink::UseCounterFeature& feature :
       embedder->observed_features()) {
    if (feature.type() == blink::mojom::UseCounterFeatureType::kWebFeature &&
        feature.value() ==
            static_cast<uint32_t>(
                blink::mojom::WebFeature::kEmailVerificationProtocol)) {
      feature_observed = true;
      break;
    }
  }
  EXPECT_FALSE(feature_observed);
}

// Verifies that if the action is not "fill", no verification is triggered.
TEST_F(EmailVerifierDelegateTest, NotFillAction) {
  base::test::ScopedFeatureList feature_list{
      ::features::kEmailVerificationProtocol};

  FormStructure* form = SetUpValidForm();

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

  TriggerDefaultFormFill(*form);
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

  TriggerDefaultFormFill(*form);
}

// Verifies that if the verification fails, no event is dispatched to the
// renderer.
TEST_F(EmailVerifierDelegateTest, VerificationFails) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list{
      ::features::kEmailVerificationProtocol};

  FormStructure* form = SetUpValidForm();

  EXPECT_CALL(email_verifier(), CheckIfVerifiable("test@example.com", _))
      .WillOnce(RunOnceCallback<1>(CreateVerifiableResult("test@example.com")));

  base::RunLoop verify_called_run_loop;
  EXPECT_CALL(email_verifier(), Verify)
      .WillOnce(
          DoAll(base::test::RunClosure(verify_called_run_loop.QuitClosure()),
                RunOnceCallback<2>(std::nullopt)));

  EXPECT_CALL(client(), ShowEmailVerificationPopup)
      .WillOnce(RunOnceCallback<3>(
          AutofillClient::EmailVerificationPermissionUiResult::kAccepted));

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

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Autofill.FlowResult",
      EvpAutofillFlowResult::kVerificationFailed, 1);
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

  FormStructure* form = SetUpValidForm();

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

  FormStructure* form = SetUpValidForm();

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

  FormStructure* form = SetUpValidForm();

  SetUpVerificationExpectations(*form, "test@example.com");

  AutofillProfile profile = test::GetFullProfile();
  profile.SetInfoWithVerificationStatus(EMAIL_ADDRESS, u"test@example.com",
                                        "en-US",
                                        VerificationStatus::kUserVerified);

  base::flat_set<FieldGlobalId> filled_field_ids = {
      form->field(0)->global_id(), form->field(1)->global_id()};
  delegate().OnFillOrPreviewForm(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, filled_field_ids, &profile);

  popup_shown_run_loop_.Run();
}

TEST_F(EmailVerifierDelegateTest, BlockedByStrikes) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list{
      ::features::kEmailVerificationProtocol};

  FormStructure* form = SetUpValidForm();

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

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Autofill.FlowResult",
      EvpAutofillFlowResult::kStrikeDatabaseBlock, 1);
}

TEST_F(EmailVerifierDelegateTest, ClearsStrikesOnAccept) {
  base::test::ScopedFeatureList feature_list{
      ::features::kEmailVerificationProtocol};

  FormStructure* form = SetUpValidForm();

  // Set up strike database with some strikes (less than limit).
  client().set_test_strike_database(std::make_unique<TestStrikeDatabase>());
  EmailVerificationStrikeDatabase strike_db(client().GetStrikeDatabase());
  std::string email_utf8 = "johndoe@hades.com";

  strike_db.AddStrikes(2, EmailVerificationStrikeDatabase::GetId(email_utf8));
  ASSERT_FALSE(strike_db.ShouldBlockFeature(
      EmailVerificationStrikeDatabase::GetId(email_utf8)));

  SetUpVerificationExpectations(*form);

  TriggerDefaultFormFill(*form);

  popup_shown_run_loop_.Run();

  // Verify that strikes are cleared.
  EXPECT_EQ(
      strike_db.GetStrikes(EmailVerificationStrikeDatabase::GetId(email_utf8)),
      0);
}

TEST_F(EmailVerifierDelegateTest, OnFillOrPreviewFieldVerificationTriggered) {
  FormStructure* form = SetUpValidForm();

  SetUpVerificationExpectations(*form);

  // Simulate autocomplete fill by calling OnFillOrPreviewField.
  // With autocomplete, field_type_used is std::nullopt, but the field's
  // predicted type is EMAIL_ADDRESS.
  delegate().OnFillOrPreviewField(
      manager(), form->global_id(), form->field(0)->global_id(),
      mojom::ActionPersistence::kFill, u"johndoe@hades.com",
      /*field_type_used=*/std::nullopt);

  popup_shown_run_loop_.Run();
}

// Verifies that ShowEmailVerificationPopup receives a valid `issuer_site`
// that is not moved-from (which would trigger a SchemeHostPort::IsValid()
// crash).
TEST_F(EmailVerifierDelegateTest, Regression_ShowPopupReceivesValidIssuerSite) {
  FormStructure* form = SetUpValidForm();

  EXPECT_CALL(email_verifier(), CheckIfVerifiable("johndoe@hades.com", _))
      .WillOnce(RunOnceCallback<1>(CreateVerifiableResult()));

  base::RunLoop run_loop;
  EXPECT_CALL(client(), ShowEmailVerificationPopup)
      .WillOnce([&](const gfx::RectF&, const net::SchemefulSite& issuer_site,
                    const std::u16string&,
                    base::OnceCallback<void(
                        AutofillClient::EmailVerificationPermissionUiResult)>
                        callback) {
        // Access issuer_site to verify it is not moved-from.
        ASSERT_TRUE(issuer_site.GetURL().is_valid());
        std::move(callback).Run(
            AutofillClient::EmailVerificationPermissionUiResult::kDeclined);
        run_loop.Quit();
      });

  TriggerDefaultFormFill(*form);

  run_loop.Run();
}

TEST_F(EmailVerifierDelegateTest, TokenFieldHasNoNonce) {
  base::HistogramTester histogram_tester;
  FormData form_data = test::GetFormData(
      {.description_for_logging = "NoNonceTokenForm",
       .fields =
           {
               {.label = u"Email",
                .name = u"email",
                .nonce = u"test_nonce",
                .value = u"Triggering field (filled)",
                .form_control_type = FormControlType::kInputEmail},
               {.label = u"Verification Token",
                .name = u"verification_token",
                .nonce = u"",  // Empty nonce!
                .autocomplete_attribute = "email-verification-token",
                .form_control_type =
                    FormControlType::kInputHiddenEmailVerification},
           },
       .host_frame = driver().GetFrameToken()});
  manager().AddSeenForm(form_data, {EMAIL_ADDRESS, UNKNOWN_TYPE});
  FormStructure* form =
      test_api(manager()).FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);
  form->field(0)->set_autofilled_type(EMAIL_ADDRESS);

  EXPECT_CALL(email_verifier(), Verify).Times(0);
  EXPECT_CALL(client(), ShowEmailVerificationPopup).Times(0);
  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);

  TriggerDefaultFormFill(*form);

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Autofill.FlowResult",
      EvpAutofillFlowResult::kTokenFieldHasNoNonce, 1);
}

TEST_F(EmailVerifierDelegateTest, UserPrefDisabled) {
  base::HistogramTester histogram_tester;
  FormStructure* form = SetUpValidForm();

  // Disable user pref.
  PrefService* prefs = manager().client().GetPrefs();
  prefs->SetBoolean(prefs::kAutofillEmailVerificationEnabled, false);

  EXPECT_CALL(email_verifier(), Verify).Times(0);
  EXPECT_CALL(client(), ShowEmailVerificationPopup).Times(0);
  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);

  TriggerDefaultFormFill(*form);

  histogram_tester.ExpectUniqueSample("Blink.Evp.Autofill.FlowResult",
                                      EvpAutofillFlowResult::kUserPrefDisabled,
                                      1);
}

// Verifies that if the driver becomes inactive before OnIsVerifiable is called,
// the flow is aborted and no popup is shown.
TEST_F(EmailVerifierDelegateTest, DriverInactiveBeforeIsVerifiable) {
  base::HistogramTester histogram_tester;
  FormStructure* form = SetUpValidForm();

  // Capture the callback to run it asynchronously.
  EmailVerifier::IsVerifiableCallback saved_callback;
  EXPECT_CALL(email_verifier(), CheckIfVerifiable("johndoe@hades.com", _))
      .WillOnce([&](const std::string&,
                    EmailVerifier::IsVerifiableCallback callback) {
        saved_callback = std::move(callback);
      });

  // Ensure no popup is shown and no token is sent.
  EXPECT_CALL(client(), ShowEmailVerificationPopup).Times(0);
  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);

  TriggerDefaultFormFill(*form);

  ASSERT_TRUE(saved_callback);

  // Simulate the driver becoming inactive (e.g., page enters BFCache).
  test_api(driver()).SetLifecycleState(
      AutofillDriver::LifecycleState::kInactive);

  // Run the callback.
  std::move(saved_callback).Run(CreateVerifiableResult());

  histogram_tester.ExpectUniqueSample("Blink.Evp.Autofill.FlowResult",
                                      EvpAutofillFlowResult::kDriverInactive,
                                      1);
}

// Verifies that if the driver becomes inactive after the popup is shown but
// before the user makes a decision, the decision callback is dropped.
TEST_F(EmailVerifierDelegateTest, DriverInactiveBeforeDecision) {
  base::HistogramTester histogram_tester;
  FormStructure* form = SetUpValidForm();

  EXPECT_CALL(email_verifier(), CheckIfVerifiable("johndoe@hades.com", _))
      .WillOnce(RunOnceCallback<1>(CreateVerifiableResult()));

  // Capture the popup decision callback.
  base::OnceCallback<void(AutofillClient::EmailVerificationPermissionUiResult)>
      saved_decision_callback;
  EXPECT_CALL(client(), ShowEmailVerificationPopup)
      .WillOnce(
          [&](const gfx::RectF&, const net::SchemefulSite&,
              const std::u16string&,
              base::OnceCallback<void(
                  AutofillClient::EmailVerificationPermissionUiResult)>
                  callback) { saved_decision_callback = std::move(callback); });

  // Ensure verification is not triggered.
  EXPECT_CALL(email_verifier(), Verify).Times(0);

  TriggerDefaultFormFill(*form);

  ASSERT_TRUE(saved_decision_callback);

  // Simulate the driver becoming inactive.
  test_api(driver()).SetLifecycleState(
      AutofillDriver::LifecycleState::kInactive);

  // Run the decision callback.
  std::move(saved_decision_callback)
      .Run(AutofillClient::EmailVerificationPermissionUiResult::kAccepted);

  histogram_tester.ExpectUniqueSample("Blink.Evp.Autofill.FlowResult",
                                      EvpAutofillFlowResult::kDriverInactive,
                                      1);
}

// Verifies that if the driver becomes inactive after the decision is made but
// before the verification response is received, the response is dropped.
TEST_F(EmailVerifierDelegateTest, DriverInactiveBeforeResponse) {
  base::HistogramTester histogram_tester;
  FormStructure* form = SetUpValidForm();

  EXPECT_CALL(email_verifier(), CheckIfVerifiable("johndoe@hades.com", _))
      .WillOnce(RunOnceCallback<1>(CreateVerifiableResult()));

  EXPECT_CALL(client(), ShowEmailVerificationPopup)
      .WillOnce(RunOnceCallback<3>(
          AutofillClient::EmailVerificationPermissionUiResult::kAccepted));

  // Capture the verification response callback.
  EmailVerifier::OnEmailVerifiedCallback saved_response_callback;
  EXPECT_CALL(email_verifier(), Verify(_, "test_nonce", _))
      .WillOnce([&](const EmailVerifier::Result&, const std::string&,
                    EmailVerifier::OnEmailVerifiedCallback callback) {
        saved_response_callback = std::move(callback);
      });

  // Ensure token is not sent to renderer.
  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);

  TriggerDefaultFormFill(*form);

  ASSERT_TRUE(saved_response_callback);

  // Simulate the driver becoming inactive.
  test_api(driver()).SetLifecycleState(
      AutofillDriver::LifecycleState::kInactive);

  // Run the response callback.
  std::move(saved_response_callback)
      .Run(std::optional<std::string>("test_token"));

  histogram_tester.ExpectUniqueSample("Blink.Evp.Autofill.FlowResult",
                                      EvpAutofillFlowResult::kDriverInactive,
                                      1);
}

// Verifies that if a page navigation completes while a verification request is
// in-flight, kPageNavigatedDuringVerification is recorded.
TEST_F(EmailVerifierDelegateTest, PageNavigatedDuringVerification) {
  base::HistogramTester histogram_tester;
  FormStructure* form = SetUpValidForm();

  EXPECT_CALL(email_verifier(), CheckIfVerifiable("johndoe@hades.com", _))
      .WillOnce(RunOnceCallback<1>(CreateVerifiableResult()));

  EXPECT_CALL(client(), ShowEmailVerificationPopup)
      .WillOnce(RunOnceCallback<3>(
          AutofillClient::EmailVerificationPermissionUiResult::kAccepted));

  // Capture the verification response callback and keep it in-flight.
  EmailVerifier::OnEmailVerifiedCallback saved_response_callback;
  EXPECT_CALL(email_verifier(), Verify(_, "test_nonce", _))
      .WillOnce([&](const EmailVerifier::Result&, const std::string&,
                    EmailVerifier::OnEmailVerifiedCallback callback) {
        saved_response_callback = std::move(callback);
      });

  TriggerDefaultFormFill(*form);

  ASSERT_TRUE(saved_response_callback);

  // Simulate primary main frame navigation committing while verification is
  // in-flight.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://other-example.com"));

  histogram_tester.ExpectUniqueSample(
      "Blink.Evp.Autofill.FlowResult",
      EvpAutofillFlowResult::kPageNavigatedDuringVerification, 1);

  // If the network response returns after navigation, running the callback
  // should be a no-op because in_flight_verify_count_ was reset.
  std::move(saved_response_callback)
      .Run(std::optional<std::string>("test_token"));

  // Verify no additional metric was logged.
  histogram_tester.ExpectTotalCount("Blink.Evp.Autofill.FlowResult", 1);
}

// Verifies that focus loss on an email field only triggers verification if the
// last change to the field was a manual user edit (not autofill or JS).
TEST_F(EmailVerifierDelegateTest, OnFieldLostFocus_OnlyTriggersOnUserEdit) {
  FormStructure* form = SetUpValidForm();

  // Expect NO verification.
  EXPECT_CALL(email_verifier(), CheckIfVerifiable).Times(0);
  EXPECT_CALL(email_verifier(), Verify).Times(0);
  EXPECT_CALL(driver(), SendEmailVerificationToken).Times(0);

  // Simulate non-user edit (e.g. autofill).
  form->field(0)->set_value(u"user@example.com");
  form->field(0)->AddFieldModifier(FieldModifier::kAutofill);

  // Focus it.
  delegate().OnAfterFocusOnFormField(manager(), form->global_id(),
                                     form->field(0)->global_id());
  // Focus away.
  delegate().OnAfterFocusOnNonFormField(manager());
}

// Verifies that the delegate maintains an LRU cache of recently verified email
// values to deduplicate verification triggers for the tab, preventing duplicate
// prompts on alternating focus, and correctly evicting the oldest entry.
TEST_F(EmailVerifierDelegateTest,
       OnFieldLostFocus_DeduplicatesAlternatingFields) {
  FormData form_data = test::GetFormData(
      {.description_for_logging = "FormWith6Emails",
       .fields =
           {
               {.label = u"Email1",
                .name = u"email1",
                .nonce = u"test_nonce",
                .form_control_type = FormControlType::kInputEmail},
               {.label = u"Email2",
                .name = u"email2",
                .nonce = u"test_nonce",
                .form_control_type = FormControlType::kInputEmail},
               {.label = u"Email3",
                .name = u"email3",
                .nonce = u"test_nonce",
                .form_control_type = FormControlType::kInputEmail},
               {.label = u"Email4",
                .name = u"email4",
                .nonce = u"test_nonce",
                .form_control_type = FormControlType::kInputEmail},
               {.label = u"Email5",
                .name = u"email5",
                .nonce = u"test_nonce",
                .form_control_type = FormControlType::kInputEmail},
               {.label = u"Email6",
                .name = u"email6",
                .nonce = u"test_nonce",
                .form_control_type = FormControlType::kInputEmail},
               {.label = u"Verification Token",
                .name = u"verification_token",
                .nonce = u"test_nonce",
                .autocomplete_attribute = "email-verification-token",
                .form_control_type =
                    FormControlType::kInputHiddenEmailVerification},
           },
       .host_frame = driver().GetFrameToken()});
  manager().AddSeenForm(
      form_data, {EMAIL_ADDRESS, EMAIL_ADDRESS, EMAIL_ADDRESS, EMAIL_ADDRESS,
                  EMAIL_ADDRESS, EMAIL_ADDRESS, UNKNOWN_TYPE});
  FormStructure* form =
      test_api(manager()).FindCachedFormById(form_data.global_id());
  ASSERT_TRUE(form);
  for (int i = 0; i < 6; ++i) {
    form->field(i)->set_autofilled_type(EMAIL_ADDRESS);
  }

  // 1. Fill 5 fields with user edits.
  std::vector<std::string> emails;
  for (int i = 0; i < 5; ++i) {
    std::string email = "user" + base::NumberToString(i + 1) + "@example.com";
    emails.push_back(email);
    form->field(i)->set_value(base::UTF8ToUTF16(email));
    form->field(i)->AddFieldModifier(FieldModifier::kUser);
  }

  testing::Sequence s;
  testing::MockFunction<void(int)> checkpoint;

  // Set up expectations in sequence
  // Part 1: Expect 5 sequential triggers
  for (int i = 0; i < 5; ++i) {
    EXPECT_CALL(email_verifier(), CheckIfVerifiable(emails[i], _))
        .InSequence(s);
  }

  EXPECT_CALL(checkpoint, Call(1)).InSequence(s);

  // Part 2: Alternating focus should NOT trigger anything.
  // We expect Checkpoint 2 to happen immediately after Checkpoint 1 in the
  // sequence, meaning no CheckIfVerifiable calls can happen in between.
  EXPECT_CALL(checkpoint, Call(2)).InSequence(s);

  // Part 3: 6th field trigger (evicts 1)
  std::string email6 = "user6@example.com";
  EXPECT_CALL(email_verifier(), CheckIfVerifiable(email6, _)).InSequence(s);

  EXPECT_CALL(checkpoint, Call(3)).InSequence(s);

  // Part 4: Blur email1 again (triggers because evicted)
  std::string email1 = "user1@example.com";
  EXPECT_CALL(email_verifier(), CheckIfVerifiable(email1, _)).InSequence(s);

  EXPECT_CALL(checkpoint, Call(4)).InSequence(s);

  // --- Execution ---

  // 1. Trigger the 5 sequential focus losses.
  // Focus 0. (No trigger yet)
  delegate().OnAfterFocusOnFormField(manager(), form->global_id(),
                                     form->field(0)->global_id());

  // Focus 1. (Triggers 0)
  delegate().OnAfterFocusOnFormField(manager(), form->global_id(),
                                     form->field(1)->global_id());

  // Focus 2. (Triggers 1)
  delegate().OnAfterFocusOnFormField(manager(), form->global_id(),
                                     form->field(2)->global_id());

  // Focus 3. (Triggers 2)
  delegate().OnAfterFocusOnFormField(manager(), form->global_id(),
                                     form->field(3)->global_id());

  // Focus 4. (Triggers 3)
  delegate().OnAfterFocusOnFormField(manager(), form->global_id(),
                                     form->field(4)->global_id());

  // Focus non-form. (Triggers 4)
  delegate().OnAfterFocusOnNonFormField(manager());

  // Verify Part 1 completed
  checkpoint.Call(1);

  // 2. Alternating focus between the 5 fields without changes -> Should NOT
  // trigger.
  delegate().OnAfterFocusOnFormField(manager(), form->global_id(),
                                     form->field(0)->global_id());  // Focus 0
  delegate().OnAfterFocusOnFormField(
      manager(), form->global_id(),
      form->field(2)->global_id());  // Focus 2 (triggers 0)
  delegate().OnAfterFocusOnFormField(
      manager(), form->global_id(),
      form->field(4)->global_id());  // Focus 4 (triggers 2)
  delegate().OnAfterFocusOnFormField(
      manager(), form->global_id(),
      form->field(1)->global_id());  // Focus 1 (triggers 4)
  delegate().OnAfterFocusOnFormField(
      manager(), form->global_id(),
      form->field(3)->global_id());  // Focus 3 (triggers 1)
  delegate().OnAfterFocusOnNonFormField(
      manager());  // Focus non-form (triggers 3)

  // Verify Part 2 completed (no triggers happened)
  checkpoint.Call(2);

  // 3. Fill and blur 6th field -> Should trigger (evicts 1).
  form->field(5)->set_value(base::UTF8ToUTF16(email6));
  form->field(5)->AddFieldModifier(FieldModifier::kUser);

  delegate().OnAfterFocusOnFormField(manager(), form_data.global_id(),
                                     form->field(5)->global_id());  // Focus 5
  delegate().OnAfterFocusOnNonFormField(
      manager());  // Focus non-form (triggers 5)

  // Verify Part 3 completed
  checkpoint.Call(3);

  // 4. Blur email1 again without changes -> Should trigger again because it was
  // evicted.
  delegate().OnAfterFocusOnFormField(manager(), form_data.global_id(),
                                     form->field(0)->global_id());  // Focus 0
  delegate().OnAfterFocusOnNonFormField(
      manager());  // Focus non-form (triggers 0)

  // Verify Part 4 completed
  checkpoint.Call(4);
}

// Verifies that when email verification is triggered on a form fill, the
// delegate immediately notifies the driver to show a loading state on the email
// field while verification check is pending.
TEST_F(EmailVerifierDelegateTest, UpdateEmailVerificationStateLoading) {
  FormStructure* form = SetUpValidForm();

  SetUpVerificationExpectations(*form);

  EXPECT_CALL(driver(), UpdateEmailVerificationState(
                            form->field(0)->global_id(),
                            mojom::EmailVerificationState::kLoading));

  TriggerDefaultFormFill(*form);

  popup_shown_run_loop_.Run();
}

}  // namespace autofill
