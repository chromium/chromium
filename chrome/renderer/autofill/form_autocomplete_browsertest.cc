// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/focus_test_utils.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_tracker_test_api.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_type.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebInputElement;
using blink::WebString;

namespace autofill {

using mojom::SubmissionSource;

namespace {

class FakeContentAutofillDriver : public mojom::AutofillDriver {
 public:
  FakeContentAutofillDriver() = default;

  ~FakeContentAutofillDriver() override = default;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutofillDriver> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  const FormData* form_submitted() const { return form_submitted_.get(); }

  SubmissionSource submission_source() const { return submission_source_; }

  const FormData* select_control_changed() const {
    return select_control_changed_.get();
  }

 private:
  // mojom::AutofillDriver:
  void FormsSeen(const std::vector<FormData>& updated_forms,
                 const std::vector<FormRendererId>& removed_forms) override {}

  void FormSubmitted(const FormData& form,
                     SubmissionSource source) override {
    form_submitted_ = std::make_unique<FormData>(form);
    submission_source_ = source;
  }

  void CaretMovedInFormField(const FormData& form,
                             FieldRendererId field_id,
                             const gfx::Rect& caret_bounds) override {}

  void TextFieldValueChanged(const FormData& form,
                             FieldRendererId field_id,
                             base::TimeTicks timestamp) override {}

  void TextFieldDidScroll(const FormData& form,
                          FieldRendererId field_id) override {}

  void SelectControlSelectionChanged(const FormData& form,
                                     FieldRendererId field_id) override {
    select_control_changed_ = std::make_unique<FormData>(form);
  }

  void JavaScriptChangedAutofilledValue(
      const FormData& form,
      FieldRendererId field_id,
      const std::u16string& old_value) override {}

  void AskForValuesToFill(const FormData& form,
                          FieldRendererId field_id,
                          const gfx::Rect& caret_bounds,
                          AutofillSuggestionTriggerSource trigger_source,
                          const std::optional<PasswordSuggestionRequest>&
                              password_request) override {}

  void HidePopup() override {}

  void SuppressAutomaticRefills(const FillId& fill_id) override {}

  void RequestRefill(const FillId& fill_id) override {}

  void FocusOnNonFormField() override {}

  void FocusOnFormField(const FormData& form,
                        FieldRendererId field_id) override {}

  void DidAutofillForm(const FormData& form) override {}

  void DidEndTextFieldEditing() override {}

  void SelectFieldOptionsDidChange(const autofill::FormData& form,
                                   FieldRendererId field_id) override {}

  void OnEmailVerificationTokenShared(FieldRendererId field_id) override {}

  // Records the form data received via FormSubmitted() call.
  std::unique_ptr<FormData> form_submitted_;

  SubmissionSource submission_source_;

  std::unique_ptr<FormData> select_control_changed_;

  mojo::AssociatedReceiverSet<mojom::AutofillDriver> receivers_;
};

// TODO(crbug.com/41495779): Update.
FormData CreateAutofillFormData(blink::WebLocalFrame* main_frame) {
  FormData data;
  data.set_name(u"name");
  data.set_url(GURL("http://example.com/"));
  data.set_action(GURL("http://example.com/blade.php"));
  data.set_renderer_id(test::MakeFormRendererId());  // Default value.

  WebDocument document = main_frame->GetDocument();
  WebFormControlElement fname_element =
      document.GetElementById(WebString("fname")).To<WebFormControlElement>();
  WebFormControlElement lname_element =
      document.GetElementById(WebString("lname")).To<WebFormControlElement>();

  FormFieldData field_data;
  field_data.set_name(u"fname");
  field_data.set_value(u"John");
  field_data.set_is_autofilled_according_to_renderer(true);
  field_data.set_renderer_id(form_util::GetFieldRendererId(fname_element));
  test_api(data).Append(field_data);

  if (lname_element) {
    field_data.set_name(u"lname");
    field_data.set_value(u"Smith");
    field_data.set_is_autofilled_according_to_renderer(true);
    field_data.set_renderer_id(form_util::GetFieldRendererId(lname_element));
    test_api(data).Append(field_data);
  }

  return data;
}

std::vector<FormFieldData::FillData> GetFieldsForFilling(
    const std::vector<FormData>& forms) {
  std::vector<FormFieldData::FillData> fields;
  for (const FormData& form : forms) {
    for (const FormFieldData& field : form.fields()) {
      fields.emplace_back(field);
    }
  }
  return fields;
}

class FormAutocompleteTest : public ChromeRenderViewTest {
 public:
  FormAutocompleteTest() = default;
  FormAutocompleteTest(const FormAutocompleteTest&) = delete;
  FormAutocompleteTest& operator=(const FormAutocompleteTest&) = delete;
  ~FormAutocompleteTest() override = default;

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // We only use the fake driver for main frame
    // because our test cases only involve the main frame.
    blink::AssociatedInterfaceProvider* remote_interfaces =
        GetMainRenderFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::AutofillDriver::Name_,
        base::BindRepeating(&FormAutocompleteTest::BindAutofillDriver,
                            base::Unretained(this)));

