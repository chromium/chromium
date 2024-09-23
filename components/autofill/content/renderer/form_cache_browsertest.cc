// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_cache.h"

#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/autofill_agent_test_api.h"
#include "components/autofill/content/renderer/autofill_renderer_test.h"
#include "components/autofill/content/renderer/focus_test_utils.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/form_cache_test_api.h"
#include "components/autofill/content/renderer/test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_select_element.h"

using blink::WebDocument;
using blink::WebElement;
using testing::AllOf;
using testing::ElementsAre;
using testing::Field;
using testing::Property;
using testing::UnorderedElementsAre;

namespace autofill {
namespace {

using CheckStatus = FormFieldData::CheckStatus;

auto HasId(FormRendererId expected_id) {
  return Property("renderer_id", &FormData::renderer_id, expected_id);
}

auto HasName(std::string_view expected_name) {
  return Property("name", &FormData::name, base::ASCIIToUTF16(expected_name));
}

auto IsToken(FrameToken expected_token, int expected_predecessor) {
  return AllOf(
      Field(&FrameTokenWithPredecessor::token, expected_token),
      Field(&FrameTokenWithPredecessor::predecessor, expected_predecessor));
}

const FormData* GetFormByName(const std::vector<FormData>& forms,
                              std::string_view name) {
  for (const FormData& form : forms) {
    if (form.name() == base::ASCIIToUTF16(name)) {
      return &form;
    }
  }
  return nullptr;
}

class FormCacheBrowserTest : public test::AutofillRendererTest {
 public:
  ~FormCacheBrowserTest() override = default;

  void SetUp() override {
    test::AutofillRendererTest::SetUp();
    form_cache_.emplace(&autofill_agent());
  }

  void TearDown() override {
    form_cache_.reset();
    test::AutofillRendererTest::TearDown();
  }

  FormCache::UpdateFormCacheResult UpdateFormCache() {
    return form_cache_->UpdateFormCache(GetFieldDataManager());
  }

  size_t num_extracted_forms() {
    return test_api(*form_cache_).num_extracted_forms();
  }

  FieldDataManager& GetFieldDataManager() const {
    return *field_data_manager_.get();
  }

  test::FocusTestUtils& focus_test_utils() { return focus_test_utils_; }

 private:
  scoped_refptr<FieldDataManager> field_data_manager_ =
      base::MakeRefCounted<FieldDataManager>();

  test::FocusTestUtils focus_test_utils_{
      base::BindRepeating(&FormCacheBrowserTest::ExecuteJavaScriptForTests,
                          base::Unretained(this))};

  // The subject of this test fixture. The fixture inherits from
  // AutofillRendererTest because we need an AutofillAgent to initialize a
  // FormCache. That AutofillAgent has a FormCache on its own, but we define a
  // separate one for this test fixture because the `AutofillAgent`'s cache is
  // used and populated by `AutofillAgent`.
  std::optional<FormCache> form_cache_;
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

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();

  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

  const FormData* form1 = GetFormByName(forms.updated_forms, "form1");
  ASSERT_TRUE(form1);
  EXPECT_EQ(3u, form1->fields().size());
  EXPECT_TRUE(form1->child_frames().empty());

  const FormData* unowned_form = GetFormByName(forms.updated_forms, "");
  ASSERT_TRUE(unowned_form);
  EXPECT_EQ(1u, unowned_form->fields().size());
  EXPECT_TRUE(unowned_form->child_frames().empty());
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

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();

  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1"),
                                   HasName("form2")));
  EXPECT_TRUE(forms.removed_forms.empty());

  std::vector<FormRendererId> forms_renderer_id;
  for (const FormData& form : forms.updated_forms) {
    if (form.renderer_id() != FormRendererId()) {
      forms_renderer_id.push_back(form.renderer_id());
    }
  }
  ASSERT_EQ(forms_renderer_id.size(), 2u);

  ExecuteJavaScriptForTests(R"(
    document.getElementById("form1").remove();
    document.getElementById("form2").innerHTML = "";
  )");

  forms = UpdateFormCache();

  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_THAT(forms.removed_forms,
              UnorderedElementsAre(forms_renderer_id[0], forms_renderer_id[1]));

  ExecuteJavaScriptForTests(R"(
    document.getElementById("unowned_element").remove();
  )");

  forms = UpdateFormCache();

  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_THAT(forms.removed_forms, ElementsAre(FormRendererId()));

  ExecuteJavaScriptForTests(R"(
    document.getElementById("form2").innerHTML = `
      <input type="text" name="foo1">
      <input type="text" name="foo2">
      <input type="text" name="foo3">
    `;
  )");

  forms = UpdateFormCache();

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

  forms = UpdateFormCache();

  EXPECT_THAT(forms.updated_forms, ElementsAre(HasName("form2")));
  EXPECT_TRUE(forms.removed_forms.empty());
}

