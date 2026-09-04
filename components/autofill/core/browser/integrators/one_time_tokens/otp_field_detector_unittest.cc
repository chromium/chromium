// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"

#include <optional>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill {

using ::autofill::test::MakeFormGlobalId;
using ::base::Bucket;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::MockFunction;

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
    : public testing::TestWithParam<bool>,
      public WithTestAutofillClientDriverManager<> {
 public:
  OtpFieldDetectorAutofillManagerObserverTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kAutofillRestrictOtpToSameTldPlusOne, GetParam());
  }
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

  FormData CreateSimpleOtp(
      bool is_focusable = true,
      const GURL& url = GURL("https://www.foo.com"),
      std::optional<url::Origin> main_frame_origin = std::nullopt,
      FormControlType form_control_type = FormControlType::kInputText) {
    FormData form;
    form.set_url(url);
    form.set_main_frame_origin(
        main_frame_origin.value_or(url::Origin::Create(url)));
    form.set_renderer_id(test::MakeFormRendererId());
    FormFieldData field = {test::CreateTestFormField(
        "some_label", "some_name", "some_value", form_control_type)};
    field.set_origin(url::Origin::Create(url));
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
            kHeuristicsOrAutocomplete,
        /*small_forms_were_parsed=*/false);
  }

  void RemoveOtpFromThePage(FormData form) {
    autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                   /*removed_forms=*/{form.global_id()},
                                   AutofillManagerTestApi::pass_key());
  }

  void SimulateSubmission(FormData form) {
    autofill_manager().OnFormSubmitted(form,
                                       mojom::SubmissionSource::XHR_SUCCEEDED,
                                       AutofillManagerTestApi::pass_key());
  }

  OtpFieldDetector& otp_field_detector() { return otp_field_detector_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_environment_;
  // Passing no autofill client disables subscription to
  // AutofillManager::Observer events.
  OtpFieldDetector otp_field_detector_{nullptr};
  // Instead of relying on the ScopedAutofillManagersObservation
  // in OtpFieldDetector, these tests use the following ScopedObservation.
  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      autofill_manager_observation_{&otp_field_detector_};
};

// Verify that IsOtpFieldPresent works as expected.
TEST_P(OtpFieldDetectorAutofillManagerObserverTest, IsOtpFieldPresent) {
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
TEST_P(OtpFieldDetectorAutofillManagerObserverTest, DiscoverOTPs) {
  base::MockRepeatingCallback<void()> otp_detected_callback;
  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(otp_detected_callback, Run()).Times(1);
    EXPECT_CALL(check, Call("otp_detected_callback called once"));
    EXPECT_CALL(otp_detected_callback, Run()).Times(0);
  }

  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsDetectedCallback(
          otp_detected_callback.Get());

  AddOtpToThePage(CreateSimpleOtp());
  check.Call("otp_detected_callback called once");
  // The second addition of OTPs should not generate more callbacks
  AddOtpToThePage(CreateSimpleOtp());
}

// Verify that an OTP form that is parsed but has non-focusable fields does
// not trigger the OTP detected callback.
TEST_P(OtpFieldDetectorAutofillManagerObserverTest,
       DiscoverOTPs_IgnoreNonfocusableOtpFields) {
  base::MockRepeatingCallback<void()> otp_detected_callback;
  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsDetectedCallback(
          otp_detected_callback.Get());

  EXPECT_CALL(otp_detected_callback, Run()).Times(0);
  AddOtpToThePage(CreateSimpleOtp(/*is_focusable=*/false));
}

// Verify that an OTP form containing only password fields does not trigger the
// OTP detected callback.
TEST_P(OtpFieldDetectorAutofillManagerObserverTest,
       DiscoverOTPs_IgnorePasswordFields) {
  base::MockRepeatingCallback<void()> otp_detected_callback;
  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsDetectedCallback(
          otp_detected_callback.Get());

  EXPECT_CALL(otp_detected_callback, Run()).Times(0);
  AddOtpToThePage(CreateSimpleOtp(
      /*is_focusable=*/true, GURL("https://www.foo.com"),
      /*main_frame_origin=*/std::nullopt,
      /*form_control_type=*/FormControlType::kInputPassword));
  EXPECT_FALSE(otp_field_detector().IsOtpFieldPresent());
}

