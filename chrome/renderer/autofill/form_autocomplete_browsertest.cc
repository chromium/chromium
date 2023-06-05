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
#include "components/autofill/content/renderer/focus_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_element.h"
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

  ~FakeContentAutofillDriver() override {}

  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutofillDriver> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  bool did_unfocus_form() const { return did_unfocus_form_; }

  bool had_interacted_form() const { return had_interacted_form_; }

  const FormData* form_submitted() const { return form_submitted_.get(); }

  bool known_success() const { return known_success_; }

  SubmissionSource submission_source() const { return submission_source_; }

  const FormFieldData* select_control_changed() const {
    return select_control_changed_.get();
  }

 private:
  // mojom::AutofillDriver:
  void SetFormToBeProbablySubmitted(
      const absl::optional<FormData>& form) override {}

  void FormsSeen(const std::vector<FormData>& updated_forms,
                 const std::vector<FormRendererId>& removed_forms) override {}

  void FormSubmitted(const FormData& form,
                     bool known_success,
                     SubmissionSource source) override {
    form_submitted_ = std::make_unique<FormData>(form);
    known_success_ = known_success;
    submission_source_ = source;
  }

  void TextFieldDidChange(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          base::TimeTicks timestamp) override {}

  void TextFieldDidScroll(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box) override {}

  void SelectControlDidChange(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override {
    select_control_changed_ = std::make_unique<FormFieldData>(field);
  }

  void JavaScriptChangedAutofilledValue(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value) override {}

  void AskForValuesToFill(
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutofillSuggestionTriggerSource trigger_source) override {}

  void HidePopup() override {}

  void FocusNoLongerOnForm(bool had_interacted_form) override {
    did_unfocus_form_ = true;
    had_interacted_form_ = had_interacted_form;
  }

  void FocusOnFormField(const FormData& form,
                        const FormFieldData& field,
                        const gfx::RectF& bounding_box) override {}

  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override {}

  void DidPreviewAutofillFormData() override {}

  void DidEndTextFieldEditing() override {}

  void SelectFieldOptionsDidChange(const autofill::FormData& form) override {}

  // Records whether FocusNoLongerOnForm() get called.
  bool did_unfocus_form_{false};

  // Records value of |had_interacted_form| on last call to
  // FocusNoLongerOnForm(). Meaningless if |did_unfocus_form_| is false.
  bool had_interacted_form_{false};

  // Records the form data received via FormSubmitted() call.
  std::unique_ptr<FormData> form_submitted_;

  bool known_success_;

  SubmissionSource submission_source_;

  std::unique_ptr<FormFieldData> select_control_changed_;

  mojo::AssociatedReceiverSet<mojom::AutofillDriver> receivers_;
};

