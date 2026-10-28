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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Pointee;

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

  auto HasFieldId(std::string_view id) {
    return Pointee(Field(&mojom::JavaScriptFieldModification::field_id,
                         form_util::GetFieldRendererId(GetWebElementById(id))));
  }

  auto IsFieldModification(std::string_view id,
                           mojom::JavaScriptModificationType type) {
    return Pointee(AllOf(
        Field(&mojom::JavaScriptFieldModification::field_id,
              form_util::GetFieldRendererId(GetWebElementById(id))),
        Field(&mojom::JavaScriptFieldModification::modification_type, type)));
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

  const std::vector<JavaScriptAutofillTracker::LogRecord>& logs =
      test_api(test_api(autofill_agent()).javascript_autofill_tracker())
          .js_logs();

  // 1. JS change without user activation -> should not log.
  js_set_value("text_1", "js_val_1");
  EXPECT_TRUE(logs.empty());

  // 2. JS change with user activation and mousedown -> should log.
  AttachCustomDropdownOption("text_1", "opt_1", {{"text_1", "js_val_2"}});
  SelectDropdownOption("text_1", "opt_1");

  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0].field_id, form_util::GetFieldRendererId(text1));
  EXPECT_EQ(logs[0].previous_value, u"js_val_1");

  // Clear logs by waiting for timer.
  EXPECT_CALL(
      autofill_driver(),
      DidDetectJavaScriptAutofill(
          _, form_util::GetFieldRendererId(text1),
          ElementsAre(IsFieldModification(
              "text_1", mojom::JavaScriptModificationType::kReassignment)),
          _));
  task_environment_.FastForwardBy(base::Milliseconds(1000));
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
  EXPECT_EQ(logs[0].field_id, form_util::GetFieldRendererId(text1));
  EXPECT_EQ(logs[0].previous_value, u"js_val_3");

  // 6. JS change to a prefix completion -> should log kPrefixCompletion.
  EXPECT_CALL(autofill_driver(),
              DidDetectJavaScriptAutofill(
                  _, form_util::GetFieldRendererId(text1),
                  ElementsAre(IsFieldModification(
                      "text_1", mojom::JavaScriptModificationType::kTrivial)),
                  _));
  task_environment_.FastForwardBy(base::Milliseconds(1000));
  AttachCustomDropdownOption("text_1", "opt_prefix",
                             {{"text_1", "js_val_3_more"}});
  SelectDropdownOption("text_1", "opt_prefix");
  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0].previous_value, u"js_val_3");

  // 7. JS change to a non-prefix substring completion -> should log
  // kSubstringCompletion.
  EXPECT_CALL(
      autofill_driver(),
      DidDetectJavaScriptAutofill(
          _, form_util::GetFieldRendererId(text1),
          ElementsAre(IsFieldModification(
              "text_1", mojom::JavaScriptModificationType::kPrefixCompletion)),
          _));
  task_environment_.FastForwardBy(base::Milliseconds(1000));
  AttachCustomDropdownOption("text_1", "opt_substring",
                             {{"text_1", "prefix_js_val_3_more"}});
  SelectDropdownOption("text_1", "opt_substring");
  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0].previous_value, u"js_val_3_more");

  // 8. JS change from non-empty to empty -> should log kClearing.
  EXPECT_CALL(autofill_driver(),
              DidDetectJavaScriptAutofill(
                  _, form_util::GetFieldRendererId(text1),
                  ElementsAre(IsFieldModification(
                      "text_1",
                      mojom::JavaScriptModificationType::kSubstringCompletion)),
                  _));
  task_environment_.FastForwardBy(base::Milliseconds(1000));
  AttachCustomDropdownOption("text_1", "opt_clear", {{"text_1", ""}});
  SelectDropdownOption("text_1", "opt_clear");
  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0].previous_value, u"prefix_js_val_3_more");

  // 9. JS change from empty to non-empty -> should log kEmptyToNonEmpty.
  EXPECT_CALL(autofill_driver(),
              DidDetectJavaScriptAutofill(
                  _, form_util::GetFieldRendererId(text1),
                  ElementsAre(IsFieldModification(
                      "text_1", mojom::JavaScriptModificationType::kClearing)),
                  _));
  task_environment_.FastForwardBy(base::Milliseconds(1000));
  AttachCustomDropdownOption("text_1", "opt_new", {{"text_1", "new_val"}});
  SelectDropdownOption("text_1", "opt_new");
  ASSERT_EQ(logs.size(), 1u);
  EXPECT_EQ(logs[0].previous_value, u"");

  EXPECT_CALL(
      autofill_driver(),
      DidDetectJavaScriptAutofill(
          _, form_util::GetFieldRendererId(text1),
          ElementsAre(IsFieldModification(
              "text_1", mojom::JavaScriptModificationType::kEmptyToNonEmpty)),
          _));
  task_environment_.FastForwardBy(base::Milliseconds(1000));
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

  const std::vector<JavaScriptAutofillTracker::LogRecord>& logs =
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
  task_environment_.FastForwardBy(base::Milliseconds(1000));
  EXPECT_TRUE(logs.empty());
  testing::Mock::VerifyAndClearExpectations(&autofill_driver());

  // Focus text_2_1 in form_2 and click option_2_1 (which modifies form_2) ->
  // should trigger.
  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(1);

  SelectDropdownOption("text_2_1", "option_2_1");

  ASSERT_EQ(logs.size(), 3u);

  task_environment_.FastForwardBy(base::Milliseconds(1000));
  EXPECT_TRUE(logs.empty());
}

