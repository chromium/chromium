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

  bool known_success() const { return known_success_; }

  SubmissionSource submission_source() const { return submission_source_; }

  const FormData* select_control_changed() const {
    return select_control_changed_.get();
  }

 private:
  // mojom::AutofillDriver:
  void FormsSeen(const std::vector<FormData>& updated_forms,
                 const std::vector<FormRendererId>& removed_forms) override {}

  void FormSubmitted(const FormData& form,
                     bool known_success,
                     SubmissionSource source) override {
    form_submitted_ = std::make_unique<FormData>(form);
    known_success_ = known_success;
    submission_source_ = source;
  }

  void CaretMovedInFormField(const FormData& form,
                             FieldRendererId field_id,
                             const gfx::Rect& caret_bounds) override {}

  void TextFieldDidChange(const FormData& form,
                          FieldRendererId field_id,
                          base::TimeTicks timestamp) override {}

  void TextFieldDidScroll(const FormData& form,
                          FieldRendererId field_id) override {}

  void SelectControlDidChange(const FormData& form,
                              FieldRendererId field_id) override {
    select_control_changed_ = std::make_unique<FormData>(form);
  }

  void JavaScriptChangedAutofilledValue(const FormData& form,
                                        FieldRendererId field_id,
                                        const std::u16string& old_value,
                                        bool formatting_only) override {}

  void AskForValuesToFill(
      const FormData& form,
      FieldRendererId field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source) override {}

  void HidePopup() override {}

  void FocusOnNonFormField() override {}

  void FocusOnFormField(const FormData& form,
                        FieldRendererId field_id) override {}

  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override {}

  void DidEndTextFieldEditing() override {}

  void SelectFieldOptionsDidChange(const autofill::FormData& form) override {}

  // Records the form data received via FormSubmitted() call.
  std::unique_ptr<FormData> form_submitted_;

  bool known_success_;

  SubmissionSource submission_source_;

  std::unique_ptr<FormData> select_control_changed_;

  mojo::AssociatedReceiverSet<mojom::AutofillDriver> receivers_;
};

// Helper function to verify the form-related messages received from the
// renderer. The same data is expected in both messages. Depending on
// `expect_submitted_message`, will verify presence of FormSubmitted message.
void VerifyReceivedRendererMessages(
    const FakeContentAutofillDriver& fake_driver,
    const std::string& fname,
    const std::string& lname,
    bool expect_known_success,
    SubmissionSource expect_submission_source) {
  ASSERT_TRUE(fake_driver.form_submitted());

  // The tuple also includes a timestamp, which is ignored.
  const FormData& submitted_form = *(fake_driver.form_submitted());
  ASSERT_LE(2U, submitted_form.fields().size());
  EXPECT_EQ(u"fname", submitted_form.fields()[0].name());
  EXPECT_EQ(base::UTF8ToUTF16(fname), submitted_form.fields()[0].value());
  EXPECT_EQ(u"lname", submitted_form.fields()[1].name());
  EXPECT_EQ(expect_known_success, fake_driver.known_success());
  EXPECT_EQ(expect_submission_source,
            mojo::ConvertTo<SubmissionSource>(fake_driver.submission_source()));
}

void VerifyReceivedAddressRendererMessages(
    const FakeContentAutofillDriver& fake_driver,
    const std::string& address,
    bool expect_known_success,
    SubmissionSource expect_submission_source) {
  ASSERT_TRUE(fake_driver.form_submitted());

  // The tuple also includes a timestamp, which is ignored.
  const FormData& submitted_form = *(fake_driver.form_submitted());
  ASSERT_LE(1U, submitted_form.fields().size());
  EXPECT_EQ(u"address", submitted_form.fields()[0].name());
  EXPECT_EQ(base::UTF8ToUTF16(address), submitted_form.fields()[0].value());
  EXPECT_EQ(expect_known_success, fake_driver.known_success());
  EXPECT_EQ(expect_submission_source,
            mojo::ConvertTo<SubmissionSource>(fake_driver.submission_source()));
}

