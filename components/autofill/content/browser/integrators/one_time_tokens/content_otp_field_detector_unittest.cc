// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/integrators/one_time_tokens/content_otp_field_detector.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class ContentOtpFieldDetectorTest : public content::RenderViewHostTestHarness {
 public:
  ContentOtpFieldDetectorTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ContentOtpFieldDetectorTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    content::NavigationSimulator::CreateBrowserInitiated(GURL("https://a.com/"),
                                                         web_contents())
        ->Commit();
  }

  void SimulateNavigation() { NavigateAndCommit(GURL("https://a.com/")); }

  FormData CreateSimpleOtp(bool is_focusable = true) {
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
    LocalFrameToken frame_token(rfh->GetFrameToken().value());
    FormData form;
    form.set_url(GURL("https://www.foo.com"));
    form.set_renderer_id(autofill::test::MakeFormRendererId());
    FormFieldData field = {autofill::test::CreateTestFormField(
        "some_label", "some_name", "some_value", FormControlType::kInputText)};
    field.set_is_focusable(is_focusable);

    form.set_fields({field});
    return autofill::test::CreateFormDataForFrame(form, frame_token);
  }

  void AddOtpToThePage(FormData form) {
    GetAutofillManager()->AddSeenForm(
        form,
        /*field_types=*/std::vector<FieldType>{ONE_TIME_CODE});

    // Notify observers manually as this would typically happen during parsing
    // but the step is skipped when using the Test APIs.
    GetAutofillManager()->NotifyObservers(
        &TestBrowserAutofillManager::Observer::OnFieldTypesDetermined,
        form.global_id(),
        TestBrowserAutofillManager::Observer::FieldTypeSource::
            kHeuristicsOrAutocomplete);
  }

  void RemoveOtpFromThePage(FormData form) {
    GetAutofillManager()->OnFormsSeen(
        /*updated_forms=*/{},
        /*removed_forms=*/{form.global_id()});
  }

  void SimulateSubmission(FormData form) {
    GetAutofillManager()->OnFormSubmitted(
        form, mojom::SubmissionSource::XHR_SUCCEEDED);
  }

  TestBrowserAutofillManager* GetAutofillManager() {
    return autofill_manager_injector_[web_contents()->GetPrimaryMainFrame()];
  }

  TestContentAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillManagerInjector<TestBrowserAutofillManager>
      autofill_manager_injector_;
};

// Verify that IsOtpFieldPresent works as expected.
TEST_F(ContentOtpFieldDetectorTest, IsOtpFieldPresent) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(autofill_client()->GetOtpFieldDetector()->IsOtpFieldPresent());
  histogram_tester.ExpectUniqueSample("PasswordManager.OtpPresentInMainTab",
                                      false, 1);

  FormData form = CreateSimpleOtp();
  AddOtpToThePage(form);

  EXPECT_TRUE(autofill_client()->GetOtpFieldDetector()->IsOtpFieldPresent());
  histogram_tester.ExpectBucketCount("PasswordManager.OtpPresentInMainTab",
                                     true, 1);
  histogram_tester.ExpectTotalCount("PasswordManager.OtpPresentInMainTab", 2);

  RemoveOtpFromThePage(form);

  EXPECT_FALSE(autofill_client()->GetOtpFieldDetector()->IsOtpFieldPresent());
  histogram_tester.ExpectBucketCount("PasswordManager.OtpPresentInMainTab",
                                     false, 2);
  histogram_tester.ExpectTotalCount("PasswordManager.OtpPresentInMainTab", 3);
}

