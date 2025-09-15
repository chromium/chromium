// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using autofill::test::MakeFormGlobalId;

namespace {

class TestOtpFieldDetector : public OtpFieldDetector {
 public:
  using OtpFieldDetector::AddFormAndNotifyIfNecessary;
  using OtpFieldDetector::OtpFieldDetector;
  using OtpFieldDetector::RemoveFormAndNotifyIfNecessary;
};

}  // namespace

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
  histogram_tester.ExpectUniqueSample("PasswordManager.OtpPresentInMainTab",
                                      false, 1);

  detector.AddFormAndNotifyIfNecessary(form);
  // Now 1 OTP field is present.

  EXPECT_TRUE(detector.IsOtpFieldPresent());
  histogram_tester.ExpectBucketCount("PasswordManager.OtpPresentInMainTab",
                                     true, 1);
  histogram_tester.ExpectTotalCount("PasswordManager.OtpPresentInMainTab", 2);

  detector.RemoveFormAndNotifyIfNecessary(form);
  // Now 0 OTP fields are present.

  EXPECT_FALSE(detector.IsOtpFieldPresent());
  histogram_tester.ExpectBucketCount("PasswordManager.OtpPresentInMainTab",
                                     false, 2);
  histogram_tester.ExpectTotalCount("PasswordManager.OtpPresentInMainTab", 3);
}

}  // namespace autofill
