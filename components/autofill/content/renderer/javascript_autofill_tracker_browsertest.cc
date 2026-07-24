// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/javascript_autofill_tracker.h"

#include <memory>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/javascript_autofill_tracker_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"

namespace autofill {

namespace {

class JavaScriptAutofillTrackerTest : public test::AutofillRendererTest {
 public:
  JavaScriptAutofillTrackerTest() = default;

  void ActivateFocusAndClick(const char* element_id) {
    GetMainFrame()->NotifyUserActivation(
        blink::mojom::UserActivationNotificationType::kInteraction);
    Focus(element_id);
    SimulateElementClickAndWait(element_id);
  }
};

// Test that the log is updated when JavaScript sets a value.
TEST_F(JavaScriptAutofillTrackerTest, JavaScriptChangedValueLogging) {
  LoadHTML(R"(
      <form id="form_id">
        <input id="text_1">
        <input type="button" id="button_id">
      </form>)");

  blink::WebFormControlElement text1 =
      GetWebElementById("text_1").DynamicTo<blink::WebFormControlElement>();
  blink::WebFormControlElement button_element =
      GetWebElementById("button_id").DynamicTo<blink::WebFormControlElement>();

  // Helper to trigger JS set value.
  auto js_set_value = [this](const char* id, const char* value) {
    ExecuteJavaScriptForTests(base::StringPrintf(
        R"(document.getElementById('%s').value = '%s';)", id, value));
  };

  const std::vector<mojom::JavaScriptFieldModificationPtr>& logs =
      test_api(test_api(autofill_agent()).javascript_autofill_tracker())
          .js_logs();

  // 1. JS change without user activation -> should not log.
  js_set_value("text_1", "js_val_1");
  EXPECT_TRUE(logs.empty());

  // 2. JS change with user activation and mousedown -> should log.
  ActivateFocusAndClick("text_1");
  js_set_value("text_1", "js_val_2");

  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0]->field_id, form_util::GetFieldRendererId(text1));
  EXPECT_EQ(logs[0]->modification_type,
            mojom::JavaScriptModificationType::kReassignment);

  // Clear logs by waiting for timer.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  ASSERT_TRUE(logs.empty());

  // 3. JS change with user activation but NO focused element -> should not log.
  ExecuteJavaScriptForTests(R"(document.activeElement.blur();)");
  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction);
  js_set_value("text_1", "js_val_3");
  EXPECT_TRUE(logs.empty());

  // 4. JS change with user activation and focused element, but modified
  // element is NOT autofillable (button) -> should not log.
  ActivateFocusAndClick("text_1");
  js_set_value("button_id", "js_val_button");
  EXPECT_TRUE(logs.empty());

  // 5. JS change to the SAME value with user activation -> should log.
  ActivateFocusAndClick("text_1");
  js_set_value("text_1", "js_val_3");  // Same value (set in step 3).

  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0]->field_id, form_util::GetFieldRendererId(text1));
  EXPECT_EQ(logs[0]->modification_type,
            mojom::JavaScriptModificationType::kTrivial);

  // 6. JS change to a prefix completion -> should log kPrefixCompletion.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  ActivateFocusAndClick("text_1");
  js_set_value("text_1", "js_val_3_more");
  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0]->modification_type,
            mojom::JavaScriptModificationType::kPrefixCompletion);

  // 7. JS change from non-empty to empty -> should log kClearing.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  ActivateFocusAndClick("text_1");
  js_set_value("text_1", "");
  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0]->modification_type,
            mojom::JavaScriptModificationType::kClearing);

  // 8. JS change from empty to non-empty -> should log kEmptyToNonEmpty.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  ActivateFocusAndClick("text_1");
  js_set_value("text_1", "new_val");
  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0]->modification_type,
            mojom::JavaScriptModificationType::kEmptyToNonEmpty);
}

