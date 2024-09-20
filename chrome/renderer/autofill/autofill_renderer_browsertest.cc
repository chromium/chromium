// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_view.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::SizeIs;
using ::testing::Truly;

namespace autofill {

namespace {

auto FormField(
    const std::string& name,
    const std::string& value = "",
    FormControlType form_control_type = FormControlType::kInputText) {
  return AllOf(
      Property("name", &FormFieldData::name, base::UTF8ToUTF16(name)),
      Property("value", &FormFieldData::value, base::UTF8ToUTF16(value)),
      Property("form_control_type", &FormFieldData::form_control_type,
               form_control_type));
}

auto FormWithFields(auto matcher) {
  return ElementsAre(Property(&FormData::fields, matcher));
}

auto Nth(int index, auto matcher) {
  return ResultOf(
      base::StringPrintf("array[%d]", index),
      [index](auto& container) { return container[index]; }, matcher);
}

using AutofillRendererTest = test::AutofillRendererTest;

TEST_F(AutofillRendererTest, SendForms) {
  EXPECT_CALL(
      autofill_driver(),
      FormsSeen(FormWithFields(ElementsAre(
                    FormField("firstname"), FormField("middlename"),
                    FormField("lastname"),
                    FormField("state", "?", FormControlType::kSelectOne))),
                IsEmpty()));
  LoadHTML(R"(<form method='POST'>
                <input type='text' id='firstname'/>
                <input type='text' id='middlename'/>
                <input type='text' id='lastname' autoComplete='off'/>
                <input type='hidden' id='email'/>
                <select id='state'/>
                  <option>?</option>
                  <option>California</option>
                  <option>Texas</option>
                </select>
              </form>)");
  WaitForFormsSeen();

  // Dynamically create a new form. A new message should be sent for it, but
  // not for the previous form.
  EXPECT_CALL(autofill_driver(),
              FormsSeen(FormWithFields(ElementsAre(
                            FormField("second_firstname", "Bob"),
                            FormField("second_lastname", "Hope"),
                            FormField("second_email", "bobhope@example.com"))),
                        IsEmpty()));
  ExecuteJavaScriptForTests(
      R"(var newForm=document.createElement('form');
         newForm.id='new_testform';
         newForm.action='http://google.com';
         newForm.method='post';
         var newFirstname=document.createElement('input');
         newFirstname.setAttribute('type', 'text');
         newFirstname.setAttribute('id', 'second_firstname');
         newFirstname.value = 'Bob';
         var newLastname=document.createElement('input');
         newLastname.setAttribute('type', 'text');
         newLastname.setAttribute('id', 'second_lastname');
         newLastname.value = 'Hope';
         var newEmail=document.createElement('input');
         newEmail.setAttribute('type', 'text');
         newEmail.setAttribute('id', 'second_email');
         newEmail.value = 'bobhope@example.com';
         newForm.appendChild(newFirstname);
         newForm.appendChild(newLastname);
         newForm.appendChild(newEmail);
         document.body.appendChild(newForm);)");
  WaitForFormsSeen();
}

// Regression test for [ http://crbug.com/346010 ].
// Shouldn't crash.
TEST_F(AutofillRendererTest, DontCrashWhileAssociatingForms) {
  EXPECT_CALL(autofill_driver(), FormsSeen(_, _)).Times(0);
  LoadHTML(R"(<form id='form'>
              <foo id='foo'>
              <script id='script'>
              document.documentElement.appendChild(foo);
              newDoc = document.implementation.createDocument(
                  'http://www.w3.org/1999/xhtml', 'html');
              foo.insertBefore(form, script);
              newDoc.adoptNode(foo);
              </script>)");
  WaitForFormsSeen();
}

TEST_F(AutofillRendererTest, DynamicallyAddedUnownedFormElements) {
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
  base::FilePath test_path = test_data_dir.AppendASCII(
      "chrome/test/data/autofill/autofill_noform_dynamic.html");
  std::string html_data;
  ASSERT_TRUE(base::ReadFileToString(test_path, &html_data));

  EXPECT_CALL(autofill_driver(),
              FormsSeen(FormWithFields(SizeIs(7)), IsEmpty()));
  LoadHTML(html_data.c_str());
  WaitForFormsSeen();

  EXPECT_CALL(autofill_driver(),
              FormsSeen(FormWithFields(AllOf(
                            SizeIs(9), Nth(7, FormField("EMAIL_ADDRESS")),
                            Nth(8, FormField("PHONE_HOME_WHOLE_NUMBER")))),
                        IsEmpty()));
  ExecuteJavaScriptForTests("AddFields()");
  WaitForFormsSeen();
}

TEST_F(AutofillRendererTest, IgnoreNonUserGestureTextFieldChanges) {
  EXPECT_CALL(autofill_driver(), TextFieldDidChange).Times(0);
  LoadHTML(R"(<form method='post'>
                <input type='text' id='full_name'/>
              </form>)");
  WaitForFormsSeen();

  blink::WebInputElement full_name = GetMainFrame()
                                         ->GetDocument()
                                         .GetElementById("full_name")
                                         .To<blink::WebInputElement>();
  while (!full_name.Focused())
    GetMainFrame()->View()->AdvanceFocus(false);

  full_name.SetValue("Alice", true);
  GetMainFrame()->AutofillClient()->TextFieldDidChange(full_name);

  EXPECT_CALL(autofill_driver(), TextFieldDidChange);
  SimulateUserInputChangeForElement(full_name, "Alice");
}

}  // namespace

}  // namespace autofill