// Helper function to verify the form-related messages received from the
// renderer. The same data is expected in both messages. Depending on
// |expect_submitted_message|, will verify presence of FormSubmitted message.
void VerifyReceivedRendererMessages(
    const FakeContentAutofillDriver& fake_driver,
    const std::string& fname,
    const std::string& lname,
    bool expect_known_success,
    SubmissionSource expect_submission_source) {
  ASSERT_TRUE(fake_driver.form_submitted());

  // The tuple also includes a timestamp, which is ignored.
  const FormData& submitted_form = *(fake_driver.form_submitted());
  ASSERT_LE(2U, submitted_form.fields.size());
  EXPECT_EQ(u"fname", submitted_form.fields[0].name);
  EXPECT_EQ(base::UTF8ToUTF16(fname), submitted_form.fields[0].value);
  EXPECT_EQ(u"lname", submitted_form.fields[1].name);
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
  ASSERT_LE(1U, submitted_form.fields.size());
  EXPECT_EQ(u"address", submitted_form.fields[0].name);
  EXPECT_EQ(base::UTF8ToUTF16(address), submitted_form.fields[0].value);
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

FormData CreateAutofillFormData(blink::WebLocalFrame* main_frame) {
  FormData data;
  data.name = u"name";
  data.url = GURL("http://example.com/");
  data.action = GURL("http://example.com/blade.php");
  data.is_form_tag = true;  // Default value.

  WebDocument document = main_frame->GetDocument();
  WebFormControlElement fname_element =
      document.GetElementById(WebString::FromUTF8("fname"))
          .To<WebFormControlElement>();
  WebFormControlElement lname_element =
      document.GetElementById(WebString::FromUTF8("lname"))
          .To<WebFormControlElement>();

  FormFieldData field_data;
  field_data.name = u"fname";
  field_data.value = u"John";
  field_data.is_autofilled = true;
  field_data.unique_renderer_id =
      FieldRendererId(fname_element.UniqueRendererFormControlId());
  data.fields.push_back(field_data);

  field_data.name = u"lname";
  field_data.value = u"Smith";
  field_data.is_autofilled = true;
  if (!lname_element.IsNull()) {
    field_data.unique_renderer_id =
        FieldRendererId(lname_element.UniqueRendererFormControlId());
  }
  data.fields.push_back(field_data);

  return data;
}

void SimulateFillForm(const FormData& form_data,
                      autofill::AutofillAgent* autofill_agent,
                      blink::WebLocalFrame* main_frame) {
  WebDocument document = main_frame->GetDocument();
  WebFormControlElement fname_element =
      document.GetElementById(WebString::FromUTF8("fname"))
          .To<WebFormControlElement>();

  ASSERT_FALSE(fname_element.IsNull());
  // This call is necessary to setup the autofill agent appropriate for the
  // user selection; simulates the menu actually popping up.
  autofill_agent->FormControlElementClicked(
      fname_element.To<WebInputElement>());

  autofill_agent->FillOrPreviewForm(form_data,
                                    mojom::RendererFormDataAction::kFill);
}

// Simulates receiving a message from the browser to fill a form.
void SimulateFillForm(autofill::AutofillAgent* autofill_agent,
                      blink::WebLocalFrame* main_frame) {
  FormData data = CreateAutofillFormData(main_frame);
  SimulateFillForm(data, autofill_agent, main_frame);
}

// Simulates receiving a message from the browser to fill a form with an
// additional non-autofillable field.
void SimulateFillFormWithNonFillableFields(
    autofill::AutofillAgent* autofill_agent,
    blink::WebLocalFrame* main_frame) {
  WebDocument document = main_frame->GetDocument();
  WebFormControlElement fname_element =
      document.GetElementById(WebString::FromUTF8("fname"))
          .To<WebFormControlElement>();
  ASSERT_FALSE(fname_element.IsNull());
  WebFormControlElement mname_element =
      document.GetElementById(WebString::FromUTF8("mname"))
          .To<WebFormControlElement>();
  ASSERT_FALSE(mname_element.IsNull());
  WebFormControlElement lname_element =
      document.GetElementById(WebString::FromUTF8("lname"))
          .To<WebFormControlElement>();
  ASSERT_FALSE(lname_element.IsNull());

  FormData data;
  data.name = u"name";
  data.url = GURL("http://example.com/");
  data.action = GURL("http://example.com/blade.php");
  data.is_form_tag = true;  // Default value.

  FormFieldData field_data;
  field_data.name = u"fname";
  field_data.value = u"John";
  field_data.is_autofilled = true;
  field_data.unique_renderer_id =
      FieldRendererId(fname_element.UniqueRendererFormControlId());
  data.fields.push_back(field_data);

  field_data.name = u"lname";
  field_data.value = u"Smith";
  field_data.is_autofilled = true;
  field_data.unique_renderer_id =
      FieldRendererId(lname_element.UniqueRendererFormControlId());
  data.fields.push_back(field_data);

  // Additional non-autofillable field.
  field_data.name = u"mname";
  field_data.value = u"James";
  field_data.is_autofilled = false;
  field_data.unique_renderer_id =
      FieldRendererId(mname_element.UniqueRendererFormControlId());
  data.fields.push_back(field_data);

  // This call is necessary to setup the autofill agent appropriate for the
  // user selection; simulates the menu actually popping up.
  autofill_agent->FormControlElementClicked(
      fname_element.To<WebInputElement>());

  autofill_agent->FillOrPreviewForm(data, mojom::RendererFormDataAction::kFill);
}

}  // end namespace

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

  void SimulateUserInput(const blink::WebString& id, const std::string& value) {
    WebDocument document = GetMainFrame()->GetDocument();
    WebElement element = document.GetElementById(id);
    ASSERT_FALSE(element.IsNull());
    WebInputElement fname_element = element.To<WebInputElement>();
    SimulateUserInputChangeForElement(&fname_element, value);
  }

  std::string GetFocusLog() {
    return focus_test_utils_->GetFocusLog(GetMainFrame()->GetDocument());
  }

  FakeContentAutofillDriver fake_driver_;
  std::unique_ptr<test::FocusTestUtils> focus_test_utils_;
};