// Helper function to verify that NO form-related messages are received from the
// renderer.
void VerifyNoSubmitMessagesReceived(
    const FakeContentAutofillDriver& fake_driver) {
  // No submission messages sent.
  EXPECT_EQ(nullptr, fake_driver.form_submitted());
}

// TODO(crbug.com/41495779): Update.
FormData CreateAutofillFormData(blink::WebLocalFrame* main_frame) {
  FormData data;
  data.set_name(u"name");
  data.set_url(GURL("http://example.com/"));
  data.set_action(GURL("http://example.com/blade.php"));
  data.set_renderer_id(test::MakeFormRendererId());  // Default value.

  WebDocument document = main_frame->GetDocument();
  WebFormControlElement fname_element =
      document.GetElementById(WebString::FromUTF8("fname"))
          .To<WebFormControlElement>();
  WebFormControlElement lname_element =
      document.GetElementById(WebString::FromUTF8("lname"))
          .To<WebFormControlElement>();

  FormFieldData field_data;
  field_data.set_name(u"fname");
  field_data.set_value(u"John");
  field_data.set_is_autofilled(true);
  field_data.set_renderer_id(form_util::GetFieldRendererId(fname_element));
  test_api(data).Append(field_data);

  if (lname_element) {
    field_data.set_name(u"lname");
    field_data.set_value(u"Smith");
    field_data.set_is_autofilled(true);
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
        document.GetElementById(WebString::FromUTF8("fname"))
            .To<WebFormControlElement>();

    ASSERT_TRUE(fname_element);
    // This call is necessary to setup the autofill agent appropriate for the
    // user selection; simulates the menu actually popping up.
    SimulateElementClick(fname_element);

    autofill_agent_->ApplyFieldsAction(mojom::FormActionType::kFill,
                                       mojom::ActionPersistence::kFill,
                                       GetFieldsForFilling({form_data}));
  }

  // This triggers a layout update to apply JS changes like display = 'none'.
  void ForceLayoutUpdate() {
    GetWebFrameWidget()->UpdateAllLifecyclePhases(
        blink::DocumentUpdateReason::kTest);
  }

  std::string GetFocusLog() {
    return focus_test_utils_->GetFocusLog(GetMainFrame()->GetDocument());
  }

  test::AutofillUnitTestEnvironment autofill_test_environment_;
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
    WebElement element = document.GetElementById(WebString::FromUTF8(c.id));
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

  ExecuteJavaScriptForTests(change_value.c_str());
  WebElement element =
      GetMainFrame()->GetDocument().GetElementById(blink::WebString("color"));
  static_cast<blink::WebAutofillClient*>(autofill_agent_)
      ->SelectControlDidChange(
          *reinterpret_cast<blink::WebFormControlElement*>(&element));
  base::RunLoop().RunUntilIdle();

  const FormData* form = fake_driver_.select_control_changed();
  ASSERT_TRUE(form);
  ASSERT_EQ(form->fields().size(), 1u);
  EXPECT_EQ(u"color", form->fields()[0].name());
  EXPECT_EQ(u"blue", form->fields()[0].value());
}

