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

  // Attaches a custom JS dropdown option to `trigger_id`. When clicked, the
  // option populates `field_values` (vector of target input ID -> new value)
  // via a JS mousedown listener.
  void AttachCustomDropdownOption(
      const std::string& trigger_id,
      const std::string& option_id,
      const std::vector<std::pair<std::string, std::string>>& field_values) {
    std::string fill_statements;
    for (const auto& [id, val] : field_values) {
      fill_statements += base::StringPrintf(
          R"({
            const elem = document.getElementById('%s');
            elem.value = '%s';
            elem.dispatchEvent(new Event('change', {bubbles: true}));
          })",
          id.c_str(), val.c_str());
    }

    constexpr std::string_view dropdown_body = R"(
      (() => {
        const trigger = document.getElementById('%s');
        const option = document.createElement('div');
        option.id = '%s';
        option.innerText = 'Option';
        option.addEventListener('mousedown', (e) => {
          e.preventDefault();
          %s
        });
        trigger.addEventListener('focus', () => {
          option.style.display = 'block';
        });
        trigger.addEventListener('blur', () => {
          option.style.display = 'none';
        });
        trigger.after(option);
      })();
    )";

    std::string js =
        base::StringPrintf(dropdown_body, trigger_id.c_str(), option_id.c_str(),
                           fill_statements.c_str());

    ExecuteJavaScriptForTests(js);
  }

  // Simulates user focus on `input_id` and clicking `option_id`.
  void SelectDropdownOption(const std::string& input_id,
                            const std::string& option_id) {
    Focus(input_id.c_str());
    SimulateElementClickAndWait(option_id.c_str());
  }

  // Loads HTML form with shipping and billing address sections and sets up the
  // "same_as_shipping" checkbox logic. The billing section starts hidden.
  // Unchecking the checkbox unhides the billing section (which stays visible
  // thereafter) and clears its fields. Changing a shipping field copies its
  // value to the corresponding billing field if the checkbox is checked.
  void LoadBillingAndShippingForm() {
    LoadHTML(R"(
        <form id="form_id">
          <!-- Shipping Section (visible by default) -->
          <div id="shipping_section">
            <input id="shipping_street">
            <input id="shipping_city">
            <input id="shipping_state">
            <input id="shipping_zip">
          </div>

          <!-- Checkbox: Same as shipping (checked by default) -->
          <input type="checkbox" id="same_as_shipping" checked>

          <!-- Billing Section (hidden by default) -->
          <div id="billing_section" style="display: none">
            <input id="billing_street">
            <input id="billing_city">
            <input id="billing_state">
            <input id="billing_zip">
          </div>
        </form>)");

    ExecuteJavaScriptForTests(R"(
      const checkbox = document.getElementById('same_as_shipping');
      const billingSection = document.getElementById('billing_section');

      const fieldPairs = [
        ['shipping_street', 'billing_street'],
        ['shipping_city', 'billing_city'],
        ['shipping_state', 'billing_state'],
        ['shipping_zip', 'billing_zip']
      ];

      checkbox.addEventListener('change', () => {
        if (!checkbox.checked) {
          billingSection.style.display = 'block';
          document.getElementById('billing_street').value = '';
          document.getElementById('billing_city').value = '';
          document.getElementById('billing_state').value = '';
          document.getElementById('billing_zip').value = '';
        } else {
          fieldPairs.forEach(([shippingId, billingId]) => {
            document.getElementById(billingId).value =
                document.getElementById(shippingId).value;
          });
        }
      });

      fieldPairs.forEach(([shippingId, billingId]) => {
        const shippingElem = document.getElementById(shippingId);
        const billingElem = document.getElementById(billingId);
        shippingElem.addEventListener('change', () => {
          if (checkbox.checked) {
            billingElem.value = shippingElem.value;
          }
        });
      });
    )");
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
  AttachCustomDropdownOption("text_1", "opt_1", {{"text_1", "js_val_2"}});
  SelectDropdownOption("text_1", "opt_1");

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
  AttachCustomDropdownOption("text_1", "opt_btn",
                             {{"button_id", "js_val_button"}});
  SelectDropdownOption("text_1", "opt_btn");
  EXPECT_TRUE(logs.empty());

  // 5. JS change to the SAME value with user activation -> should log.
  AttachCustomDropdownOption("text_1", "opt_same", {{"text_1", "js_val_3"}});
  SelectDropdownOption("text_1", "opt_same");

  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0]->field_id, form_util::GetFieldRendererId(text1));
  EXPECT_EQ(logs[0]->modification_type,
            mojom::JavaScriptModificationType::kTrivial);

  // 6. JS change to a prefix completion -> should log kPrefixCompletion.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  AttachCustomDropdownOption("text_1", "opt_prefix",
                             {{"text_1", "js_val_3_more"}});
  SelectDropdownOption("text_1", "opt_prefix");
  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0]->modification_type,
            mojom::JavaScriptModificationType::kPrefixCompletion);

  // 7. JS change from non-empty to empty -> should log kClearing.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  AttachCustomDropdownOption("text_1", "opt_clear", {{"text_1", ""}});
  SelectDropdownOption("text_1", "opt_clear");
  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0]->modification_type,
            mojom::JavaScriptModificationType::kClearing);

  // 8. JS change from empty to non-empty -> should log kEmptyToNonEmpty.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  AttachCustomDropdownOption("text_1", "opt_new", {{"text_1", "new_val"}});
  SelectDropdownOption("text_1", "opt_new");
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

  // Dropdown attached to text_1_1 in form_1 that modifies fields in form_2.
  AttachCustomDropdownOption(
      "text_1_1", "option_1_1",
      {{"text_2_1", "val_1"}, {"text_2_2", "val_2"}, {"text_2_3", "val_3"}});

  // Dropdown attached to text_2_1 in form_2 that modifies fields in form_2.
  AttachCustomDropdownOption(
      "text_2_1", "option_2_1",
      {{"text_2_1", "val_1"}, {"text_2_2", "val_2"}, {"text_2_3", "val_3"}});

  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(0);

  const std::vector<mojom::JavaScriptFieldModificationPtr>& logs =
      test_api(test_api(autofill_agent()).javascript_autofill_tracker())
          .js_logs();

  // Focus text_1_1 in form_1 and click option_1_1 (which modifies form_2) ->
  // ignored.
  SelectDropdownOption("text_1_1", "option_1_1");

  ASSERT_EQ(logs.size(), 3u);

  // Fast forward time so the timer fires. Because the focused field belongs to
  // form_1 while the modified fields belong to form_2,
  // DidDetectJavaScriptAutofill() should not be called and logs should be
  // cleared.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_TRUE(logs.empty());
  testing::Mock::VerifyAndClearExpectations(&autofill_driver());

  // Focus text_2_1 in form_2 and click option_2_1 (which modifies form_2) ->
  // should trigger.
  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(1);

  SelectDropdownOption("text_2_1", "option_2_1");

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

  AttachCustomDropdownOption("text_1", "option_1", {{"text_1", "apple"}});

  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(1);

  SelectDropdownOption("text_1", "option_1");

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

  AttachCustomDropdownOption("text_1", "option_1",
                             {{"text_1", "apple"}, {"text_2", "banana"}});

  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(0);

  SelectDropdownOption("text_1", "option_1");

  // 2 fields changed (< 3), but neither is kPrefixCompletion -> should NOT
  // trigger detection.
  task_environment_.FastForwardBy(base::Milliseconds(200));
}