// Tests that submitting a form generates FormSubmitted message with the form
// fields.
TEST_F(FormAutocompleteTest, NormalFormSubmit) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='about:blank'>"
      "<input name='fname' value='Rick'/>"
      "<input name='lname' value='Deckard'/></form></html>");

  // Submit the form.
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 false /* expect_known_success */,
                                 SubmissionSource::FORM_SUBMISSION);
}

// Tests that FormSubmitted message is generated even the submit event isn't
// propagated by Javascript.
TEST_F(FormAutocompleteTest, SubmitEventPrevented) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'><input name='fname' value='Rick'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form>"
      "</html>");

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

// Tests that completing an Ajax request and having the form disappear will
// trigger submission from Autofill's point of view.
TEST_F(FormAutocompleteTest, AjaxSucceeded_NoLongerVisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

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
TEST_F(FormAutocompleteTest,
       AjaxSucceeded_NoLongerVisible_DifferentActionsSameData) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form>"
      "<form id='myForm2' action='http://example.com/runner.php'>"
      "<input name='fname' id='fname2' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

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
TEST_F(FormAutocompleteTest, MAYBE_NoLongerVisibleBothNoActions) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form>"
      "<form id='myForm2'>"
      "<input name='fname' id='fname2' value='John'/>"
      "<input name='lname' value='Doe'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

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
TEST_F(FormAutocompleteTest, AjaxSucceeded_NoLongerVisible_NoAction) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

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
TEST_F(FormAutocompleteTest, AjaxSucceeded_StillVisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  // No submission messages sent.
  VerifyNoSubmitMessagesReceived(fake_driver_);
}

// Tests that completing an Ajax request without any prior form interaction
// does not trigger form submission from Autofill's point of view.
TEST_F(FormAutocompleteTest, AjaxSucceeded_NoFormInteractionInvisible) {
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
TEST_F(FormAutocompleteTest, AjaxSucceeded_FilledFormIsInvisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname'/>"
      "<input name='lname'/></form></html>");

  // Simulate filling a form using Autofill.
  SimulateFillForm(autofill_agent_, GetMainFrame());

  // Simulate user input since ajax request doesn't fire submission message
  // if there is no user input.
  SimulateUserInput(WebString::FromUTF8("fname"), std::string("Rick"));

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
TEST_F(FormAutocompleteTest, AjaxSucceeded_FilledFormStillVisible) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Rick'/>"
      "<input name='lname' value='Deckard'/></form></html>");

  // Simulate filling a form using Autofill.
  SimulateFillForm(autofill_agent_, GetMainFrame());

  // Form still visible.

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  // No submission messages sent.
  VerifyNoSubmitMessagesReceived(fake_driver_);
}

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
  SimulateFillFormWithNonFillableFields(autofill_agent_, GetMainFrame());
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
  SimulateFillForm(autofill_agent_, GetMainFrame());
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
  SimulateFillForm(autofill_agent_, GetMainFrame());
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
  // everything works correctly even if unique_renderer_ids of the <input>
  // elements are not in ascending order.
  ExecuteJavaScriptForTests(
      "document.getElementById('fname').insertAdjacentHTML('beforebegin', "
      "'<label>Zip code:</label><input id=\"zip_code\"/>');");
  SimulateFillForm(data, autofill_agent_, GetMainFrame());
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
  SimulateFillForm(data, autofill_agent_, GetMainFrame());
  base::RunLoop().RunUntilIdle();

  // Expected Result in order:
  // * Change fname
  EXPECT_EQ(GetFocusLog(), "c0");
}

// Tests that completing an Ajax request without a form present will still
// trigger submission, if all the inputs the user has modified disappear.
TEST_F(FormAutocompleteTest, AjaxSucceeded_FormlessElements) {
  // Load a "form." Note that kRequiredFieldsForUpload fields are required
  // for the formless logic to trigger, so we add a throwaway third field.
  LoadHTML(
      "<head><title>Checkout</title></head>"
      "<input type='text' name='fname' id='fname'/>"
      "<input type='text' name='lname' value='Puckett'/>"
      "<input type='number' name='number' value='34'/>");

  // Simulate user input.
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Kirby"));

  // Remove element from view.
  ExecuteJavaScriptForTests(
      "var element = document.getElementById('fname');"
      "element.style.display = 'none';");

  // Simulate AJAX request.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Kirby", "Puckett",
                                 true /* expect_known_success */,
                                 SubmissionSource::XHR_SUCCEEDED);
}

