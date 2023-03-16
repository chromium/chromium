// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

using base::ASCIIToUTF16;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebInputElement;
using blink::WebString;
using blink::WebURLRequest;
using blink::WebVector;

namespace autofill {

namespace {

class FakeContentAutofillDriver : public mojom::AutofillDriver {
 public:
  FakeContentAutofillDriver() : called_field_change_(false) {}
  ~FakeContentAutofillDriver() override {}

  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutofillDriver> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  bool called_field_change() const { return called_field_change_; }

  const std::vector<FormData>* forms() const { return forms_.get(); }

  void reset_forms() { return forms_.reset(); }

 private:
  // mojom::AutofillDriver:
  void SetFormToBeProbablySubmitted(
      const absl::optional<FormData>& form) override {}

  void FormsSeen(const std::vector<FormData>& updated_forms,
                 const std::vector<FormRendererId>& removed_forms) override {
    // FormsSeen() could be called multiple times and sometimes even with empty
    // forms array for main frame, but we're interested in only the first time
    // call.
    if (!forms_)
      forms_ = std::make_unique<std::vector<FormData>>(updated_forms);
  }

  void FormSubmitted(const FormData& form,
                     bool known_success,
                     mojom::SubmissionSource source) override {}

  void TextFieldDidChange(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          base::TimeTicks timestamp) override {
    called_field_change_ = true;
  }

  void TextFieldDidScroll(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box) override {}

  void SelectControlDidChange(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override {}

  void JavaScriptChangedAutofilledValue(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value) override {}

  void AskForValuesToFill(
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutoselectFirstSuggestion autoselect_first_suggestion,
      FormElementWasClicked form_element_was_clicked) override {}

  void HidePopup() override {}

  void FocusNoLongerOnForm(bool had_interacted_form) override {}

  void FocusOnFormField(const FormData& form,
                        const FormFieldData& field,
                        const gfx::RectF& bounding_box) override {}

  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override {}

  void DidPreviewAutofillFormData() override {}

  void DidEndTextFieldEditing() override {}

  void SelectFieldOptionsDidChange(const autofill::FormData& form) override {}

  // Records whether TextFieldDidChange() get called.
  bool called_field_change_;
  // Records data received via FormSeen() call.
  std::unique_ptr<std::vector<FormData>> forms_;

  mojo::AssociatedReceiverSet<mojom::AutofillDriver> receivers_;
};

}  // namespace

using AutofillQueryParam =
    std::tuple<int, autofill::FormData, autofill::FormFieldData, gfx::RectF>;

class AutofillRendererTest : public ChromeRenderViewTest {
 public:
  AutofillRendererTest() = default;
  AutofillRendererTest(const AutofillRendererTest&) = delete;
  AutofillRendererTest& operator=(const AutofillRendererTest&) = delete;
  ~AutofillRendererTest() override = default;

 protected:
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // We only use the fake driver for main frame
    // because our test cases only involve the main frame.
    blink::AssociatedInterfaceProvider* remote_interfaces =
        GetMainRenderFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::AutofillDriver::Name_,
        base::BindRepeating(&AutofillRendererTest::BindAutofillDriver,
                            base::Unretained(this)));
  }

  void BindAutofillDriver(mojo::ScopedInterfaceEndpointHandle handle) {
    fake_driver_.BindReceiver(
        mojo::PendingAssociatedReceiver<mojom::AutofillDriver>(
            std::move(handle)));
  }

  FakeContentAutofillDriver fake_driver_;
};