// Test that if JS copies values to hidden fields (e.g. billing section hidden
// when "same as shipping" is checked), those hidden fields are filtered out
// when detecting JS autofill.
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

  const std::vector<JavaScriptAutofillTracker::LogRecord>& logs =
      test_api(test_api(autofill_agent()).javascript_autofill_tracker())
          .js_logs();

  // Click shipping address option while same_as_shipping is checked.
  // JS sets all 8 fields, which are all initially logged, and only the 4
  // visible shipping fields are kept when the detection timer fires.
  SelectDropdownOption("shipping_street", "shipping_option");
  ASSERT_EQ(logs.size(), 8u);

  EXPECT_CALL(
      autofill_driver(),
      DidDetectJavaScriptAutofill(
          _,
          form_util::GetFieldRendererId(GetWebElementById("shipping_street")),
          ElementsAre(HasFieldId("shipping_street"),
                      HasFieldId("shipping_city"), HasFieldId("shipping_state"),
                      HasFieldId("shipping_zip")),
          _));

  task_environment_.FastForwardBy(base::Milliseconds(1000));
  EXPECT_TRUE(logs.empty());
}

// Test that fields dynamically created in JS, populated while still
// disconnected from the DOM, and then attached to the form are tracked and
// detected.
TEST_F(JavaScriptAutofillTrackerTest,
       DetectDynamicallyCreatedAndPopulatedFields) {
  LoadHTML(R"(
      <form id="form_id">
        <input id="street">
        <div id="option">Option</div>
      </form>)");

  ExecuteJavaScriptForTests(R"(
    document.getElementById('option').addEventListener('mousedown', (e) => {
      e.preventDefault();
      document.getElementById('street').value = '1600 Amphitheatre Pkwy';

      const form = document.getElementById('form_id');
      for (const [id, val] of [
          ['city', 'Mountain View'],
          ['state', 'CA'],
          ['zip', '94043']]) {
        const input = document.createElement('input');
        input.id = id;
        // Set value while the element is still disconnected from the DOM.
        input.value = val;
        form.appendChild(input);
      }
    });
  )");

  SelectDropdownOption("street", "option");

  EXPECT_CALL(autofill_driver(),
              DidDetectJavaScriptAutofill(
                  _, form_util::GetFieldRendererId(GetWebElementById("street")),
                  ElementsAre(HasFieldId("street"), HasFieldId("city"),
                              HasFieldId("state"), HasFieldId("zip")),
                  _));

  task_environment_.FastForwardBy(base::Milliseconds(1000));
}