// Unit test for CollectFormlessElements.
TEST_F(FormAutocompleteTest, CollectFormlessElements) {
  LoadHTML(
      "<html><title>Checkout</title></head>"
      "<input type='text' name='text_input'/>"
      "<input type='checkbox' name='check_input'/>"
      "<input type='number' name='number_input'/>"
      "<select name='select_input'/>"
      "  <option value='option_1'></option>"
      "  <option value='option_2'></option>"
      "</select>"
      "<form><input type='text' name='excluded'/></form>"
      "</html>");

  FormData result;
  autofill_agent_->CollectFormlessElements(&result);

  // Asserting size 4 also ensures that 'excluded' field inside <form> is not
  // collected.
  ASSERT_EQ(4U, result.fields.size());
  EXPECT_EQ(u"text_input", result.fields[0].name);
  EXPECT_EQ(u"check_input", result.fields[1].name);
  EXPECT_EQ(u"number_input", result.fields[2].name);
  EXPECT_EQ(u"select_input", result.fields[3].name);
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
    ASSERT_FALSE(element.IsNull());
    WebInputElement input_element = element.To<WebInputElement>();
    FieldRendererId field_id(input_element.UniqueRendererFormControlId());
    // Select this element in |autofill_agent_|.
    autofill_agent_->FormControlElementClicked(input_element);

    autofill_agent_->AcceptDataListSuggestion(field_id, kSuggestion);
    EXPECT_EQ(c.expected, input_element.Value().Utf8()) << "Case id: " << c.id;
  }
}

// Test that a FocusNoLongerOnForm message is sent if focus goes from a
// focused but uninteracted form to a null element.
TEST_F(FormAutocompleteTest,
       UninteractedFormFocusChangesToNull_FocusNoLongerOnForm) {
  // Load a form.
  LoadHTML(
      "<html><input type='text' id='different'/>"
      "<form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Change focus to the form.
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  SetFocused(element);

  ASSERT_FALSE(fake_driver_.did_unfocus_form());

  // Change focus to a null element.
  ChangeFocusToNull(document);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_driver_.did_unfocus_form());
  EXPECT_FALSE(fake_driver_.had_interacted_form());
}

// Test that a FocusNoLongerOnForm message is sent if focus goes from an
// interacted form to a null element.
TEST_F(FormAutocompleteTest,
       InteractedFormFocusChangesToNull_FocusNoLongerOnForm) {
  // Load a form.
  LoadHTML(
      "<html><input type='text' id='different'/>"
      "<form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  ASSERT_FALSE(fake_driver_.did_unfocus_form());

  // Change focus to a null element.
  ChangeFocusToNull(document);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_driver_.did_unfocus_form());
  EXPECT_TRUE(fake_driver_.had_interacted_form());
}

// Test that a FocusNoLongerOnForm message is sent if focus goes from an
// interacted form to an element outside the form.
TEST_F(FormAutocompleteTest,
       InteractedFormFocusChangesToExternalElement_FocusNoLongerOnForm) {
  // Load a form.
  LoadHTML(
      "<html><input type='text' id='different'/>"
      "<form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input so that the form is "remembered".
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  ASSERT_FALSE(fake_driver_.did_unfocus_form());

  // Change focus to a different node outside the form.
  WebElement different =
      document.GetElementById(WebString::FromUTF8("different"));
  SetFocused(different);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_driver_.did_unfocus_form());
  EXPECT_TRUE(fake_driver_.had_interacted_form());
}

// Test that a FocusNoLongerOnForm message is sent if focus goes from one
// interacted form to another.
TEST_F(FormAutocompleteTest, InteractingInDifferentForms_FocusNoLongerOnForm) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='http://example.com/blade.php'>"
      "<input name='fname' id='fname' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form>"
      "<form id='myForm2' action='http://example.com/runner.php'>"
      "<input name='fname' id='fname2' value='Bob'/>"
      "<input name='lname' value='Deckard'/><input type=submit></form></html>");

  // Simulate user input in the first form so that the form is "remembered".
  WebDocument document = GetMainFrame()->GetDocument();
  WebElement element = document.GetElementById(WebString::FromUTF8("fname"));
  ASSERT_FALSE(element.IsNull());
  WebInputElement fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("Rick"));

  ASSERT_FALSE(fake_driver_.did_unfocus_form());

  // Simulate user input in the second form so that a "no longer focused"
  // message is sent for the first form.
  document = GetMainFrame()->GetDocument();
  element = document.GetElementById(WebString::FromUTF8("fname2"));
  ASSERT_FALSE(element.IsNull());
  fname_element = element.To<WebInputElement>();
  SimulateUserInputChangeForElement(&fname_element, std::string("John"));

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_TRUE(fake_driver_.did_unfocus_form());
  EXPECT_TRUE(fake_driver_.had_interacted_form());
}