TEST_F(AutofillRendererTest, SendForms) {
  LoadHTML("<form method='POST'>"
           "  <input type='text' id='firstname'/>"
           "  <input type='text' id='middlename'/>"
           "  <input type='text' id='lastname' autoComplete='off'/>"
           "  <input type='hidden' id='email'/>"
           "  <select id='state'/>"
           "    <option>?</option>"
           "    <option>California</option>"
           "    <option>Texas</option>"
           "  </select>"
           "</form>");

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  // Verify that "FormsSeen" sends the expected number of fields.
  ASSERT_TRUE(fake_driver_.forms());
  std::vector<FormData> forms = *(fake_driver_.forms());
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(4UL, forms[0].fields.size());

  FormFieldData expected;

  expected.id_attribute = u"firstname";
  expected.name = expected.id_attribute;
  expected.value = std::u16string();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::DefaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[0]);

  expected.id_attribute = u"middlename";
  expected.name = expected.id_attribute;
  expected.value = std::u16string();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::DefaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[1]);

  expected.id_attribute = u"lastname";
  expected.name = expected.id_attribute;
  expected.value = std::u16string();
  expected.form_control_type = "text";
  expected.autocomplete_attribute = "off";
  expected.max_length = WebInputElement::DefaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[2]);
  expected.autocomplete_attribute = std::string();  // reset

  expected.id_attribute = u"state";
  expected.name = expected.id_attribute;
  expected.value = u"?";
  expected.form_control_type = "select-one";
  expected.max_length = 0;
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[3]);

  fake_driver_.reset_forms();

  // Dynamically create a new form. A new message should be sent for it, but
  // not for the previous form.
  ExecuteJavaScriptForTests(
      "var newForm=document.createElement('form');"
      "newForm.id='new_testform';"
      "newForm.action='http://google.com';"
      "newForm.method='post';"
      "var newFirstname=document.createElement('input');"
      "newFirstname.setAttribute('type', 'text');"
      "newFirstname.setAttribute('id', 'second_firstname');"
      "newFirstname.value = 'Bob';"
      "var newLastname=document.createElement('input');"
      "newLastname.setAttribute('type', 'text');"
      "newLastname.setAttribute('id', 'second_lastname');"
      "newLastname.value = 'Hope';"
      "var newEmail=document.createElement('input');"
      "newEmail.setAttribute('type', 'text');"
      "newEmail.setAttribute('id', 'second_email');"
      "newEmail.value = 'bobhope@example.com';"
      "newForm.appendChild(newFirstname);"
      "newForm.appendChild(newLastname);"
      "newForm.appendChild(newEmail);"
      "document.body.appendChild(newForm);");

  WaitForAutofillDidAddOrRemoveFormRelatedElements();
  ASSERT_TRUE(fake_driver_.forms());
  forms = *(fake_driver_.forms());
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(3UL, forms[0].fields.size());

  expected.form_control_type = "text";
  expected.max_length = WebInputElement::DefaultMaxLength();

  expected.id_attribute = u"second_firstname";
  expected.name = expected.id_attribute;
  expected.value = u"Bob";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[0]);

  expected.id_attribute = u"second_lastname";
  expected.name = expected.id_attribute;
  expected.value = u"Hope";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[1]);

  expected.id_attribute = u"second_email";
  expected.name = expected.id_attribute;
  expected.value = u"bobhope@example.com";
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[2]);
}

// Regression test for [ http://crbug.com/346010 ].
TEST_F(AutofillRendererTest, DontCrashWhileAssociatingForms) {
  LoadHTML("<form id='form'>"
           "<foo id='foo'>"
           "<script id='script'>"
           "document.documentElement.appendChild(foo);"
           "newDoc = document.implementation.createDocument("
           "    'http://www.w3.org/1999/xhtml', 'html');"
           "foo.insertBefore(form, script);"
           "newDoc.adoptNode(foo);"
           "</script>");

  // Shouldn't crash.
}

TEST_F(AutofillRendererTest, DynamicallyAddedUnownedFormElements) {
  std::string html_data;
  base::FilePath test_path = ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("autofill")),
      base::FilePath(FILE_PATH_LITERAL("autofill_noform_dynamic.html")));
  ASSERT_TRUE(base::ReadFileToString(test_path, &html_data));
  LoadHTML(html_data.c_str());

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  // Verify that "FormsSeen" sends the expected number of fields.
  ASSERT_TRUE(fake_driver_.forms());
  std::vector<FormData> forms = *(fake_driver_.forms());
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(7UL, forms[0].fields.size());

  fake_driver_.reset_forms();

  ExecuteJavaScriptForTests("AddFields()");

  WaitForAutofillDidAddOrRemoveFormRelatedElements();
  ASSERT_TRUE(fake_driver_.forms());
  forms = *(fake_driver_.forms());
  ASSERT_EQ(1UL, forms.size());
  ASSERT_EQ(9UL, forms[0].fields.size());

  FormFieldData expected;

  expected.id_attribute = u"EMAIL_ADDRESS";
  expected.name = expected.id_attribute;
  expected.value.clear();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::DefaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[7]);

  expected.id_attribute = u"PHONE_HOME_WHOLE_NUMBER";
  expected.name = expected.id_attribute;
  expected.value.clear();
  expected.form_control_type = "text";
  expected.max_length = WebInputElement::DefaultMaxLength();
  EXPECT_FORM_FIELD_DATA_EQUALS(expected, forms[0].fields[8]);
}

TEST_F(AutofillRendererTest, IgnoreNonUserGestureTextFieldChanges) {
  LoadHTML("<form method='post'>"
           "  <input type='text' id='full_name'/>"
           "</form>");

  blink::WebInputElement full_name = GetMainFrame()
                                         ->GetDocument()
                                         .GetElementById("full_name")
                                         .To<blink::WebInputElement>();
  while (!full_name.Focused())
    GetMainFrame()->View()->AdvanceFocus(false);

  ASSERT_FALSE(fake_driver_.called_field_change());
  full_name.SetValue("Alice", true);
  GetMainFrame()->AutofillClient()->TextFieldDidChange(full_name);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(fake_driver_.called_field_change());

  SimulateUserInputChangeForElement(&full_name, "Alice");
  ASSERT_TRUE(fake_driver_.called_field_change());
}

}  // namespace autofill