// Test that filling shipping form via dropdown, unticking same_as_shipping to
// reveal billing section, focusing a billing field, and ticking
// same_as_shipping again does NOT trigger JS autofill detection.
TEST_F(JavaScriptAutofillTrackerTest,
       IgnoreCopyOnCheckingSameAsShippingCheckbox) {
  // 1. Load the billing and shipping form.
  LoadBillingAndShippingForm();

  AttachCustomDropdownOption("shipping_street", "shipping_option",
                             {{"shipping_street", "1600 Amphitheatre Pkwy"},
                              {"shipping_city", "Mountain View"},
                              {"shipping_state", "CA"},
                              {"shipping_zip", "94043"}});

  // 2. Fill the shipping form using the dropdown.
  SelectDropdownOption("shipping_street", "shipping_option");
  task_environment_.FastForwardBy(base::Milliseconds(1000));

  // 3. Untick the checkbox to reveal the billing section and empty it.
  SimulateElementClickAndWait("same_as_shipping");

  // 4. Focus on a field in the billing section.
  SimulateElementClickAndWait("billing_street");

  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(0);

  // 5. Check the checkbox again. This causes a similar effect to clicking on a
  // dropdown option, while a field in the form is focused, and causes fields to
  // be filled by JS. However, this should not be detected by the tracker since
  // it is obviously not a use case of selecting a dropdown item.
  SimulateElementClickAndWait("same_as_shipping");

  task_environment_.FastForwardBy(base::Milliseconds(1000));
}

// Test that clicking a reset/clear button that empties form fields via JS does
// NOT trigger JS autofill detection.
TEST_F(JavaScriptAutofillTrackerTest, IgnoreResetButtonClearingFields) {
  LoadHTML(R"(
      <form id="form_id">
        <input id="text_1" value="initial 1">
        <input id="text_2" value="initial 2">
        <input id="text_3" value="initial 3">
        <button id="reset_btn" type="button">Reset</button>
      </form>)");

  ExecuteJavaScriptForTests(R"(
    document.getElementById('reset_btn').addEventListener('click', () => {
      document.getElementById('text_1').value = '';
      document.getElementById('text_2').value = '';
      document.getElementById('text_3').value = '';
    });
  )");

  // Focus on a field first so document.FocusedElement() is valid.
  Focus("text_1");

  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(0);

  // Click the reset button to clear all 3 fields.
  SimulateElementClickAndWait("reset_btn");

  task_environment_.FastForwardBy(base::Milliseconds(1000));
}

// Test that clicking a dropdown option rendered as a `<button>` (either with
// `type="button"` or without a `type` attribute, and with `role="option"` on
// the button or `role="listbox"` / `role="menu"` on an ancestor) triggers JS
// autofill detection, while explicit `type="submit"` buttons are ignored.
TEST_F(JavaScriptAutofillTrackerTest, DetectButtonWithOptionRole) {
  LoadHTML(R"(
      <form id="form_id">
        <input id="street">
        <input id="city">
        <input id="state">
        <button id="option_btn_1" type="button" role="option">
          1600 Amphitheatre Pkwy, Mountain View, CA
        </button>
        <ul role="listbox">
          <li>
            <button id="option_btn_2">
              111 8th Ave, New York, NY
            </button>
          </li>
        </ul>
        <button id="submit_btn" type="submit" role="option">Submit</button>
      </form>)");

  ExecuteJavaScriptForTests(R"(
    document.getElementById('option_btn_1').addEventListener('click', () => {
      document.getElementById('street').value = '1600 Amphitheatre Pkwy';
      document.getElementById('city').value = 'Mountain View';
      document.getElementById('state').value = 'CA';
    });
    document.getElementById('option_btn_2').addEventListener('click', (e) => {
      e.preventDefault();
      document.getElementById('street').value = '111 8th Ave';
      document.getElementById('city').value = 'New York';
      document.getElementById('state').value = 'NY';
    });
    document.getElementById('submit_btn').addEventListener('click', (e) => {
      e.preventDefault();
      document.getElementById('street').value = 'Submitted';
      document.getElementById('city').value = 'Submitted';
      document.getElementById('state').value = 'Submitted';
    });
  )");

  // 1. `<button type="button" role="option">` -> detected.
  EXPECT_CALL(autofill_driver(),
              DidDetectJavaScriptAutofill(
                  _, form_util::GetFieldRendererId(GetWebElementById("street")),
                  ElementsAre(HasFieldId("street"), HasFieldId("city"),
                              HasFieldId("state")),
                  _));
  SelectDropdownOption("street", "option_btn_1");
  task_environment_.FastForwardBy(base::Milliseconds(1000));
  testing::Mock::VerifyAndClearExpectations(&autofill_driver());

  // 2. Default `<button>` (no `type` attribute) inside `role="listbox"` ->
  // detected.
  EXPECT_CALL(autofill_driver(),
              DidDetectJavaScriptAutofill(
                  _, form_util::GetFieldRendererId(GetWebElementById("street")),
                  ElementsAre(HasFieldId("street"), HasFieldId("city"),
                              HasFieldId("state")),
                  _));
  SelectDropdownOption("street", "option_btn_2");
  task_environment_.FastForwardBy(base::Milliseconds(1000));
  testing::Mock::VerifyAndClearExpectations(&autofill_driver());

  // 3. Explicit `<button type="submit">` -> ignored even with `role="option"`.
  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(0);
  SelectDropdownOption("street", "submit_btn");
  task_environment_.FastForwardBy(base::Milliseconds(1000));
}