// Verify that a navigation which drops all forms is recognized.
TEST_P(OtpFieldDetectorAutofillManagerObserverTest,
       CallbackInvokedAfterNavigationClearsOtps) {
  AddOtpToThePage(CreateSimpleOtp());

  base::MockRepeatingCallback<void()> otp_fields_submitted_callback;
  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);
    EXPECT_CALL(check, Call("no otp_fields_submitted_callback callbacks"));
    EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(1);
  }

  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsSubmittedCallback(
          otp_fields_submitted_callback.Get());
  check.Call("no otp_fields_submitted_callback callbacks");

  // Verify that a navigation triggers a callback.
  SimulateNavigation();
}

// Verify that removing an OTP form from the DOM is detected.
TEST_P(OtpFieldDetectorAutofillManagerObserverTest,
       CallbackInvokedFromFormRemoval) {
  FormData form = CreateSimpleOtp();
  AddOtpToThePage(form);

  base::MockRepeatingCallback<void()> otp_fields_submitted_callback;
  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);
    EXPECT_CALL(check, Call("1: no otp_fields_submitted_callback callbacks"));
    EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(1);
    EXPECT_CALL(check, Call("2: otp_fields_submitted_callback called once"));
    EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);
  }

  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsSubmittedCallback(
          otp_fields_submitted_callback.Get());

  check.Call("1: no otp_fields_submitted_callback callbacks");
  // Now, make the field disappear and simulate another navigation.
  RemoveOtpFromThePage(form);

  check.Call("2: otp_fields_submitted_callback called once");
  // A following navigation should not trigger another callback.
  SimulateNavigation();
}

// Verify that submitting an OTP form is detected.
TEST_P(OtpFieldDetectorAutofillManagerObserverTest,
       CallbackInvokedFromFormSubmission) {
  FormData form = CreateSimpleOtp();
  AddOtpToThePage(form);

  base::MockRepeatingCallback<void()> otp_fields_submitted_callback;
  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);
    EXPECT_CALL(check, Call("1: no otp_fields_submitted_callback callbacks"));
    EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(1);
    EXPECT_CALL(check, Call("2: otp_fields_submitted_callback called once"));
    EXPECT_CALL(otp_fields_submitted_callback, Run()).Times(0);
  }

  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsSubmittedCallback(
          otp_fields_submitted_callback.Get());

  check.Call("1: no otp_fields_submitted_callback callbacks");
  // Now, make the field disappear and simulate another navigation.
  SimulateSubmission(form);

  check.Call("2: otp_fields_submitted_callback called once");
  // A following navigation should not trigger another callback because the site
  // had a single form that was considered removed at submission time (even
  // though it stayed in the DOM).
  SimulateNavigation();
}

// Verify that OTP fields in the main frame trigger detection callbacks.
TEST_P(OtpFieldDetectorAutofillManagerObserverTest, AllowsMainFrame) {
  base::MockRepeatingCallback<void()> otp_detected_callback;
  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsDetectedCallback(
          otp_detected_callback.Get());

  EXPECT_CALL(otp_detected_callback, Run()).Times(1);
  AddOtpToThePage(CreateSimpleOtp());
}

// Verify that OTP fields in iframes with the same TLD+1 trigger detection
// callbacks.
TEST_P(OtpFieldDetectorAutofillManagerObserverTest,
       AllowsSameTldPlusOneIframe) {
  url::Origin top_frame_origin =
      url::Origin::Create(GURL("https://example.com"));

  base::MockRepeatingCallback<void()> otp_detected_callback;
  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsDetectedCallback(
          otp_detected_callback.Get());

  EXPECT_CALL(otp_detected_callback, Run()).Times(1);
  AddOtpToThePage(
      CreateSimpleOtp(true, GURL("https://sub.example.com"), top_frame_origin));
}

