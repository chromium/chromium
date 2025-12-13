// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using autofill::test::MakeFormGlobalId;
using base::Bucket;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace {

constexpr char kOtpPresentInMainTabHistogram[] =
    "PasswordManager.OtpPresentInMainTab";

class TestOtpFieldDetector : public OtpFieldDetector {
 public:
  // Passing no autofill client disables subscription to
  // AutofillManager::Observer events.
  TestOtpFieldDetector() : OtpFieldDetector(nullptr) {}
  ~TestOtpFieldDetector() override = default;
  using OtpFieldDetector::AddFormAndNotifyIfNecessary;
  using OtpFieldDetector::OtpFieldDetector;
  using OtpFieldDetector::RemoveFormAndNotifyIfNecessary;
};

}  // namespace

// These are tests for the basic callback mechanisms, mocking the integration
// AutofillManager::Observer aspects.
class OtpFieldDetectorTest : public testing::Test {
 public:
  OtpFieldDetectorTest() = default;

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Verifies that the OtpFieldsDetectedCallback is called during the transition
// from 0 OTP forms to >0 OTP forms.
TEST_F(OtpFieldDetectorTest, TestCallbacks) {
  int detected_call_counter = 0;
  auto detected_callback = base::BindLambdaForTesting(
      [&detected_call_counter]() { detected_call_counter++; });
  int submitted_call_counter = 0;
  auto submitted_callback = base::BindLambdaForTesting(
      [&submitted_call_counter]() { submitted_call_counter++; });

  TestOtpFieldDetector detector;
  base::CallbackListSubscription detected_subscription =
      detector.RegisterOtpFieldsDetectedCallback(detected_callback);
  base::CallbackListSubscription submitted_subscription =
      detector.RegisterOtpFieldsSubmittedCallback(submitted_callback);

  FormGlobalId form1 = MakeFormGlobalId();
  FormGlobalId form2 = MakeFormGlobalId();

  EXPECT_EQ(detected_call_counter, 0);
  EXPECT_EQ(submitted_call_counter, 0);

  // [0 -> 1 OTP forms]: The first time an OTP form generates a callback.
  detector.AddFormAndNotifyIfNecessary(form1);
  EXPECT_EQ(detected_call_counter, 1);
  EXPECT_EQ(submitted_call_counter, 0);

  // [1 -> 1 OTP forms]: If the form is seen a second time, that does not
  // trigger a callback.
  detector.AddFormAndNotifyIfNecessary(form1);
  EXPECT_EQ(detected_call_counter, 1);
  EXPECT_EQ(submitted_call_counter, 0);

  // [1 -> 2 OTP forms]: If a another form with OTPs is observed, that does not
  // trigger a callback
  detector.AddFormAndNotifyIfNecessary(form2);
  EXPECT_EQ(detected_call_counter, 1);
  EXPECT_EQ(submitted_call_counter, 0);

  // [2 -> 1 OTP forms]: If only one of two forms is removed, that does not
  // trigger a submitted-callback.
  detector.RemoveFormAndNotifyIfNecessary(form1);
  EXPECT_EQ(detected_call_counter, 1);
  EXPECT_EQ(submitted_call_counter, 0);

  // [1 -> 0 OTP forms]: If the last OTP form is removed, that triggers a
  // a submitted-callback.
  detector.RemoveFormAndNotifyIfNecessary(form2);
  EXPECT_EQ(detected_call_counter, 1);
  EXPECT_EQ(submitted_call_counter, 1);

  // [0 -> 1 OTP forms]: Adding an OTP form back triggers a callback again.
  detector.AddFormAndNotifyIfNecessary(form2);
  EXPECT_EQ(detected_call_counter, 2);
  EXPECT_EQ(submitted_call_counter, 1);
}

// Verifies the the correct answers and UMA logging of IsOtpFieldPresent.
TEST_F(OtpFieldDetectorTest, IsOtpFieldPresent) {
  base::HistogramTester histogram_tester;
  TestOtpFieldDetector detector;
  FormGlobalId form = MakeFormGlobalId();

  // 0 OTP fields are present.

  EXPECT_FALSE(detector.IsOtpFieldPresent());
  histogram_tester.ExpectUniqueSample(kOtpPresentInMainTabHistogram, false, 1);

  detector.AddFormAndNotifyIfNecessary(form);
  // Now 1 OTP field is present.

  EXPECT_TRUE(detector.IsOtpFieldPresent());
  histogram_tester.ExpectBucketCount(kOtpPresentInMainTabHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kOtpPresentInMainTabHistogram, 2);

  detector.RemoveFormAndNotifyIfNecessary(form);
  // Now 0 OTP fields are present.

  EXPECT_FALSE(detector.IsOtpFieldPresent());
  histogram_tester.ExpectBucketCount(kOtpPresentInMainTabHistogram, false, 2);
  histogram_tester.ExpectTotalCount(kOtpPresentInMainTabHistogram, 3);
}

// Tests that the AutofillManager::Observer notifications work as expected.
class OtpFieldDetectorAutofillManagerObserverTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<> {
 public:
  OtpFieldDetectorAutofillManagerObserverTest() = default;
  ~OtpFieldDetectorAutofillManagerObserverTest() override = default;

  void SetUp() override {
    InitAutofillClient();
    CreateAutofillDriver();
    autofill_manager_observation_.Observe(&autofill_manager());
  }

