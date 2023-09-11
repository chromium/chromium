// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/focus_test_utils.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "components/autofill/content/renderer/form_cache_test_api.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_select_element.h"

using base::ASCIIToUTF16;
using blink::WebDocument;
using blink::WebElement;
using blink::WebInputElement;
using blink::WebSelectElement;
using blink::WebString;
using testing::AllOf;
using testing::ElementsAre;
using testing::Field;
using testing::UnorderedElementsAre;

namespace autofill {
using CheckStatus = FormFieldData::CheckStatus;

auto HasId(FormRendererId expected_id) {
  return Field("unique_renderer_id", &FormData::unique_renderer_id,
               expected_id);
}

auto HasName(base::StringPiece expected_name) {
  return Field("name", &FormData::name, base::ASCIIToUTF16(expected_name));
}

auto IsToken(FrameToken expected_token, int expected_predecessor) {
  return AllOf(
      Field(&FrameTokenWithPredecessor::token, expected_token),
      Field(&FrameTokenWithPredecessor::predecessor, expected_predecessor));
}

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
  FormCacheBrowserTest() {
    focus_test_utils_ = std::make_unique<test::FocusTestUtils>(
        base::BindRepeating(&FormCacheBrowserTest::ExecuteJavaScriptForTests,
                            base::Unretained(this)));
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableSelectList);
  }
  ~FormCacheBrowserTest() override = default;
  FormCacheBrowserTest(const FormCacheBrowserTest&) = delete;
  FormCacheBrowserTest& operator=(const FormCacheBrowserTest&) = delete;

 protected:
  std::string GetFocusLog() {
    return focus_test_utils_->GetFocusLog(GetMainFrame()->GetDocument());
  }

  std::unique_ptr<test::FocusTestUtils> focus_test_utils_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FormCacheBrowserTest, UpdatedForms) {
  LoadHTML(R"(
    <form id="form1">
      <input type="text" name="foo1">
      <input type="text" name="foo2">
      <input type="text" name="foo3">
    </form>
    <input type="text" name="unowned_element">
  )");

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms = form_cache.UpdateFormCache(nullptr);

  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

  const FormData* form1 = GetFormByName(forms.updated_forms, "form1");
  ASSERT_TRUE(form1);
  EXPECT_EQ(3u, form1->fields.size());
  EXPECT_TRUE(form1->child_frames.empty());

  const FormData* unowned_form = GetFormByName(forms.updated_forms, "");
  ASSERT_TRUE(unowned_form);
  EXPECT_EQ(1u, unowned_form->fields.size());
  EXPECT_TRUE(unowned_form->child_frames.empty());
}

TEST_F(FormCacheBrowserTest, RemovedForms) {
  LoadHTML(R"(
    <form id="form1">
      <input type="text" name="foo1">
      <input type="text" name="foo2">
      <input type="text" name="foo3">
    </form>
    <form id="form2">
      <input type="text" name="foo1">
      <input type="text" name="foo2">
      <input type="text" name="foo3">
    </form>
    <input type="text" id="unowned_element">
  )");

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms = form_cache.UpdateFormCache(nullptr);

  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1"),
                                   HasName("form2")));
  EXPECT_TRUE(forms.removed_forms.empty());

  std::vector<FormRendererId> forms_renderer_id;
  for (const FormData& form : forms.updated_forms) {
    if (form.unique_renderer_id != FormRendererId()) {
      forms_renderer_id.push_back(form.unique_renderer_id);
    }
  }
  ASSERT_EQ(forms_renderer_id.size(), 2u);

  ExecuteJavaScriptForTests(R"(
    document.getElementById("form1").remove();
    document.getElementById("form2").innerHTML = "";
  )");

  forms = form_cache.UpdateFormCache(nullptr);

  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_THAT(forms.removed_forms,
              UnorderedElementsAre(forms_renderer_id[0], forms_renderer_id[1]));

  ExecuteJavaScriptForTests(R"(
    document.getElementById("unowned_element").remove();
  )");

  forms = form_cache.UpdateFormCache(nullptr);

  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_THAT(forms.removed_forms, ElementsAre(FormRendererId()));

  ExecuteJavaScriptForTests(R"(
    document.getElementById("form2").innerHTML = `
      <input type="text" name="foo1">
      <input type="text" name="foo2">
      <input type="text" name="foo3">
    `;
  )");

  forms = form_cache.UpdateFormCache(nullptr);

  EXPECT_THAT(forms.updated_forms, ElementsAre(HasName("form2")));
  EXPECT_TRUE(forms.removed_forms.empty());

  ExecuteJavaScriptForTests(R"(
    document.getElementById("form2").innerHTML = `
      <input type="text" name="foo1">
      <input type="text" name="foo2">
      <input type="text" name="foo3">
      <input type="text" name="foo4">
    `;
  )");

  forms = form_cache.UpdateFormCache(nullptr);

  EXPECT_THAT(forms.updated_forms, ElementsAre(HasName("form2")));
  EXPECT_TRUE(forms.removed_forms.empty());
}