// Test that if JavaScript modifies fields in a different form than the
// currently focused field, the modifications are ignored when the timer fires.
TEST_F(JavaScriptAutofillTrackerTest, IgnoreCrossFormModifications) {
  LoadHTML(R"(
      <form id="form_1">
        <input id="text_1_1">
      </form>
      <form id="form_2">
        <input id="text_2_1">
        <input id="text_2_2">
        <input id="text_2_3">
      </form>)");

  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(0);

  // Helper to trigger JS set value.
  auto js_set_value = [this](const char* id, const char* value) {
    ExecuteJavaScriptForTests(base::StringPrintf(
        R"(document.getElementById('%s').value = '%s';)", id, value));
  };

  const std::vector<mojom::JavaScriptFieldModificationPtr>& logs =
      test_api(test_api(autofill_agent()).javascript_autofill_tracker())
          .js_logs();

  // Focus and click a field in form_1.
  ActivateFocusAndClick("text_1_1");

  // Modify 3 fields in form_2.
  js_set_value("text_2_1", "val_1");
  js_set_value("text_2_2", "val_2");
  js_set_value("text_2_3", "val_3");

  ASSERT_EQ(logs.size(), 3u);

  // Fast forward time so the timer fires. Because the focused field belongs to
  // form_1 while the modified fields belong to form_2,
  // DidDetectJavaScriptAutofill() should not be called and logs should be
  // cleared.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_TRUE(logs.empty());
  testing::Mock::VerifyAndClearExpectations(&autofill_driver());

  // Focus and click a field in form_2 and modify the same fields in form_2.
  // Now that the focused field belongs to the same form as the modified fields,
  // DidDetectJavaScriptAutofill() should be called.
  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(1);

  ActivateFocusAndClick("text_2_1");
  js_set_value("text_2_1", "val_1_new");
  js_set_value("text_2_2", "val_2_new");
  js_set_value("text_2_3", "val_3_new");

  ASSERT_EQ(logs.size(), 3u);

  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_TRUE(logs.empty());
}

// Test that if fewer than 3 fields are modified by JavaScript, detection is
// still triggered if at least one field underwent prefix completion
// (kPrefixCompletion).
TEST_F(JavaScriptAutofillTrackerTest,
       DetectFewerThanMinFieldsWithPrefixCompletion) {
  LoadHTML(R"(
      <form id="form_id">
        <input id="text_1" value="app">
        <input id="text_2">
      </form>)");

  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(1);

  auto js_set_value = [this](const char* id, const char* value) {
    ExecuteJavaScriptForTests(base::StringPrintf(
        R"(document.getElementById('%s').value = '%s';)", id, value));
  };

  ActivateFocusAndClick("text_1");

  // JS extends "app" to "apple" (kPrefixCompletion).
  js_set_value("text_1", "apple");

  // Even though only 1 field changed (< 3), kPrefixCompletion triggers
  // detection.
  task_environment_.FastForwardBy(base::Milliseconds(200));
}

// Test that if fewer than 3 fields are modified by JavaScript and NONE of them
// underwent prefix completion, detection is NOT triggered.
TEST_F(JavaScriptAutofillTrackerTest,
       IgnoreFewerThanMinFieldsWithoutPrefixCompletion) {
  LoadHTML(R"(
      <form id="form_id">
        <input id="text_1">
        <input id="text_2">
      </form>)");

  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(0);

  auto js_set_value = [this](const char* id, const char* value) {
    ExecuteJavaScriptForTests(base::StringPrintf(
        R"(document.getElementById('%s').value = '%s';)", id, value));
  };

  ActivateFocusAndClick("text_1");

  // JS sets text_1 to "apple" from empty (kEmptyToNonEmpty) and text_2 to
  // "banana" (kEmptyToNonEmpty).
  js_set_value("text_1", "apple");
  js_set_value("text_2", "banana");

  // 2 fields changed (< 3), but neither is kPrefixCompletion -> should NOT
  // trigger detection.
  task_environment_.FastForwardBy(base::Milliseconds(200));
}

}  // namespace

}  // namespace autofill