  void TearDown() override {
    autofill_manager_observation_.Reset();
    DestroyAutofillClient();
  }

  void SimulateNavigation() {
    otp_field_detector_.OnAutofillManagerStateChanged(
        autofill_manager(), AutofillDriver::LifecycleState::kActive,
        AutofillDriver::LifecycleState::kPendingDeletion);
  }

  FormData CreateSimpleOtp(bool is_focusable = true) {
    FormData form;
    form.set_url(GURL("https://www.foo.com"));
    form.set_renderer_id(autofill::test::MakeFormRendererId());
    FormFieldData field = {autofill::test::CreateTestFormField(
        "some_label", "some_name", "some_value", FormControlType::kInputText)};
    field.set_is_focusable(is_focusable);
    form.set_fields({field});
    return form;
  }

  void AddOtpToThePage(const FormData& form) {
    autofill_manager().AddSeenForm(
        form,
        /*field_types=*/std::vector<FieldType>{ONE_TIME_CODE});

    // Notify observers manually as this would typically happen during parsing
    // but the step is skipped when using the Test APIs.
    autofill_manager().NotifyObservers(
        &TestBrowserAutofillManager::Observer::OnFieldTypesDetermined,
        form.global_id(),
        TestBrowserAutofillManager::Observer::FieldTypeSource::
            kHeuristicsOrAutocomplete);
  }

  void RemoveOtpFromThePage(FormData form) {
    autofill_manager().OnFormsSeen(
        /*updated_forms=*/{},
        /*removed_forms=*/{form.global_id()});
  }

  void SimulateSubmission(FormData form) {
    autofill_manager().OnFormSubmitted(form,
                                       mojom::SubmissionSource::XHR_SUCCEEDED);
  }

  OtpFieldDetector& otp_field_detector() { return otp_field_detector_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
  // Passing no autofill client disables subscription to
  // AutofillManager::Observer events.
  OtpFieldDetector otp_field_detector_{nullptr};
  // Instead of relying on the ScopedAutofillManagersObservation
  // in OtpFieldDetector, these tests use the following ScopedObservation.
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      autofill_manager_observation_{&otp_field_detector_};
};

// Verify that IsOtpFieldPresent works as expected.
TEST_F(OtpFieldDetectorAutofillManagerObserverTest, IsOtpFieldPresent) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(otp_field_detector().IsOtpFieldPresent());
  EXPECT_THAT(histogram_tester.GetAllSamples(kOtpPresentInMainTabHistogram),
              ElementsAre(Bucket(0, 1)));  // false == 0

  FormData form = CreateSimpleOtp();
  AddOtpToThePage(form);

  EXPECT_TRUE(otp_field_detector().IsOtpFieldPresent());
  EXPECT_THAT(histogram_tester.GetAllSamples(kOtpPresentInMainTabHistogram),
              ElementsAre(Bucket(0, 1), Bucket(1, 1)));  // true == 1

  RemoveOtpFromThePage(form);

  EXPECT_FALSE(otp_field_detector().IsOtpFieldPresent());
  EXPECT_THAT(histogram_tester.GetAllSamples(kOtpPresentInMainTabHistogram),
              ElementsAre(Bucket(0, 2), Bucket(1, 1)));
}

// Verify that the OtpFieldsDetectedCallback is triggered when an OTP form is
// detected.
TEST_F(OtpFieldDetectorAutofillManagerObserverTest, DiscoverOTPs) {
  base::MockRepeatingCallback<void()> otp_detected_callback;
  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsDetectedCallback(
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
TEST_F(OtpFieldDetectorAutofillManagerObserverTest,
       DiscoverOTPs_IgnoreNonfocusableOtpFields) {
  base::MockRepeatingCallback<void()> otp_detected_callback;
  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsDetectedCallback(
          otp_detected_callback.Get());

  EXPECT_CALL(otp_detected_callback, Run()).Times(0);
  AddOtpToThePage(CreateSimpleOtp(/*is_focusable=*/false));
}

// Verify that a navigation which drops all forms is recognized.
TEST_F(OtpFieldDetectorAutofillManagerObserverTest,
       CallbackInvokedAfterNavigationClearsOtps) {
  AddOtpToThePage(CreateSimpleOtp());

  base::MockRepeatingCallback<void()> otp_fields_submitted_callback;
  EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);

  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsSubmittedCallback(
          otp_fields_submitted_callback.Get());

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(
      &otp_fields_submitted_callback));

  // Verify that a navigation triggers a callback.
  EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(1);
  SimulateNavigation();
}

// Verify that removing an OTP form from the DOM is detected.
TEST_F(OtpFieldDetectorAutofillManagerObserverTest,
       CallbackInvokedFromFormRemoval) {
  FormData form = CreateSimpleOtp();
  AddOtpToThePage(form);

  base::MockRepeatingCallback<void()> otp_fields_submitted_callback;
  EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);

  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsSubmittedCallback(
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
TEST_F(OtpFieldDetectorAutofillManagerObserverTest,
       CallbackInvokedFromFormSubmission) {
  FormData form = CreateSimpleOtp();
  AddOtpToThePage(form);

  base::MockRepeatingCallback<void()> otp_fields_submitted_callback;
  EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);

  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsSubmittedCallback(
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