// Test if the form gets re-extracted after a label change.
TEST_F(FormCacheBrowserTest, ExtractFormAfterDynamicFieldChange) {
  LoadHTML(R"(
    <form id="f"><input></form>
    <form id="g"> <label id="label">Name</label><input></form>
  )");

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);
  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasName("f"), HasName("g")));
  EXPECT_TRUE(forms.removed_forms.empty());

  ExecuteJavaScriptForTests(R"(
    document.getElementById("label").innerHTML = "Last Name";
  )");

  forms = form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);
  EXPECT_THAT(forms.updated_forms, ElementsAre(HasName("g")));
  EXPECT_TRUE(forms.removed_forms.empty());
}

TEST_F(FormCacheBrowserTest, ExtractFrames) {
  LoadHTML(R"(
    <form id="form1">
      <iframe id="frame1"></iframe>
    </form>
    <iframe id="frame2"></iframe>
  )");

  FrameToken frame1_token =
      GetFrameToken(GetMainFrame()->GetDocument(), "frame1");
  FrameToken frame2_token =
      GetFrameToken(GetMainFrame()->GetDocument(), "frame2");

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);

  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

  const FormData* form1 = GetFormByName(forms.updated_forms, "form1");
  ASSERT_TRUE(form1);
  EXPECT_TRUE(form1->fields.empty());
  EXPECT_THAT(form1->child_frames, ElementsAre(IsToken(frame1_token, -1)));

  const FormData* unowned_form = GetFormByName(forms.updated_forms, "");
  ASSERT_TRUE(unowned_form);
  EXPECT_TRUE(unowned_form->fields.empty());
  EXPECT_THAT(unowned_form->child_frames,
              ElementsAre(AllOf(IsToken(frame2_token, -1))));
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
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);

  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

  forms = form_cache.UpdateFormCache(nullptr);
  // As nothing has changed, there are no new or removed forms.
  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_TRUE(forms.removed_forms.empty());
}

TEST_F(FormCacheBrowserTest, ExtractFramesTwice) {
  LoadHTML(R"(
    <form id="form1">
      <iframe></iframe>
    </form>
    <iframe></iframe>
  )");

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);

  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

  forms = form_cache.UpdateFormCache(nullptr);
  // As nothing has changed, there are no new or removed forms.
  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_TRUE(forms.removed_forms.empty());
}

// TODO(crbug.com/1117028) Adjust expectations when we omit invisible iframes.
TEST_F(FormCacheBrowserTest, ExtractFramesAfterVisibilityChange) {
  LoadHTML(R"(
    <form id="form1">
      <iframe id="frame1" style="display: none;"></iframe>
      <iframe id="frame2" style="display: none;"></iframe>
    </form>
    <iframe id="frame3" style="display: none;"></iframe>
  )");

  WebElement iframe1 = GetElementById(GetMainFrame()->GetDocument(), "frame1");
  WebElement iframe2 = GetElementById(GetMainFrame()->GetDocument(), "frame2");
  WebElement iframe3 = GetElementById(GetMainFrame()->GetDocument(), "frame3");

  auto GetSize = [](const WebElement& element) {
    gfx::Rect bounds = element.BoundsInWidget();
    return bounds.width() * bounds.height();
  };

  ASSERT_LE(GetSize(iframe1), 0);
  ASSERT_LE(GetSize(iframe2), 0);
  ASSERT_LE(GetSize(iframe3), 0);

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);
  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

  iframe1.SetAttribute("style", "display: block;");
  iframe2.SetAttribute("style", "display: block;");
  iframe3.SetAttribute("style", "display: block;");

  ASSERT_GT(GetSize(iframe1), 0);
  ASSERT_GT(GetSize(iframe2), 0);
  ASSERT_GT(GetSize(iframe3), 0);

  forms = form_cache.UpdateFormCache(nullptr);
  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_TRUE(forms.removed_forms.empty());

  iframe2.SetAttribute("style", "display: none;");
  iframe3.SetAttribute("style", "display: none;");

  ASSERT_GT(GetSize(iframe1), 0);
  ASSERT_LE(GetSize(iframe2), 0);
  ASSERT_LE(GetSize(iframe3), 0);

  forms = form_cache.UpdateFormCache(nullptr);
  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_TRUE(forms.removed_forms.empty());
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
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);
  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

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

  forms = form_cache.UpdateFormCache(nullptr);
  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

  const FormData* form1 = GetFormByName(forms.updated_forms, "form1");
  ASSERT_TRUE(form1);
  EXPECT_EQ(4u, form1->fields.size());

  const FormData* unowned_form = GetFormByName(forms.updated_forms, "");
  ASSERT_TRUE(unowned_form);
  EXPECT_EQ(2u, unowned_form->fields.size());
}

