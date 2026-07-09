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

  const std::vector<JavaScriptAutofillTracker::JsChangeRecord>& logs =
      test_api(test_api(autofill_agent()).javascript_autofill_tracker())
          .js_logs();

  // 1. JS change without user activation -> should not log.
  js_set_value("text_1", "js_val_1");
  EXPECT_TRUE(logs.empty());

  // 2. JS change with user activation -> should log.
  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction);
  Focus("text_1");
  js_set_value("text_1", "js_val_2");

  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0].modified_field_id, form_util::GetFieldRendererId(text1));
  EXPECT_EQ(logs[0].focused_field_id, form_util::GetFieldRendererId(text1));

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
  Focus("text_1");
  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction);
  js_set_value("button_id", "js_val_button");
  EXPECT_TRUE(logs.empty());

  // 5. JS change to the SAME value with user activation -> should log.
  Focus("text_1");
  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction);
  js_set_value("text_1", "js_val_3");  // Same value (set in step 3).

  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0].modified_field_id, form_util::GetFieldRendererId(text1));
}

// Test that JS changes triggered by browser autofill are ignored.
TEST_F(JavaScriptAutofillTrackerTest, BrowserAutofillIgnored) {
  LoadHTML(R"(
      <form id="form_id">
        <input id="text_1">
        <input id="text_2">
        <input id="text_3">
      </form>
      <script>
        document.getElementById('text_1').addEventListener('input', () => {
          document.getElementById('text_1').value = 'js_1';
          document.getElementById('text_2').value = 'js_2';
          document.getElementById('text_3').value = 'js_3';
        });
      </script>)");

  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(0);

  std::optional<FormData> form = ExtractFormData("form_id");
  ASSERT_TRUE(form);
  EXPECT_TRUE(SimulateFillForm(
      *form, "text_1",
      {{u"text_1", u"autofill_1"}, {u"text_2", u"autofill_2"}}));

  // Fast forward to let the clear timer fire.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  const std::vector<JavaScriptAutofillTracker::JsChangeRecord>& logs =
      test_api(test_api(autofill_agent()).javascript_autofill_tracker())
          .js_logs();
  EXPECT_TRUE(logs.empty());
}

// Test that consecutive browser autofills (like refills) extend the guard
// window, ensuring delayed JS changes are still ignored.
TEST_F(JavaScriptAutofillTrackerTest, AutofillAndRefillIgnored) {
  LoadHTML(R"(
      <form id="form_id">
        <input id="text_1">
        <input id="text_2">
        <input id="text_3">
        <input id="text_4">
      </form>
      <script>
        document.getElementById('text_1').addEventListener('input', () => {
          // Triggered by the initial autofill operation
          document.getElementById('text_1').value = 'js_1_1';
          document.getElementById('text_2').value = 'js_2_1';
          document.getElementById('text_3').value = 'js_3_1';
        });
        document.getElementById('text_4').addEventListener('input', () => {
          // Triggered by the refill operation
          document.getElementById('text_2').value = 'js_2_2';
          setTimeout(() => {
            document.getElementById('text_3').value = 'js_3_2';
          }, 60);
          setTimeout(() => {
            document.getElementById('text_4').value = 'js_4_2';
          }, 70);
        });
      </script>)");

  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(0);

  FormData form = ExtractFormData("form_id").value();

  const std::vector<JavaScriptAutofillTracker::JsChangeRecord>& logs =
      test_api(test_api(autofill_agent()).javascript_autofill_tracker())
          .js_logs();

  // 1. First Fill at t=0.
  EXPECT_TRUE(SimulateFillForm(form, "text_1", {{u"text_1", u"autofill_1"}}));
  task_environment_.FastForwardBy(base::Milliseconds(150));

  // 2. Refill at t=150ms.
  // This should restart the clear timer to expire at t=350ms.
  EXPECT_TRUE(SimulateFillForm(form, "text_4", {{u"text_4", u"autofill_4"}}));

  // Advance time by 50ms (to 200ms).
  // Logs should not have been cleared by now because the timer was restarted.
  task_environment_.FastForwardBy(base::Milliseconds(50));
  EXPECT_EQ(logs.size(), 4u);

  // Advance time by 50ms (to t=250ms).
  // During this time:
  // - t=210ms: text_3 is modified by JS.
  // - t=220ms: text_4 is modified by JS.
  task_environment_.FastForwardBy(base::Milliseconds(50));
  EXPECT_EQ(logs.size(), 6u);

  // Advance time by 100ms (to t=350ms).
  // All the logs should be cleared because of the timer firing at t=350ms.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(logs.empty());
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

  const std::vector<JavaScriptAutofillTracker::JsChangeRecord>& logs =
      test_api(test_api(autofill_agent()).javascript_autofill_tracker())
          .js_logs();

  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction);

  // Focus a field in form_1.
  Focus("text_1_1");

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

  // Focus a field in form_2 and modify the same fields in form_2.
  // Now that the focused field belongs to the same form as the modified fields,
  // DidDetectJavaScriptAutofill() should be called.
  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(1);

  Focus("text_2_1");
  js_set_value("text_2_1", "val_1_new");
  js_set_value("text_2_2", "val_2_new");
  js_set_value("text_2_3", "val_3_new");

  ASSERT_EQ(logs.size(), 3u);

  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_TRUE(logs.empty());
}

}  // namespace

}  // namespace autofill