// Parameterized test for submission detection. The parameter dictates whether
// the tests run with `kAutofillReplaceFormElementObserver` enabled or not.
class FormAutocompleteSubmissionTest : public FormAutocompleteTest,
                                       public testing::WithParamInterface<int> {
 public:
  FormAutocompleteSubmissionTest() {
    EXPECT_LE(GetParam(), 2);
    std::vector<base::test::FeatureRef> features = {
        features::kAutofillReplaceCachedWebElementsByRendererIds,
        features::kAutofillReplaceFormElementObserver};

    std::vector<base::test::FeatureRef> enabled_features(
        features.begin(), features.begin() + GetParam());
    std::vector<base::test::FeatureRef> disabled_features(
        features.begin() + GetParam(), features.end());
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(AutofillSubmissionTest,
                         FormAutocompleteSubmissionTest,
                         ::testing::Values(0, 1, 2));

// Tests that submitting a form generates FormSubmitted message with the form
// fields.
TEST_P(FormAutocompleteSubmissionTest, NormalFormSubmit) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='about:blank'>"
      "<input name='fname' id='fname'/>"
      "<input name='lname' value='Deckard'/></form></html>");
  SimulateUserInputChangeForElementById("fname", "Rick");

  // Submit the form.
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 false /* expect_known_success */,
                                 SubmissionSource::FORM_SUBMISSION);
}

// Tests that FormSubmitted message is generated even the submit event isn't
// propagated by Javascript.
TEST_P(FormAutocompleteSubmissionTest, SubmitEventPrevented) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'><input name='fname' id='fname'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form>"
      "</html>");
  SimulateUserInputChangeForElementById("fname", "Rick");

  // Submit the form.
  ExecuteJavaScriptForTests(
      "var form = document.forms[0];"
      "form.onsubmit = function(event) { event.preventDefault(); };"
      "document.querySelector('input[type=submit]').click();");
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 false /* expect_known_success */,
                                 SubmissionSource::FORM_SUBMISSION);
}

// Tests that having the form disappear after autofilling triggers submission
// from Autofill's point of view.
TEST_P(FormAutocompleteSubmissionTest, DomMutationAfterAutofill) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillAcceptDomMutationAfterAutofillSubmission};
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname'/>"
      "<input name='lname'/></form></html>");

  // Simulate removing the form after autofilling.
  SimulateFillForm();
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "John", "Smith",
                                 /*expect_known_success=*/true,
                                 SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL);
}

// Tests that completing an Ajax request and having the form disappear will
// trigger submission from Autofill's point of view.
TEST_P(FormAutocompleteSubmissionTest, AjaxSucceeded_NoLongerVisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");
  SimulateUserInputChangeForElementById("fname", "Rick");

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 true /* expect_known_success */,
                                 SubmissionSource::XHR_SUCCEEDED);
}

// Tests that completing an Ajax request and having the form with a specific
// action disappear will trigger submission from Autofill's point of view, even
// if there is another form with the same data but different action on the page.
TEST_P(FormAutocompleteSubmissionTest,
       AjaxSucceeded_NoLongerVisible_DifferentActionsSameData) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form>"
      "<form id='myForm2' action='http://example.com/runner.php'>"
      "<input name='fname' id='fname2' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");
  SimulateUserInputChangeForElementById("fname", "Rick");

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 true /* expect_known_success */,
                                 SubmissionSource::XHR_SUCCEEDED);
}

// Tests that completing an Ajax request and having the form with no action
// specified disappear will trigger submission from Autofill's point of view,
// even if there is still another form with no action in the page. It will
// compare field data within the forms.
// TODO(kolos) Re-enable when the implementation of IsFormVisible is on-par
// for these platforms.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NoLongerVisibleBothNoActions DISABLED_NoLongerVisibleBothNoActions
#else
#define MAYBE_NoLongerVisibleBothNoActions NoLongerVisibleBothNoActions
#endif
TEST_P(FormAutocompleteSubmissionTest, MAYBE_NoLongerVisibleBothNoActions) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form>"
      "<form id='myForm2'>"
      "<input name='fname' id='fname2' value='John'/>"
      "<input name='lname' value='Doe'/><input type=submit></form></html>");
  SimulateUserInputChangeForElementById("fname", "Rick");

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 true /* expect_known_success */,
                                 SubmissionSource::XHR_SUCCEEDED);
}