// Test if the form gets re-extracted after a label change.
TEST_F(FormCacheBrowserTest, ExtractFormAfterDynamicFieldChange) {
  LoadHTML(R"(
    <form id="f"><input></form>
    <form id="g"> <label id="label">Name</label><input></form>
  )");

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();
  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasName("f"), HasName("g")));
  EXPECT_TRUE(forms.removed_forms.empty());

  ExecuteJavaScriptForTests(R"(
    document.getElementById("label").innerHTML = "Last Name";
  )");

  forms = UpdateFormCache();
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

  FrameToken frame1_token = GetFrameToken(GetDocument(), "frame1");
  FrameToken frame2_token = GetFrameToken(GetDocument(), "frame2");

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();

  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

  const FormData* form1 = GetFormByName(forms.updated_forms, "form1");
  ASSERT_TRUE(form1);
  EXPECT_TRUE(form1->fields().empty());
  EXPECT_THAT(form1->child_frames(), ElementsAre(IsToken(frame1_token, -1)));

  const FormData* unowned_form = GetFormByName(forms.updated_forms, "");
  ASSERT_TRUE(unowned_form);
  EXPECT_TRUE(unowned_form->fields().empty());
  EXPECT_THAT(unowned_form->child_frames(),
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

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();

  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

  forms = UpdateFormCache();
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

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();

  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

  forms = UpdateFormCache();
  // As nothing has changed, there are no new or removed forms.
  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_TRUE(forms.removed_forms.empty());
}

// TODO(crbug.com/40144964) Adjust expectations when we omit invisible iframes.
TEST_F(FormCacheBrowserTest, ExtractFramesAfterVisibilityChange) {
  LoadHTML(R"(
    <form id="form1">
      <iframe id="frame1" style="display: none;"></iframe>
      <iframe id="frame2" style="display: none;"></iframe>
    </form>
    <iframe id="frame3" style="display: none;"></iframe>
  )");

  WebElement iframe1 = GetElementById(GetDocument(), "frame1");
  WebElement iframe2 = GetElementById(GetDocument(), "frame2");
  WebElement iframe3 = GetElementById(GetDocument(), "frame3");

  auto GetSize = [](const WebElement& element) {
    gfx::Rect bounds = element.BoundsInWidget();
    return bounds.width() * bounds.height();
  };

  ASSERT_LE(GetSize(iframe1), 0);
  ASSERT_LE(GetSize(iframe2), 0);
  ASSERT_LE(GetSize(iframe3), 0);

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();
  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

  iframe1.SetAttribute("style", "display: block;");
  iframe2.SetAttribute("style", "display: block;");
  iframe3.SetAttribute("style", "display: block;");

  ASSERT_GT(GetSize(iframe1), 0);
  ASSERT_GT(GetSize(iframe2), 0);
  ASSERT_GT(GetSize(iframe3), 0);

  forms = UpdateFormCache();
  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_TRUE(forms.removed_forms.empty());

  iframe2.SetAttribute("style", "display: none;");
  iframe3.SetAttribute("style", "display: none;");

  ASSERT_GT(GetSize(iframe1), 0);
  ASSERT_LE(GetSize(iframe2), 0);
  ASSERT_LE(GetSize(iframe3), 0);

  forms = UpdateFormCache();
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

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();
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

  forms = UpdateFormCache();
  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("form1")));
  EXPECT_TRUE(forms.removed_forms.empty());

  const FormData* form1 = GetFormByName(forms.updated_forms, "form1");
  ASSERT_TRUE(form1);
  EXPECT_EQ(4u, form1->fields().size());

  const FormData* unowned_form = GetFormByName(forms.updated_forms, "");
  ASSERT_TRUE(unowned_form);
  EXPECT_EQ(2u, unowned_form->fields().size());
}

// Tests that correct focus, change and blur events are emitted during the
// autofilling and clearing of the form with an initially focused element.
// TODO: crbug.com/40100455 - Move elsewhere; the test is not about FormCache.
TEST_F(FormCacheBrowserTest,
       VerifyFocusAndBlurEventsAfterAutofillAndClearingWithFocusElement) {
  LoadHTML(R"(<form id='myForm'>
              <label>First Name:</label><input id='fname' name='0'><br>
              <label>Last Name:</label> <input id='lname' name='1'><br>
              </form>)");

  focus_test_utils().SetUpFocusLogging();
  focus_test_utils().FocusElement("fname");

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();

  EXPECT_THAT(forms.updated_forms,
              UnorderedElementsAre(HasId(FormRendererId()), HasName("myForm")));
  EXPECT_TRUE(forms.removed_forms.empty());

  std::vector<FormFieldData::FillData> values_to_fill;
  values_to_fill.reserve(forms.updated_forms[0].fields().size());
  for (const FormFieldData& field : forms.updated_forms[0].fields()) {
    values_to_fill.emplace_back(field);
  }
  values_to_fill[0].value = u"John";
  values_to_fill[0].is_autofilled = true;
  values_to_fill[1].value = u"Smith";
  values_to_fill[1].is_autofilled = true;

  auto fname = GetFormControlElementById("fname");

  // Simulate filling the form using Autofill.
  form_util::ApplyFieldsAction(
      GetDocument(), values_to_fill, mojom::FormActionType::kFill,
      mojom::ActionPersistence::kFill, GetFieldDataManager());

  // Expected Result in order:
  // - from filling
  //  * Change fname
  //  * Blur fname
  //  * Focus lname
  //  * Change lname
  //  * Blur lname
  //  * Focus fname
  EXPECT_EQ(focus_test_utils().GetFocusLog(GetDocument()), "c0b0f1c1b1f0");
}