// Verify that the OtpFieldsDetectedCallback is triggered when an OTP form is
// detected.
TEST_F(ContentOtpFieldDetectorTest, DiscoverOTPs) {
  base::MockRepeatingCallback<void()> otp_detected_callback;
  OtpFieldDetector* otp_field_detector =
      autofill_client()->GetOtpFieldDetector();
  base::CallbackListSubscription subscription =
      otp_field_detector->RegisterOtpFieldsDetectedCallback(
          otp_detected_callback.Get());

  EXPECT_CALL(otp_detected_callback, Run()).Times(1);
  AddOtpToThePage(CreateSimpleOtp());
  ASSERT_TRUE(
      testing::Mock::VerifyAndClearExpectations(&otp_detected_callback));

  // The second addition of OTPs should not generate more callbacks
  EXPECT_CALL(otp_detected_callback, Run()).Times(0);
  AddOtpToThePage(CreateSimpleOtp());
}

// Verify that an OTP form that is parsed but has non-focusable fields does
// not trigger the OTP detected callback.
TEST_F(ContentOtpFieldDetectorTest, DiscoverOTPs_IgnoreNonfocusableOtpFields) {
  base::MockRepeatingCallback<void()> otp_detected_callback;
  OtpFieldDetector* otp_field_detector =
      autofill_client()->GetOtpFieldDetector();
  base::CallbackListSubscription subscription =
      otp_field_detector->RegisterOtpFieldsDetectedCallback(
          otp_detected_callback.Get());

  EXPECT_CALL(otp_detected_callback, Run()).Times(0);
  AddOtpToThePage(CreateSimpleOtp(/*is_focusable=*/false));
}

// Verify that a navigation which drops all forms is recognized.
TEST_F(ContentOtpFieldDetectorTest, CallbackInvokedAfterNavigationClearsOtps) {
  AddOtpToThePage(CreateSimpleOtp());

  base::MockRepeatingCallback<void()> otp_fields_submitted_callback;
  EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);

  OtpFieldDetector* otp_field_detector =
      autofill_client()->GetOtpFieldDetector();
  base::CallbackListSubscription subscription =
      otp_field_detector->RegisterOtpFieldsSubmittedCallback(
          otp_fields_submitted_callback.Get());

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(
      &otp_fields_submitted_callback));

  // Verify that a navigation triggers a callback.
  EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(1);
  SimulateNavigation();
}

// Verify that removing an OTP form from the DOM is detected.
TEST_F(ContentOtpFieldDetectorTest, CallbackInvokedFromFormRemoval) {
  FormData form = CreateSimpleOtp();
  AddOtpToThePage(form);

  base::MockRepeatingCallback<void()> otp_fields_submitted_callback;
  EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);

  OtpFieldDetector* otp_field_detector =
      autofill_client()->GetOtpFieldDetector();
  base::CallbackListSubscription subscription =
      otp_field_detector->RegisterOtpFieldsSubmittedCallback(
          otp_fields_submitted_callback.Get());

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(
      &otp_fields_submitted_callback));

  // Now, make the field disappear and simulate another navigation.
  EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(1);
  RemoveOtpFromThePage(form);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(
      &otp_fields_submitted_callback));

  // A following navigation should not trigger another callback.
  EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);
  SimulateNavigation();
}

// Verify that submitting an OTP form is detected.
TEST_F(ContentOtpFieldDetectorTest, CallbackInvokedFromFormSubmission) {
  FormData form = CreateSimpleOtp();
  AddOtpToThePage(form);

  base::MockRepeatingCallback<void()> otp_fields_submitted_callback;
  EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);

  OtpFieldDetector* otp_field_detector =
      autofill_client()->GetOtpFieldDetector();
  base::CallbackListSubscription subscription =
      otp_field_detector->RegisterOtpFieldsSubmittedCallback(
          otp_fields_submitted_callback.Get());

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(
      &otp_fields_submitted_callback));

  // Now, make the field disappear and simulate another navigation.
  EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(1);
  SimulateSubmission(form);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(
      &otp_fields_submitted_callback));

  // A following navigation should not trigger another callback because the site
  // had a single form that was considered removed at submission time (even
  // though it stayed in the DOM).
  EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);
  SimulateNavigation();
}

}  // namespace autofill
