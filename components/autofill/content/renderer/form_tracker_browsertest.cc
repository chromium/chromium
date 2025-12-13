// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_tracker.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {
namespace {

using ::testing::_;

class MockFormTracker : public FormTracker {
 public:
  using FormTracker::FormTracker;
  MOCK_METHOD((void),
              FireFormSubmission,
              (mojom::SubmissionSource,
               std::optional<blink::WebFormElement>,
               bool),
              (override));
};

class FormTrackerTest : public test::AutofillRendererTest,
                        public testing::WithParamInterface<int> {
 public:
  FormTrackerTest() {
    EXPECT_LE(GetParam(), 3);
    std::vector<base::test::FeatureRef> features = {
        features::kAutofillFixFormTracking,
        features::kAutofillReplaceCachedWebElementsByRendererIds,
        features::kAutofillReplaceFormElementObserver};

    std::vector<base::test::FeatureRef> enabled_features(
        features.begin(), features.begin() + GetParam());
    std::vector<base::test::FeatureRef> disabled_features(
        features.begin() + GetParam(), features.end());
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUp() override {
    test::AutofillRendererTest::SetUp();
    auto tracker = std::make_unique<MockFormTracker>(
        GetMainRenderFrame(), autofill_agent(), password_autofill_agent());
    tracker->SetUserGestureRequired(FormTracker::UserGestureRequired(true));
    test_api(autofill_agent()).set_form_tracker(std::move(tracker));
  }

  MockFormTracker& form_tracker() {
    return static_cast<MockFormTracker&>(
        test_api(autofill_agent()).form_tracker());
  }

  blink::WebFormControlElement GetFormControlById(const std::string& id) {
    return GetMainFrame()
        ->GetDocument()
        .GetElementById(blink::WebString::FromUTF8(id))
        .DynamicTo<blink::WebFormControlElement>();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(AutofillSubmissionTest,
                         FormTrackerTest,
                         ::testing::Values(0, 1, 2, 3));

// Check that submission is detected on a page with no <form> when in sequence:
// 1) User types into a field.
// 2) Page does an XHR.
// 3) Page hides all of the inputs.
TEST_P(FormTrackerTest, FormlessXHRThenHide) {
  LoadHTML("<!DOCTYPE HTML><input id='input1'><input id='input2'/>");

  blink::WebFormControlElement input1 = GetFormControlById("input1");

  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);
  ExecuteJavaScriptForTests("document.getElementById('input1').focus();");
  form_tracker().TextFieldValueChanged(input1);

  task_environment_.RunUntilIdle();

  form_tracker().AjaxSucceeded();
  task_environment_.RunUntilIdle();
  // FormTracker should not think there is a submission because the <input>s are
  // still visible.

  // FormTracker should detect a submission after the <input>s are hidden.
  EXPECT_CALL(form_tracker(),
              FireFormSubmission(mojom::SubmissionSource::XHR_SUCCEEDED, _, _))
      .Times(1);
  ExecuteJavaScriptForTests(
      R"(document.getElementById('input1').style.display = 'none';
         document.getElementById('input2').style.display = 'none';)");
  ForceLayoutUpdate();
}

// Check that submission is detected on a page with no <form> when in sequence:
// 1) User types into a field.
// 2) Page hides all of the inputs.
// 3) Page does an XHR.
TEST_P(FormTrackerTest, FormlessHideThenXhr) {
  LoadHTML("<!DOCTYPE HTML><input id='input1'><input id='input2'/>");

  blink::WebFormControlElement input1 = GetFormControlById("input1");

  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);
  ExecuteJavaScriptForTests("document.getElementById('input1').focus();");
  form_tracker().TextFieldValueChanged(input1);
  task_environment_.RunUntilIdle();

  ExecuteJavaScriptForTests(
      "document.getElementById('input1').style.display = 'none';"
      "document.getElementById('input2').style.display = 'none';");
  ForceLayoutUpdate();
  task_environment_.RunUntilIdle();
  // FormTracker should not think there is a submission because the page has not
  // done any XHRs.

  // FormTracker should detect a submission when the XHR succeeds.
  EXPECT_CALL(form_tracker(),
              FireFormSubmission(mojom::SubmissionSource::XHR_SUCCEEDED, _, _))
      .Times(1);
  form_tracker().AjaxSucceeded();
  task_environment_.RunUntilIdle();
}

// Check that if a SelectControlSelectionChanged() is called asynchronously
// after a navigation, the event is a dead end.
TEST_P(FormTrackerTest, IgnoreSelectChangeInOldDocument) {
  LoadHTML(R"(<!DOCTYPE HTML>
    <select id=select>
    <option value=0>0</option>
    <option value=1>1</option>
    <option value=2>2</option>
    <option value=3>3</option>
    </select>)");
  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);

  EXPECT_CALL(autofill_driver(), SelectControlSelectionChanged);
  ExecuteJavaScriptForTests("document.getElementById('select').value = '1';");
  task_environment_.RunUntilIdle();

  EXPECT_CALL(autofill_driver(), SelectControlSelectionChanged);
  ExecuteJavaScriptForTests("document.getElementById('select').value = '2';");
  task_environment_.RunUntilIdle();

  EXPECT_CALL(autofill_driver(), SelectControlSelectionChanged).Times(0);
  ExecuteJavaScriptForTests("document.getElementById('select').value = '3';");
  LoadHTML(R"(<!DOCTYPE HTML><input>)");  // Turns the event into a no-op.
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace autofill