    focus_test_utils_ = std::make_unique<test::FocusTestUtils>(
        base::BindRepeating(&FormAutocompleteTest::ExecuteJavaScriptForTests,
                            base::Unretained(this)));
  }

  void BindAutofillDriver(mojo::ScopedInterfaceEndpointHandle handle) {
    fake_driver_.BindReceiver(
        mojo::PendingAssociatedReceiver<mojom::AutofillDriver>(
            std::move(handle)));
  }

  void SimulateElementClick(const WebElement element) {
    SimulatePointClick(element.BoundsInWidget().CenterPoint());
  }

  // Simulates receiving a message from the browser to fill a form.
  void SimulateFillForm() {
    FormData data = CreateAutofillFormData(GetMainFrame());
    SimulateFillForm(data);
  }

  void SimulateFillForm(const FormData& form_data) {
    WebDocument document = GetMainFrame()->GetDocument();
    WebFormControlElement fname_element =
        document.GetElementById(WebString("fname")).To<WebFormControlElement>();

    ASSERT_TRUE(fname_element);
    // This call is necessary to setup the autofill agent appropriate for the
    // user selection; simulates the menu actually popping up.
    SimulateElementClick(fname_element);

    autofill_agent_->ApplyFieldsAction(
        mojom::FormActionType::kFill, mojom::ActionPersistence::kFill,
        GetFieldsForFilling({form_data}), FillId::Create(),
        /*supports_refill=*/false);
  }

  std::string GetFocusLog() {
    return focus_test_utils_->GetFocusLog(GetMainFrame()->GetDocument());
  }

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  FakeContentAutofillDriver fake_driver_;
  std::unique_ptr<test::FocusTestUtils> focus_test_utils_;
};

// Tests that correct focus, change and blur events are emitted during the
// autofilling process when there is an initial focused element in a form
// having non-fillable fields.
TEST_F(FormAutocompleteTest, VerifyFocusAndBlurEventsAfterAutofill) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "<label>Middle Name:</label><input id='mname' name='2'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  SimulateFillForm();
  base::RunLoop().RunUntilIdle();

  // Expected Result in order:
  // * Change fname
  // * Blur fname
  // * Focus lname
  // * Change lname
  // * Blur lname
  // * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0");
}

// Tests that correct focus, change and blur events are emitted during the
// autofilling process when there is an initial focused element.
TEST_F(FormAutocompleteTest,
       VerifyFocusAndBlurEventsAfterAutofillWithFocusedElement) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  SimulateFillForm();
  base::RunLoop().RunUntilIdle();

  // Expected Result in order:
  // * Change fname
  // * Blur fname
  // * Focus lname
  // * Change lname
  // * Blur lname
  // * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0");
}

// Tests that correct focus, change and blur events are emitted during the
// autofilling process when there is an initial focused element in a form having
// single field.
TEST_F(FormAutocompleteTest,
       VerifyFocusAndBlurEventAfterAutofillWithFocusedElementForSingleElement) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  SimulateFillForm();
  base::RunLoop().RunUntilIdle();

  // Expected Result in order:
  // * Change fname
  EXPECT_EQ(GetFocusLog(), "c0");
}