struct FillElementData {
  blink::WebFormControlElement& element;
  std::u16string value;
};

FormFieldData* FindFieldByName(FormData& form_data,
                               blink::WebString search_field_name) {
  auto it = base::ranges::find(form_data.fields, search_field_name.Utf16(),
                               &FormFieldData::name);
  return it != form_data.fields.end() ? &*it : nullptr;
}

// Fills the fields referenced in `form_fill_data`. Fills `checkbox_element`, if
// non-null. `autofill_initiating_element` is the element which initiates the
// autofill.
void FillAndCheckState(
    const FormData& form_data,
    const blink::WebFormControlElement& autofill_initiating_element,
    const std::vector<FillElementData>& form_to_fill,
    absl::optional<blink::WebInputElement> checkbox_element = absl::nullopt,
    CheckStatus fill_checkbox_check_status =
        CheckStatus::kCheckableButUnchecked) {
  FormData values_to_fill = form_data;
  for (const FillElementData& field_to_fill : form_to_fill) {
    FormFieldData* value_to_fill = FindFieldByName(
        values_to_fill, field_to_fill.element.NameForAutofill());
    ASSERT_TRUE(value_to_fill != nullptr);
    value_to_fill->value = field_to_fill.value;
    value_to_fill->is_autofilled = true;
  }

  if (checkbox_element) {
    FormFieldData* value_to_fill =
        FindFieldByName(values_to_fill, checkbox_element->NameForAutofill());
    ASSERT_TRUE(value_to_fill != nullptr);
    value_to_fill->check_status = fill_checkbox_check_status;
    value_to_fill->is_autofilled = true;
  }

  form_util::ApplyAutofillAction(values_to_fill, autofill_initiating_element,
                                 mojom::AutofillActionType::kFill,
                                 mojom::AutofillActionPersistence::kFill);

  for (const FillElementData& field_to_fill : form_to_fill) {
    EXPECT_EQ(field_to_fill.value, field_to_fill.element.Value().Utf16());
  }

  if (checkbox_element) {
    bool expect_checked = (fill_checkbox_check_status == CheckStatus::kChecked);
    EXPECT_EQ(expect_checked, checkbox_element->IsChecked());
  }
}

TEST_F(FormCacheBrowserTest, FillAndClear) {
  // TODO(crbug.com/1422114): Make test work without explicit <selectlist>
  // tabindex.
  LoadHTML(R"(
    <input type="text" name="text" id="text">
    <input type="checkbox" checked name="checkbox" id="checkbox">
    <select name="select" id="select">
      <option value="first">first</option>
      <option value="second" selected>second</option>
    </select>
    <selectlist name="selectlist" id="selectlist" tabindex=0>
      <option value="uno">uno</option>
      <option value="dos" selected>dos</option>
    </selectlist>
  )");

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);

  EXPECT_THAT(forms.updated_forms, ElementsAre(HasId(FormRendererId())));
  EXPECT_TRUE(forms.removed_forms.empty());

  WebDocument doc = GetMainFrame()->GetDocument();
  auto text = GetFormControlElementById(doc, "text");
  auto checkbox = GetElementById(doc, "checkbox").To<WebInputElement>();
  auto select_element = GetFormControlElementById(doc, "select");
  auto selectlist_element = GetFormControlElementById(doc, "selectlist");

  FillAndCheckState(forms.updated_forms[0], text,
                    {{text, u"test"},
                     {select_element, u"first"},
                     {selectlist_element, u"uno"}},
                    checkbox, CheckStatus::kCheckableButUnchecked);

  // Validate that clearing works, in particular that the previous values
  // were saved correctly.
  form_cache.ClearSectionWithElement(text);

  EXPECT_EQ("", text.Value().Ascii());
  EXPECT_TRUE(checkbox.IsChecked());
  EXPECT_EQ("second", select_element.Value().Ascii());
  EXPECT_EQ("dos", selectlist_element.Value().Ascii());
}