// Test that the FormCache does not contain empty forms.
TEST_F(FormCacheBrowserTest, DoNotStoreEmptyForms) {
  LoadHTML(R"(<form></form>)");

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();

  EXPECT_TRUE(forms.updated_forms.empty());
  EXPECT_TRUE(forms.removed_forms.empty());

  EXPECT_EQ(1u, GetDocument().GetTopLevelForms().size());
  EXPECT_EQ(0u, num_extracted_forms());
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

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();

  EXPECT_EQ(forms.updated_forms.size(), kMaxExtractableFields);
  EXPECT_TRUE(forms.removed_forms.empty());

  EXPECT_EQ(kMaxExtractableFields + 1, GetDocument().GetTopLevelForms().size());
  EXPECT_EQ(kMaxExtractableFields, num_extracted_forms());
}

// Test that FormCache::UpdateFormCache() limits the number of total fields by
// skipping any additional forms.
TEST_F(FormCacheBrowserTest, FieldLimit) {
  std::string html;
  for (unsigned int i = 0; i < kMaxExtractableFields + 1; i++) {
    html += "<form><input></form>";
  }
  LoadHTML(html.c_str());

  ASSERT_EQ(kMaxExtractableFields + 1, GetDocument().GetTopLevelForms().size());

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();

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
            GetDocument().GetTopLevelForms().size());

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();

  EXPECT_EQ(kMaxExtractableChildFrames, forms.updated_forms.size());
  EXPECT_TRUE(forms.removed_forms.empty());
}

// Test that FormCache::UpdateFormCache() limits the number of total fields and
// total frames:
// - the forms [0, kMaxExtractableChildFrames) should be unchanged,
// - the forms [kMaxExtractableChildFrames, kMaxExtractableFields) should have
//   empty FormData::child_frames,
// - the forms [kMaxExtractableFields, end) should be skipped.
// TODO(crbug.com/40816477): Flaky on android.
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

  ASSERT_EQ(kMaxExtractableFields + 1, GetDocument().GetTopLevelForms().size());

  FormCache::UpdateFormCacheResult forms = UpdateFormCache();

  EXPECT_EQ(forms.updated_forms.size(), kMaxExtractableFields);
  EXPECT_TRUE(std::ranges::none_of(forms.updated_forms,
                                   &std::vector<FormFieldData>::empty,
                                   &FormData::fields));
  EXPECT_TRUE(std::ranges::none_of(
      base::make_span(forms.updated_forms).first(kMaxExtractableChildFrames),
      &std::vector<FrameTokenWithPredecessor>::empty, &FormData::child_frames));
  EXPECT_TRUE(std::ranges::all_of(
      base::make_span(forms.updated_forms).subspan(kMaxExtractableChildFrames),
      &std::vector<FrameTokenWithPredecessor>::empty, &FormData::child_frames));

  EXPECT_TRUE(forms.removed_forms.empty());
}

// Tests that form extraction measures its total time.
TEST_F(FormCacheBrowserTest, UpdateFormCacheMeasuresTotalTime) {
  base::HistogramTester histogram_tester;
  LoadHTML(R"(
    <input>
  )");
  // FormCache::UpdateFormCache() is called by AutofillAgent.
  histogram_tester.ExpectTotalCount(  //
      "Autofill.TimingPrecise.UpdateFormCache", 1);
  histogram_tester.ExpectTotalCount(  //
      "Autofill.TimingPrecise.UpdateFormCache.UpdateFormCache", 1);
  // form_util::ExtractFormData() is also called by PasswordAutofillAgent.
  histogram_tester.ExpectTotalCount(  //
      "Autofill.TimingPrecise.ExtractFormData", 3);
  histogram_tester.ExpectTotalCount(  //
      "Autofill.TimingPrecise.ExtractFormData.UpdateFormCache", 1);
}

}  // namespace
}  // namespace autofill