// Tests that a field is added to the form between the times of triggering
// and executing the filling.
TEST_F(FormAutocompleteTest, VerifyFocusAndBlurEventAfterElementAdded) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  FormData data = CreateAutofillFormData(GetMainFrame());
  // Simulate that the form was modified between parsing and executing the fill.
  // The element is inserted at the beginning of the form to verify that
  // everything works correctly even if renderer_ids of the <input>
  // elements are not in ascending order.
  ExecuteJavaScriptForTests(
      "document.getElementById('fname').insertAdjacentHTML('beforebegin', "
      "'<label>Zip code:</label><input id=\"zip_code\"/>');");
  SimulateFillForm(data);
  base::RunLoop().RunUntilIdle();

  // Expected Result in order:
  // * Change fname
  // * Blur fname
  // * Focus lname
  // * Change lname
  // * Blur lname
  // * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0");
}

// Tests that a field is removed from the form between the times of
// triggering and executing the filling.
TEST_F(FormAutocompleteTest, VerifyFocusAndBlurEventAfterElementRemoved) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'/><br/>"
      "<label>Last Name:</label> <input id='lname' name='1'/><br/>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  // Simulate filling the form using Autofill.
  FormData data = CreateAutofillFormData(GetMainFrame());
  ExecuteJavaScriptForTests("document.getElementById('lname').remove()");
  SimulateFillForm(data);
  base::RunLoop().RunUntilIdle();

  // Expected Result in order:
  // * Change fname
  EXPECT_EQ(GetFocusLog(), "c0");
}

// Unit test for AutofillAgent::AcceptDataListSuggestion.
TEST_F(FormAutocompleteTest, AcceptDataListSuggestion) {
  LoadHTML(
      "<html>"
      "<input id='empty' type='email' multiple />"
      "<input id='multi_one' type='email' multiple value='one@example.com'/>"
      "<input id='multi_two' type='email' multiple"
      "  value='one@example.com,two@example.com'/>"
      "<input id='multi_trailing' type='email' multiple"
      "  value='one@example.com,two@example.com,'/>"
      "<input id='not_multi' type='email'"
      "  value='one@example.com,two@example.com,'/>"
      "<input id='not_email' type='text' multiple"
      "  value='one@example.com,two@example.com,'/>"
      "</html>");
  WebDocument document = GetMainFrame()->GetDocument();

  // Each case tests a different field value with the same suggestion.
  const std::u16string kSuggestion = u"suggestion@example.com";
  struct TestCase {
    std::string id;
    std::string expected;
  } cases[] = {
      // Empty text field; expect to populate with suggestion.
      {"empty", "suggestion@example.com"},
      // Single entry; expect to replace with suggestion.
      {"multi_one", "suggestion@example.com"},
      // Two comma-separated entries; expect to replace second with suggestion.
      {"multi_two", "one@example.com,suggestion@example.com"},
      // Two comma-separated entries with trailing comma; expect to append
      // suggestion.
      {"multi_trailing",
       "one@example.com,two@example.com,suggestion@example.com"},
      // Do not apply this logic for a non-multiple or non-email field.
      {"not_multi", "suggestion@example.com"},
      {"not_email", "suggestion@example.com"},
  };

  for (const auto& c : cases) {
    WebElement element = document.GetElementById(WebString::FromUtf8(c.id));
    ASSERT_TRUE(element);
    WebInputElement input_element = element.To<WebInputElement>();
    SimulateElementClick(input_element);

    autofill_agent_->AcceptDataListSuggestion(
        form_util::GetFieldRendererId(input_element), kSuggestion);
    EXPECT_EQ(c.expected, input_element.Value().Utf8()) << "Case id: " << c.id;
  }
}

TEST_F(FormAutocompleteTest, SelectControlChanged) {
  LoadHTML(
      "<html>"
      "<form>"
      "<select id='color'><option value='red'>red</option><option "
      "value='blue'>blue</option></select>"
      "</form>"
      "</html>");

  std::string change_value =
      "var color = document.getElementById('color');"
      "color.selectedIndex = 1;";

  // The click simulation is necessary to give the frame transient user
  // activation, otherwise the select value-change event will be ignored by the
  // agent.
  SimulateElementClick(
      GetMainFrame()->GetDocument().GetElementById(blink::WebString("color")));
  ExecuteJavaScriptForTests(change_value.c_str());
  base::RunLoop().RunUntilIdle();

  const FormData* form = fake_driver_.select_control_changed();
  ASSERT_TRUE(form);
  ASSERT_EQ(form->fields().size(), 1u);
  EXPECT_EQ(u"color", form->fields()[0].name());
  EXPECT_EQ(u"blue", form->fields()[0].value());
}

}  // namespace

}  // namespace autofill