// Tests that submitting a form that has autocomplete="off" generates
// WillSubmitForm and FormSubmitted messages.
TEST_F(FormAutocompleteTest, AutoCompleteOffFormSubmit) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' autocomplete='off' action='about:blank'>"
      "<input name='fname' value='Rick'/>"
      "<input name='lname' value='Deckard'/>"
      "</form></html>");

  // Submit the form.
  ExecuteJavaScriptForTests("document.getElementById('myForm').submit();");
  base::RunLoop().RunUntilIdle();

  VerifyReceivedRendererMessages(fake_driver_, "Rick", "Deckard",
                                 false /* expect_known_success */,
                                 SubmissionSource::FORM_SUBMISSION);
}

// Tests that fields with autocomplete off are submitted.
TEST_F(FormAutocompleteTest, AutoCompleteOffInputSubmit) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm' action='about:blank'>"
      "<input name='fname' value='Rick'/>"
      "<input name='lname' value='Deckard' autocomplete='off'/>"
      "</form></html>");

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
TEST_F(FormAutocompleteTest, DynamicAutoCompleteOffFormSubmit) {
  LoadHTML(
      "<html><form id='myForm' action='about:blank'>"
      "<input name='fname' value='Rick'/>"
      "<input name='lname' value='Deckard'/></form></html>");

  WebElement element =
      GetMainFrame()->GetDocument().GetElementById(blink::WebString("myForm"));
  ASSERT_FALSE(element.IsNull());
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

TEST_F(FormAutocompleteTest, FormSubmittedByDOMMutationAfterXHR) {
  LoadHTML(
      "<html>"
      "<input type='text' id='address_field' name='address' autocomplete='on'>"
      "</html>");

  SimulateUserInput(WebString::FromUTF8("address_field"), std::string("City"));

  // Simulate an Ajax request completing.
  static_cast<blink::WebAutofillClient*>(autofill_agent_)->AjaxSucceeded();

  // Hide elements to simulate successful form submission.
  std::string hide_elements =
      "var address = document.getElementById('address_field');"
      "address.style = 'display:none';";

  ExecuteJavaScriptForTests(hide_elements.c_str());
  base::RunLoop().RunUntilIdle();

  VerifyReceivedAddressRendererMessages(
      fake_driver_, "City", true /* expect_known_success */,
      SubmissionSource::DOM_MUTATION_AFTER_XHR);
}

TEST_F(FormAutocompleteTest, FormSubmittedBySameDocumentNavigation) {
  LoadHTML(
      "<html>"
      "<input type='text' id='address_field' name='address' autocomplete='on'>"
      "</html>");

  SimulateUserInput(WebString::FromUTF8("address_field"), std::string("City"));

  // Hide elements to simulate successful form submission.
  std::string hide_elements =
      "var address = document.getElementById('address_field');"
      "address.style = 'display:none';";

  ExecuteJavaScriptForTests(hide_elements.c_str());

  // Simulate same document navigation.
  autofill_agent_->form_tracker_for_testing()
      ->DidFinishSameDocumentNavigation();
  base::RunLoop().RunUntilIdle();

  VerifyReceivedAddressRendererMessages(
      fake_driver_, "City", true /* expect_known_success */,
      SubmissionSource::SAME_DOCUMENT_NAVIGATION);
}

TEST_F(FormAutocompleteTest, FormSubmittedByProbablyFormSubmitted) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(
      features::kAutofillProbableFormSubmissionInBrowser);

  LoadHTML(
      "<html>"
      "<input type='text' id='address_field' name='address' autocomplete='on'>"
      "</html>");

  SimulateUserInput(WebString::FromUTF8("address_field"), std::string("City"));

  // Hide elements to simulate successful form submission.
  std::string hide_elements =
      "var address = document.getElementById('address_field');"
      "address.style = 'display:none';";

  ExecuteJavaScriptForTests(hide_elements.c_str());

  // Simulate navigation.
  autofill_agent_->form_tracker_for_testing()
      ->FireProbablyFormSubmittedForTesting();

  base::RunLoop().RunUntilIdle();

  VerifyReceivedAddressRendererMessages(
      fake_driver_, "City", false /* expect_known_success */,
      SubmissionSource::PROBABLY_FORM_SUBMITTED);
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

  const FormFieldData* field = fake_driver_.select_control_changed();
  ASSERT_TRUE(field);
  EXPECT_EQ(u"color", field->name);
  EXPECT_EQ(u"blue", field->value);
}

}  // namespace autofill