// Test that clicking empty space on `<form>` or `<body>` (which blurs the
// focused input and may trigger `blur` listeners that modify fields) does not
// trigger JS autofill detection.
TEST_F(JavaScriptAutofillTrackerTest, IgnoreClicksOnFormOrBody) {
  LoadHTML(R"(
      <body id="body_id">
        <form id="form_id">
          <input id="zip" value="94043">
          <input id="city">
          <input id="state">
        </form>
      </body>)");

  ExecuteJavaScriptForTests(R"(
    document.getElementById('zip').addEventListener('blur', () => {
      document.getElementById('city').value = 'Mountain View';
      document.getElementById('state').value = 'CA';
    });
  )");

  EXPECT_CALL(autofill_driver(), DidDetectJavaScriptAutofill).Times(0);

  SelectDropdownOption("zip", "form_id");
  task_environment_.FastForwardBy(base::Milliseconds(1000));

  ExecuteJavaScriptForTests(R"(
    document.getElementById('city').value = '';
    document.getElementById('state').value = '';
  )");

  SelectDropdownOption("zip", "body_id");
  task_environment_.FastForwardBy(base::Milliseconds(1000));
}

// Test that when JS writes to the same field multiple times during the same
// selection (e.g., clearing the input first and then writing the selected
// option's value), only a single modification entry is kept and its
// modification type is computed from the field's initial value prior to the
// first write and its final value.
TEST_F(JavaScriptAutofillTrackerTest,
       MultipleWritesToSameFieldUsesInitialAndFinalValue) {
  LoadHTML(R"(
      <form id="form_id">
        <input id="state" value="Cal">
      </form>)");

  // Selecting the option first clears `state` ("Cal" -> "") and then sets it to
  // "California" ("" -> "California"). Relative to the initial value "Cal", the
  // net modification type should be `kPrefixCompletion` (rather than
  // `kClearing` from the first write or `kEmptyToNonEmpty` from the second
  // write alone).
  AttachCustomDropdownOption("state", "option",
                             {{"state", ""}, {"state", "California"}});

  const std::vector<JavaScriptAutofillTracker::LogRecord>& logs =
      test_api(test_api(autofill_agent()).javascript_autofill_tracker())
          .js_logs();

  SelectDropdownOption("state", "option");
  ASSERT_EQ(logs.size(), 2u);
  EXPECT_EQ(logs[0].previous_value, u"Cal");
  EXPECT_EQ(logs[1].previous_value, u"");

  EXPECT_CALL(
      autofill_driver(),
      DidDetectJavaScriptAutofill(
          _, form_util::GetFieldRendererId(GetWebElementById("state")),
          ElementsAre(IsFieldModification(
              "state", mojom::JavaScriptModificationType::kPrefixCompletion)),
          _));

  task_environment_.FastForwardBy(base::Milliseconds(1000));
}

}  // namespace

}  // namespace autofill