// Verify that OTP fields in cross-origin iframes with mismatched TLD+1 are
// ignored and do not trigger detection callbacks.
TEST_P(OtpFieldDetectorAutofillManagerObserverTest, IgnoreCrossOriginIframe) {
  // Set up mismatched TLD+1 origins.
  url::Origin top_frame_origin =
      url::Origin::Create(GURL("https://example.com"));

  base::MockRepeatingCallback<void()> otp_detected_callback;
  base::CallbackListSubscription subscription =
      otp_field_detector().RegisterOtpFieldsDetectedCallback(
          otp_detected_callback.Get());

  EXPECT_CALL(otp_detected_callback, Run()).Times(GetParam() ? 0 : 1);
  AddOtpToThePage(
      CreateSimpleOtp(true, GURL("https://attacker.com"), top_frame_origin));
}

INSTANTIATE_TEST_SUITE_P(All,
                         OtpFieldDetectorAutofillManagerObserverTest,
                         testing::Bool());

namespace {

// Helper to construct a FormStructure with a single field for testing
// `IsOtpForm`.
std::unique_ptr<FormStructure> CreateFormWithField(
    const url::Origin& main_frame_origin,
    const url::Origin& field_origin,
    FieldType type,
    bool is_focusable = true,
    FormControlType form_control_type = FormControlType::kInputText) {
  FormData form;
  form.set_main_frame_origin(main_frame_origin);
  FormFieldData field;
  field.set_origin(field_origin);
  field.set_is_focusable(is_focusable);
  field.set_form_control_type(form_control_type);
  form.set_fields({field});

  auto form_structure = std::make_unique<FormStructure>(form);
  if (form_structure->fields().empty()) {
    return nullptr;
  }
  form_structure->field(0)->SetTypeTo(AutofillType(type), std::nullopt);
  return form_structure;
}

}  // namespace

// Tests that `IsOtpForm` returns false when there are no OTP fields in the
// form.
TEST(OtpFieldDetectorIsOtpFormTest, NoOtpField) {
  std::unique_ptr<FormStructure> form_structure = CreateFormWithField(
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.com")),
      /*field_origin=*/url::Origin::Create(GURL("https://example.com")),
      /*type=*/UNKNOWN_TYPE);
  ASSERT_TRUE(form_structure);
  EXPECT_FALSE(OtpFieldDetector::IsOtpForm(*form_structure));
}

// Tests that `IsOtpForm` returns false when the only OTP field is not
// focusable.
TEST(OtpFieldDetectorIsOtpFormTest, UnfocusableOtpField) {
  std::unique_ptr<FormStructure> form_structure = CreateFormWithField(
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.com")),
      /*field_origin=*/url::Origin::Create(GURL("https://example.com")),
      /*type=*/ONE_TIME_CODE, /*is_focusable=*/false);
  ASSERT_TRUE(form_structure);
  EXPECT_FALSE(OtpFieldDetector::IsOtpForm(*form_structure));
}

// Tests that `IsOtpForm` returns true when there is a focusable OTP field on
// the same origin as the main frame.
TEST(OtpFieldDetectorIsOtpFormTest, FocusableOtpFieldSameOrigin) {
  std::unique_ptr<FormStructure> form_structure = CreateFormWithField(
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.com")),
      /*field_origin=*/url::Origin::Create(GURL("https://example.com")),
      /*type=*/ONE_TIME_CODE, /*is_focusable=*/true);
  ASSERT_TRUE(form_structure);
  EXPECT_TRUE(OtpFieldDetector::IsOtpForm(*form_structure));
}

// Tests that `IsOtpForm` returns true when an OTP field is in a subdomain
// iframe matching the main frame's TLD+1.
TEST(OtpFieldDetectorIsOtpFormTest, SameTldPlusOneSubdomain) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillRestrictOtpToSameTldPlusOne);

  std::unique_ptr<FormStructure> form_structure = CreateFormWithField(
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.com")),
      /*field_origin=*/url::Origin::Create(GURL("https://sub.example.com")),
      /*type=*/ONE_TIME_CODE);
  ASSERT_TRUE(form_structure);
  EXPECT_TRUE(OtpFieldDetector::IsOtpForm(*form_structure));
}