// Tests that correct focus, change and blur events are emitted during the
// autofilling and clearing of the form with an initially focused element.
TEST_F(FormCacheBrowserTest,
       VerifyFocusAndBlurEventsAfterAutofillAndClearingWithFocusElement) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'><br>"
      "<label>Last Name:</label> <input id='lname' name='1'><br>"
      "</form></html>");

  focus_test_utils_->SetUpFocusLogging();
  focus_test_utils_->FocusElement("fname");

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);

  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("myForm")));
  EXPECT_TRUE(forms.removed_forms.empty());

  FormData values_to_fill = forms.updated_forms[0];
  values_to_fill.fields[0].value = u"John";
  values_to_fill.fields[0].is_autofilled = true;
  values_to_fill.fields[1].value = u"Smith";
  values_to_fill.fields[1].is_autofilled = true;

  auto fname =
      GetFormControlElementById(GetMainFrame()->GetDocument(), "fname");

  // Simulate filling the form using Autofill.
  form_util::ApplyAutofillAction(values_to_fill, fname,
                                 mojom::AutofillActionType::kFill,
                                 mojom::AutofillActionPersistence::kFill);

  // Simulate clearing the form.
  form_cache.ClearSectionWithElement(fname);

  // Expected Result in order:
  // - from filling
  //  * Change fname
  //  * Blur fname
  //  * Focus lname
  //  * Change lname
  //  * Blur lname
  //  * Focus fname
  // - from clearing
  //  * Change fname
  //  * Blur fname
  //  * Focus lname
  //  * Change lname
  //  * Blur lname
  //  * Focus fname
  EXPECT_EQ(GetFocusLog(), "c0b0f1c1b1f0c0b0f1c1b1f0");
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
      <selectlist name="selectlist" id="selectlist">
        <option value="first">first</option>
        <option value="second" selected>second</option>
      </selectlist>
    </div>
  )");

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);

  EXPECT_THAT(forms.updated_forms, ElementsAre(HasId(FormRendererId())));
  EXPECT_TRUE(forms.removed_forms.empty());

  EXPECT_EQ(1u, test_api(form_cache).initial_select_values_size());
  EXPECT_EQ(1u, test_api(form_cache).initial_selectlist_values_size());
  EXPECT_EQ(1u, test_api(form_cache).initial_checked_state_size());

  ExecuteJavaScriptForTests(R"(
    const container = document.getElementById('container');
    while (container.childElementCount > 0) {
      container.removeChild(container.children.item(0));
    }
  )");

  forms = form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);
  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_THAT(forms.removed_forms, ElementsAre(FormRendererId()));
  EXPECT_EQ(0u, test_api(form_cache).initial_select_values_size());
  EXPECT_EQ(0u, test_api(form_cache).initial_selectlist_values_size());
  EXPECT_EQ(0u, test_api(form_cache).initial_checked_state_size());
}

TEST_F(FormCacheBrowserTest, IsFormElementEligibleForManualFilling) {
  // Load a form.
  LoadHTML(
      "<html><form id='myForm'>"
      "<label>First Name:</label><input id='fname' name='0'><br>"
      "<label>Middle Name:</label> <input id='mname' name='1'><br>"
      "<label>Last Name:</label> <input id='lname' name='2'><br>"
      "</form></html>");

  WebDocument doc = GetMainFrame()->GetDocument();
  auto first_name_element = GetFormControlElementById(doc, "fname");
  auto middle_name_element = GetFormControlElementById(doc, "mname");
  auto last_name_element = GetFormControlElementById(doc, "lname");

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);

  EXPECT_THAT(forms.updated_forms, ElementsAre(HasName("myForm")));
  EXPECT_TRUE(forms.removed_forms.empty());

  const FormData* form_data = GetFormByName(forms.updated_forms, "myForm");
  EXPECT_EQ(3u, form_data->fields.size());

  // Set the first_name and last_name fields as eligible for manual filling.
  std::vector<FieldRendererId> fields_eligible_for_manual_filling;
  fields_eligible_for_manual_filling.push_back(
      form_data->fields[0].unique_renderer_id);
  fields_eligible_for_manual_filling.push_back(
      form_data->fields[2].unique_renderer_id);
  form_cache.SetFieldsEligibleForManualFilling(
      fields_eligible_for_manual_filling);

  EXPECT_TRUE(test_api(form_cache)
                  .IsFormElementEligibleForManualFilling(first_name_element));
  EXPECT_FALSE(test_api(form_cache)
                   .IsFormElementEligibleForManualFilling(middle_name_element));
  EXPECT_TRUE(test_api(form_cache)
                  .IsFormElementEligibleForManualFilling(last_name_element));
}