// Tests that completing an Ajax request and having the form with no action
// specified disappear will trigger submission from Autofill's point of view.
TEST_P(FormAutocompleteSubmissionTest, AjaxSucceeded_NoLongerVisible_NoAction) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");
  SimulateUserInputChangeForElementById("fname", "Rick");

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 true /* expect_known_success */,
                                 SubmissionSource::XHR_SUCCEEDED);
}

// Tests that completing an Ajax request but leaving a form visible will not
// trigger submission from Autofill's point of view.
TEST_P(FormAutocompleteSubmissionTest, AjaxSucceeded_StillVisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  SimulateUserInputChangeForElementById("fname", "Rick");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  // No submission messages sent.
  VerifyNoSubmitMessagesReceived(fake_driver_);
}

// Tests that completing an Ajax request without any prior form interaction
// does not trigger form submission from Autofill's point of view.
TEST_P(FormAutocompleteSubmissionTest,
       AjaxSucceeded_NoFormInteractionInvisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // No form interaction.

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing without prior user interaction.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  // No submission messages sent.
  VerifyNoSubmitMessagesReceived(fake_driver_);
}

// Tests that completing an Ajax request after having autofilled a form,
// with the form disappearing, will trigger submission from Autofill's
// point of view.
TEST_P(FormAutocompleteSubmissionTest, AjaxSucceeded_FilledFormIsInvisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname'/>"
      "<input name='lname'/></form></html>");
  SimulateFillForm();

  // Simulate user input since ajax request doesn't fire submission message
  // if there is no user input.
  SimulateUserInputChangeForElementById("fname", "Rick");

  // Simulate removing the form just before the ajax request completes.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('myForm');"
      "element.parentNode.removeChild(element);");

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Smith",
                                 true /* expect_known_success */,
                                 SubmissionSource::XHR_SUCCEEDED);
}

// Tests that completing an Ajax request after having autofilled a form,
// without the form disappearing, will not trigger submission from Autofill's
// point of view.
TEST_P(FormAutocompleteSubmissionTest, AjaxSucceeded_FilledFormStillVisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Rick'/>"
      "<input name='lname' value='Deckard'/></form></html>");
  SimulateFillForm();

  // Form still visible.

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  // No submission messages sent.
  VerifyNoSubmitMessagesReceived(fake_driver_);
}

// Tests that completing an Ajax request without a form present will still
// trigger submission, if all the inputs the user has modified disappear.
TEST_P(FormAutocompleteSubmissionTest, AjaxSucceeded_FormlessElements) {
  // Load a "form." Note that kRequiredFieldsForUpload fields are required
  // for the formless logic to trigger, so we add a throwaway third field.
  LoadHTML(
      "<head><title>Checkout</title></head>"
      "<input type='text' name='fname' id='fname'/>"
      "<input type='text' name='lname' value='Puckett'/>"
      "<input type='number' name='number' value='34'/>");
  SimulateUserInputChangeForElementById("fname", "Kirby");

  // Remove element from view.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('fname');"
      "element.style.display = 'none';");
  ForceLayoutUpdate();

  // Simulate AJAX request.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Kirby", "Puckett",
                                 true /* expect_known_success */,
                                 SubmissionSource::XHR_SUCCEEDED);
}

// Tests that submitting a form that has autocomplete="off" generates
// WillSubmitForm and FormSubmitted messages.
TEST_P(FormAutocompleteSubmissionTest, AutoCompleteOffFormSubmit) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' autocomplete='off' action='about:blank'>"
      "<input name='fname' id='fname'/>"
      "<input name='lname' value='Deckard'/>"
      "</form></html>");
  SimulateUserInputChangeForElementById("fname", "Rick");

  // Submit the form.
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 false /* expect_known_success */,
                                 SubmissionSource::FORM_SUBMISSION);
}

// Tests that fields with autocomplete off are submitted.
TEST_P(FormAutocompleteSubmissionTest, AutoCompleteOffInputSubmit) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='about:blank'>"
      "<input name='fname' id='fname'/>"
      "<input name='lname' value='Deckard' autocomplete='off'/>"
      "</form></html>");
  SimulateUserInputChangeForElementById("fname", "Rick");

  // Submit the form.
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 false /* expect_known_success */,
                                 SubmissionSource::FORM_SUBMISSION);
}