// Tests that `IsOtpForm` returns false when
// `kAutofillRestrictOtpToSameTldPlusOne` is enabled and an OTP field is in a
// cross-origin iframe with mismatched TLD+1.
TEST(OtpFieldDetectorIsOtpFormTest, MismatchedTldPlusOneFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillRestrictOtpToSameTldPlusOne);

  std::unique_ptr<FormStructure> form_structure = CreateFormWithField(
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.com")),
      /*field_origin=*/url::Origin::Create(GURL("https://attacker.com")),
      /*type=*/ONE_TIME_CODE);
  ASSERT_TRUE(form_structure);
  EXPECT_FALSE(OtpFieldDetector::IsOtpForm(*form_structure));
}

// Tests that `IsOtpForm` returns true when
// `kAutofillRestrictOtpToSameTldPlusOne` is disabled even if an OTP field is in
// a cross-origin iframe.
TEST(OtpFieldDetectorIsOtpFormTest, MismatchedTldPlusOneFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillRestrictOtpToSameTldPlusOne);

  std::unique_ptr<FormStructure> form_structure = CreateFormWithField(
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.com")),
      /*field_origin=*/url::Origin::Create(GURL("https://attacker.com")),
      /*type=*/ONE_TIME_CODE);
  ASSERT_TRUE(form_structure);
  EXPECT_TRUE(OtpFieldDetector::IsOtpForm(*form_structure));
}

// Tests that `IsOtpForm` returns false when multiple OTP fields are present and
// at least one has a mismatched TLD+1 with the main frame.
TEST(OtpFieldDetectorIsOtpFormTest, MultipleOtpFieldsOneMismatched) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillRestrictOtpToSameTldPlusOne);

  FormData form;
  form.set_main_frame_origin(url::Origin::Create(GURL("https://example.com")));
  FormFieldData field1;
  field1.set_origin(url::Origin::Create(GURL("https://example.com")));
  field1.set_is_focusable(true);
  FormFieldData field2;
  field2.set_origin(url::Origin::Create(GURL("https://attacker.com")));
  field2.set_is_focusable(true);
  form.set_fields({field1, field2});

  FormStructure form_structure(form);
  ASSERT_EQ(form_structure.fields().size(), 2u);
  form_structure.field(0)->SetTypeTo(AutofillType(ONE_TIME_CODE), std::nullopt);
  form_structure.field(1)->SetTypeTo(AutofillType(ONE_TIME_CODE), std::nullopt);

  EXPECT_FALSE(OtpFieldDetector::IsOtpForm(form_structure));
}

// Tests that `IsOtpForm` returns false when the only OTP field is a password
// field.
TEST(OtpFieldDetectorIsOtpFormTest, PasswordOtpField) {
  std::unique_ptr<FormStructure> form_structure = CreateFormWithField(
      /*main_frame_origin=*/url::Origin::Create(GURL("https://example.com")),
      /*field_origin=*/url::Origin::Create(GURL("https://example.com")),
      /*type=*/ONE_TIME_CODE, /*is_focusable=*/true,
      /*form_control_type=*/FormControlType::kInputPassword);
  ASSERT_TRUE(form_structure);
  EXPECT_FALSE(OtpFieldDetector::IsOtpForm(*form_structure));
}

// Tests that `IsOtpForm` returns true when there is both a password field
// and a text-based OTP field.
TEST(OtpFieldDetectorIsOtpFormTest, PasswordFieldAndTextOtpField) {
  FormData form;
  form.set_main_frame_origin(url::Origin::Create(GURL("https://example.com")));
  FormFieldData field1;
  field1.set_origin(url::Origin::Create(GURL("https://example.com")));
  field1.set_is_focusable(true);
  field1.set_form_control_type(FormControlType::kInputPassword);
  FormFieldData field2;
  field2.set_origin(url::Origin::Create(GURL("https://example.com")));
  field2.set_is_focusable(true);
  field2.set_form_control_type(FormControlType::kInputText);
  form.set_fields({field1, field2});

  FormStructure form_structure(form);
  ASSERT_EQ(form_structure.fields().size(), 2u);
  form_structure.field(0)->SetTypeTo(AutofillType(ONE_TIME_CODE), std::nullopt);
  form_structure.field(1)->SetTypeTo(AutofillType(ONE_TIME_CODE), std::nullopt);

  EXPECT_TRUE(OtpFieldDetector::IsOtpForm(form_structure));
}

}  // namespace autofill
