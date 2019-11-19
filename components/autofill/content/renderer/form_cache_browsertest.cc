// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_cache.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_select_element.h"

using base::ASCIIToUTF16;
using blink::WebDocument;
using blink::WebElement;
using blink::WebInputElement;
using blink::WebSelectElement;
using blink::WebString;

namespace autofill {

const FormData* GetFormByName(const std::vector<FormData>& forms,
                              base::StringPiece name) {
  for (const FormData& form : forms) {
    if (form.name == ASCIIToUTF16(name))
      return &form;
  }
  return nullptr;
}

class FormCacheBrowserTest : public content::RenderViewTest {
 public:
  FormCacheBrowserTest() = default;
  ~FormCacheBrowserTest() override = default;
};

TEST_F(FormCacheBrowserTest, ExtractForms) {
  LoadHTML(R"(
    <form id="form1">
      <input type="text" name="foo1">
      <input type="text" name="foo2">
      <input type="text" name="foo3">
    </form>
    <input type="text" name="unowned_element">
  )");

  FormCache form_cache(GetMainFrame());
  std::vector<FormData> forms = form_cache.ExtractNewForms();

  const FormData* form1 = GetFormByName(forms, "form1");
  ASSERT_TRUE(form1);
  EXPECT_EQ(3u, form1->fields.size());

  const FormData* unowned_form = GetFormByName(forms, "");
  ASSERT_TRUE(unowned_form);
  EXPECT_EQ(1u, unowned_form->fields.size());
}

TEST_F(FormCacheBrowserTest, ExtractFormsTwice) {
  LoadHTML(R"(
    <form id="form1">
      <input type="text" name="foo1">
      <input type="text" name="foo2">
      <input type="text" name="foo3">
    </form>
    <input type="text" name="unowned_element">
  )");

  FormCache form_cache(GetMainFrame());
  std::vector<FormData> forms = form_cache.ExtractNewForms();

  forms = form_cache.ExtractNewForms();
  // As nothing has changed, there are no new forms and |forms| should be empty.
  EXPECT_TRUE(forms.empty());
}

TEST_F(FormCacheBrowserTest, ExtractFormsAfterModification) {
  LoadHTML(R"(
    <form id="form1">
      <input type="text" name="foo1">
      <input type="text" name="foo2">
      <input type="text" name="foo3">
    </form>
    <input type="text" name="unowned_element">
  )");

  FormCache form_cache(GetMainFrame());
  std::vector<FormData> forms = form_cache.ExtractNewForms();

  // Append an input element to the form and to the list of unowned inputs.
  ExecuteJavaScriptForTests(R"(
    var new_input_1 = document.createElement("input");
    new_input_1.setAttribute("type", "text");
    new_input_1.setAttribute("name", "foo4");

    var form1 = document.getElementById("form1");
    form1.appendChild(new_input_1);

    var new_input_2 = document.createElement("input");
    new_input_2.setAttribute("type", "text");
    new_input_2.setAttribute("name", "unowned_element_2");
    document.body.appendChild(new_input_2);
  )");

  forms = form_cache.ExtractNewForms();

  const FormData* form1 = GetFormByName(forms, "form1");
  ASSERT_TRUE(form1);
  EXPECT_EQ(4u, form1->fields.size());

  const FormData* unowned_form = GetFormByName(forms, "");
  ASSERT_TRUE(unowned_form);
  EXPECT_EQ(2u, unowned_form->fields.size());
}

TEST_F(FormCacheBrowserTest, FillAndClear) {
  LoadHTML(R"(
    <input type="text" name="text" id="text">
    <input type="checkbox" checked name="checkbox" id="checkbox">
    <select name="select" id="select">
      <option value="first">first</option>
      <option value="second" selected>second</option>
    </select>
  )");

  FormCache form_cache(GetMainFrame());
  std::vector<FormData> forms = form_cache.ExtractNewForms();

  ASSERT_EQ(1u, forms.size());
  FormData values_to_fill = forms[0];
  values_to_fill.fields[0].value = ASCIIToUTF16("test");
  values_to_fill.fields[0].is_autofilled = true;
  values_to_fill.fields[1].check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  values_to_fill.fields[1].is_autofilled = true;
  values_to_fill.fields[2].value = ASCIIToUTF16("first");
  values_to_fill.fields[2].is_autofilled = true;

  WebDocument doc = GetMainFrame()->GetDocument();
  auto text = doc.GetElementById("text").To<WebInputElement>();
  auto checkbox = doc.GetElementById("checkbox").To<WebInputElement>();
  auto select_element = doc.GetElementById("select").To<WebSelectElement>();

  form_util::FillForm(values_to_fill, text);

  EXPECT_EQ("test", text.Value().Ascii());
  EXPECT_FALSE(checkbox.IsChecked());
  EXPECT_EQ("first", select_element.Value().Ascii());

  // Validate that clearing works, in particular that the previous values
  // were saved correctly.
  form_cache.ClearSectionWithElement(text);

  EXPECT_EQ("", text.Value().Ascii());
  EXPECT_TRUE(checkbox.IsChecked());
  EXPECT_EQ("second", select_element.Value().Ascii());
}

TEST_F(FormCacheBrowserTest, FreeDataOnElementRemoval) {
  LoadHTML(R"(
    <div id="container">
      <input type="text" name="text" id="text">
      <input type="checkbox" checked name="checkbox" id="checkbox">
      <select name="select" id="select">
        <option value="first">first</option>
        <option value="second" selected>second</option>
      </select>
    </div>
  )");

  FormCache form_cache(GetMainFrame());
  form_cache.ExtractNewForms();

  EXPECT_EQ(1u, form_cache.initial_select_values_.size());
  EXPECT_EQ(1u, form_cache.initial_checked_state_.size());

  ExecuteJavaScriptForTests(R"(
    const container = document.getElementById('container');
    while (container.childElementCount > 0) {
      container.removeChild(container.children.item(0));
    }
  )");

  std::vector<FormData> forms = form_cache.ExtractNewForms();
  EXPECT_EQ(0u, forms.size());
  EXPECT_EQ(0u, form_cache.initial_select_values_.size());
  EXPECT_EQ(0u, form_cache.initial_checked_state_.size());
}

}  // namespace autofill