// Tests that submitting a form that has been dynamically set as autocomplete
// off generates WillSubmitForm and FormSubmitted messages.
// Note: We previously did the opposite, for bug http://crbug.com/36520
TEST_P(FormAutocompleteSubmissionTest, DynamicAutoCompleteOffFormSubmit) {
  LoadHTML(
      "<html><form id='myForm' action='about:blank'>"
      "<input name='fname' id='fname'/>"
      "<input name='lname' value='Deckard'/></form></html>");
  SimulateUserInputChangeForElementById("fname", "Rick");

  WebElement element =
      GetMainFrame()->GetDocument().GetElementById(blink::WebString("myForm"));
  ASSERT_TRUE(element);
  blink::WebFormElement form = element.To<blink::WebFormElement>();
  EXPECT_TRUE(form.AutoComplete());

  // Dynamically mark the form as autocomplete off.
  ExecuteJavaScriptForTests(
      "document.getElementById('myForm')."
      "setAttribute('autocomplete', 'off');");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(form.AutoComplete());

  // Submit the form.
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 false /* expect_known_success */,
                                 SubmissionSource::FORM_SUBMISSION);
}

TEST_P(FormAutocompleteSubmissionTest, FormSubmittedByDOMMutationAfterXHR) {
  LoadHTML(
      "<html>"
      "<input type='text' id='address_field' name='address' autocomplete='on'>"
      "</html>");
  SimulateUserInputChangeForElementById("address_field", "City");
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();

  // Hide elements to simulate successful form submission.
  std::string hide_elements =
      "var address = document.getElementById('address_field');"
      "address.style = 'display:none';";
  ExecuteJavaScriptForTests(hide_elements.c_str());
  ForceLayoutUpdate();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedAddressRendererMessages(fake_driver_, "City",
                                        true /* expect_known_success */,
                                        SubmissionSource::XHR_SUCCEEDED);
}

TEST_P(FormAutocompleteSubmissionTest, FormSubmittedBySameDocumentNavigation) {
  LoadHTML(
      "<html>"
      "<input type='text' id='address_field' name='address' autocomplete='on'>"
      "</html>");
  SimulateUserInputChangeForElementById("address_field", "City");

  // Hide elements to simulate successful form submission.
  std::string hide_elements =
      "var address = document.getElementById('address_field');"
      "address.style = 'display:none';";

  ExecuteJavaScriptForTests(hide_elements.c_str());

  // Simulate same document navigation.
  test_api(test_api(*autofill_agent_).form_tracker())
      .DidFinishSameDocumentNavigation();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedAddressRendererMessages(
      fake_driver_, "City", true /* expect_known_success */,
      SubmissionSource::SAME_DOCUMENT_NAVIGATION);
}

TEST_P(FormAutocompleteSubmissionTest, FormSubmittedByProbablyFormSubmitted) {
  LoadHTML(
      "<html>"
      "<input type='text' id='address_field' name='address' autocomplete='on'>"
      "</html>");
  SimulateUserInputChangeForElementById("address_field", "City");

  // Hide elements to simulate successful form submission.
  std::string hide_elements =
      "var address = document.getElementById('address_field');"
      "address.style = 'display:none';";

  ExecuteJavaScriptForTests(hide_elements.c_str());

  // Simulate navigation.
  test_api(test_api(*autofill_agent_).form_tracker())
      .FireProbablyFormSubmitted();

  base::RunLoop().RunUntilIdle();

  VerifyReceivedAddressRendererMessages(
      fake_driver_, "City", false /* expect_known_success */,
      SubmissionSource::PROBABLY_FORM_SUBMITTED);
}

}  // namespace

}  // namespace autofill