// Test that if JS copies values to hidden fields (e.g. billing section hidden
// when "same as shipping" is checked), those hidden fields are ignored and not
// added to the logs.
TEST_F(JavaScriptAutofillTrackerTest, IgnoreHiddenFieldsInSameAsShippingForm) {
  LoadBillingAndShippingForm();

  // Attach shipping picker to shipping_street. When clicked, it sets the 4
  // shipping fields. The 'change' event listeners automatically copy those
  // values to the 4 hidden billing fields.
  AttachCustomDropdownOption("shipping_street", "shipping_option",
                             {{"shipping_street", "1600 Amphitheatre Pkwy"},
                              {"shipping_city", "Mountain View"},
                              {"shipping_state", "CA"},
                              {"shipping_zip", "94043"}});

  const std::vector<mojom::JavaScriptFieldModificationPtr>& logs =
      test_api(test_api(autofill_agent()).javascript_autofill_tracker())
          .js_logs();

  // Click shipping address option while same_as_shipping is checked.
  // JS sets all 8 fields, but only the 4 visible shipping fields should be
  // logged.
  SelectDropdownOption("shipping_street", "shipping_option");

  EXPECT_EQ(logs[0]->field_id, form_util::GetFieldRendererId(
                                   GetWebElementById("shipping_street")));
  EXPECT_EQ(logs[1]->field_id,
            form_util::GetFieldRendererId(GetWebElementById("shipping_city")));
  EXPECT_EQ(logs[2]->field_id,
            form_util::GetFieldRendererId(GetWebElementById("shipping_state")));
  EXPECT_EQ(logs[3]->field_id,
            form_util::GetFieldRendererId(GetWebElementById("shipping_zip")));

  task_environment_.FastForwardBy(base::Milliseconds(200));
  EXPECT_TRUE(logs.empty());
}

}  // namespace

}  // namespace autofill