// Test that the FormCache does not contain empty forms.
TEST_F(FormCacheBrowserTest, DoNotStoreEmptyForms) {
  LoadHTML(R"(<form></form>)");

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);

  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_TRUE(forms.removed_forms.empty());

  EXPECT_EQ(1u, GetMainFrame()->GetDocument().Forms().size());
  EXPECT_EQ(0u, test_api(form_cache).extracted_forms_size());
}

// Test that the FormCache never contains more than |kMaxExtractableFields|
// non-empty extracted forms.
TEST_F(FormCacheBrowserTest, FormCacheSizeUpperBound) {
  // Create a HTML page that contains `kMaxExtractableFields + 1` non-empty
  // forms.
  std::string html;
  for (unsigned int i = 0; i < kMaxExtractableFields + 1; i++) {
    html += "<form><input></form>";
  }
  LoadHTML(html.c_str());

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);

  EXPECT_EQ(forms.updated_forms.size(), kMaxExtractableFields);
  EXPECT_TRUE(forms.removed_forms.empty());

  EXPECT_EQ(kMaxExtractableFields + 1,
            GetMainFrame()->GetDocument().Forms().size());
  EXPECT_EQ(kMaxExtractableFields, test_api(form_cache).extracted_forms_size());
}

// Test that FormCache::UpdateFormCache() limits the number of total fields by
// skipping any additional forms.
TEST_F(FormCacheBrowserTest, FieldLimit) {
  std::string html;
  for (unsigned int i = 0; i < kMaxExtractableFields + 1; i++) {
    html += "<form><input></form>";
  }
  LoadHTML(html.c_str());

  ASSERT_EQ(kMaxExtractableFields + 1,
            GetMainFrame()->GetDocument().Forms().size());

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);

  EXPECT_EQ(kMaxExtractableFields, forms.updated_forms.size());
  EXPECT_TRUE(forms.removed_forms.empty());
}

// Test that FormCache::UpdateFormCache() limits the number of total frames by
// clearing their frames and skipping the then-empty forms.
TEST_F(FormCacheBrowserTest, FrameLimit) {
  std::string html;
  for (unsigned int i = 0; i < kMaxExtractableChildFrames + 1; i++) {
    html += "<form><iframe></iframe></form>";
  }
  LoadHTML(html.c_str());

  ASSERT_EQ(kMaxExtractableChildFrames + 1,
            GetMainFrame()->GetDocument().Forms().size());

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);

  EXPECT_EQ(kMaxExtractableChildFrames, forms.updated_forms.size());
  EXPECT_TRUE(forms.removed_forms.empty());
}

// Test that FormCache::UpdateFormCache() limits the number of total fields and
// total frames:
// - the forms [0, kMaxExtractableChildFrames) should be unchanged,
// - the forms [kMaxExtractableChildFrames, kMaxExtractableFields) should have
//   empty FormData::child_frames,
// - the forms [kMaxExtractableFields, end) should be skipped.
// TODO(https://crbug.com/1287782): Flaky on android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_FieldAndFrameLimit DISABLED_FieldAndFrameLimit
#else
#define MAYBE_FieldAndFrameLimit FieldAndFrameLimit
#endif
TEST_F(FormCacheBrowserTest, MAYBE_FieldAndFrameLimit) {
  ASSERT_LE(kMaxExtractableChildFrames, kMaxExtractableFields);

  std::string html;
  for (unsigned int i = 0; i < kMaxExtractableFields + 1; i++) {
    html += "<form><input><iframe></iframe></form>";
  }
  LoadHTML(html.c_str());

  ASSERT_EQ(kMaxExtractableFields + 1,
            GetMainFrame()->GetDocument().Forms().size());

  FormCache form_cache(GetMainFrame());
  FormCache::UpdateFormCacheResult forms =
      form_cache.UpdateFormCache(/*field_data_manager=*/nullptr);

  EXPECT_EQ(forms.updated_forms.size(), kMaxExtractableFields);
  EXPECT_TRUE(base::ranges::none_of(forms.updated_forms,
                                    &std::vector<FormFieldData>::empty,
                                    &FormData::fields));
  EXPECT_TRUE(base::ranges::none_of(
      base::make_span(forms.updated_forms)
          .subspan(0, kMaxExtractableChildFrames),
      &std::vector<FrameTokenWithPredecessor>::empty, &FormData::child_frames));
  EXPECT_TRUE(base::ranges::all_of(
      base::make_span(forms.updated_forms).subspan(kMaxExtractableChildFrames),
      &std::vector<FrameTokenWithPredecessor>::empty, &FormData::child_frames));

  EXPECT_TRUE(forms.removed_forms.empty());
}

}  // namespace autofill
