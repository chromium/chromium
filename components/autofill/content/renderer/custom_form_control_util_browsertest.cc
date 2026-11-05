// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_util.h"

#include <optional>
#include <vector>

#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/test/render_view_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using CustomFormControlUtilTest = content::RenderViewTest;

// Tests that `GroupCustomFormControlsByForm()` groups custom form controls by
// their outermost ancestor `<form>` element and groups unowned controls under a
// null `blink::WebFormElement`, preserving relative order within each form.
TEST_F(CustomFormControlUtilTest, GroupCustomFormControlsByForm) {
  LoadHTML(R"(
    <div id="unowned1"></div>
    <form id="form1">
      <div id="f1_c1"></div>
      <div id="f1_c2"></div>
    </form>
    <div id="unowned2"></div>
    <form id="form2">
      <div id="f2_c1"></div>
    </form>
  )");

  blink::WebDocument doc = GetMainFrame()->GetDocument();
  CustomFormControl unowned1{.canonical_dom_node =
                                 doc.GetElementById("unowned1")};
  CustomFormControl f1_c1{.canonical_dom_node = doc.GetElementById("f1_c1")};
  CustomFormControl f1_c2{.canonical_dom_node = doc.GetElementById("f1_c2")};
  CustomFormControl unowned2{.canonical_dom_node =
                                 doc.GetElementById("unowned2")};
  CustomFormControl f2_c1{.canonical_dom_node = doc.GetElementById("f2_c1")};

  blink::WebFormElement form1 =
      doc.GetElementById("form1").To<blink::WebFormElement>();
  blink::WebFormElement form2 =
      doc.GetElementById("form2").To<blink::WebFormElement>();

  EXPECT_THAT(
      GroupCustomFormControlsByForm({unowned1, f1_c1, f1_c2, unowned2, f2_c1}),
      UnorderedElementsAre(
          Pair(blink::WebFormElement(), ElementsAre(unowned1, unowned2)),
          Pair(form1, ElementsAre(f1_c1, f1_c2)),
          Pair(form2, ElementsAre(f2_c1))));
}

// Tests that `CustomFormControlToFormField()` populates a `FormFieldData` from
// an active `CustomFormControl` and its underlying DOM element.
TEST_F(CustomFormControlUtilTest, CustomFormControlToFormField) {
  LoadHTML(R"(
    <form id="form1">
      <div id="c1" name="country_select" class="custom-select primary"
           aria-label="Select country" aria-description="Billing country"
           style="width: 100px; height: 20px;"></div>
    </form>
  )");
  blink::WebDocument doc = GetMainFrame()->GetDocument();
  blink::WebFormElement form =
      doc.GetElementById("form1").To<blink::WebFormElement>();
  blink::WebElement node = doc.GetElementById("c1");

  FormFieldData field = CustomFormControlToFormField(CustomFormControl{
      .canonical_dom_node = node,
      .field_type = u"ADDRESS_HOME_COUNTRY",
      .is_active = true,
      .label = u"Country",
      .current_option = SelectOption{.value = u"CH", .text = u"Switzerland"},
  });
  EXPECT_EQ(field.renderer_id(), form_util::GetFieldRendererId(node));
  EXPECT_EQ(field.host_form_id(), form_util::GetFormRendererId(form));
  EXPECT_EQ(field.form_control_type(), FormControlType::kSyntheticSelectOne);
  EXPECT_EQ(field.max_length(), 0u);
  EXPECT_EQ(field.id_attribute(), u"c1");
  EXPECT_EQ(field.name_attribute(), u"country_select");
  EXPECT_EQ(field.name(), u"country_select");
  EXPECT_EQ(field.css_classes(), u"custom-select primary");
  EXPECT_EQ(field.aria_label(), u"Select country");
  EXPECT_EQ(field.aria_description(), u"Billing country");
  EXPECT_TRUE(field.autocomplete_attribute().empty());
  EXPECT_EQ(
      field.parsed_autocomplete(),
      AutocompleteParsingResult{.field_type = HtmlFieldType::kCountryName});
  EXPECT_TRUE(field.is_focusable());
  EXPECT_TRUE(field.is_visible());
  EXPECT_TRUE(field.is_enabled());
  EXPECT_FALSE(field.is_readonly());
  EXPECT_FALSE(field.should_autocomplete());
  EXPECT_FALSE(field.bounds().IsEmpty());
  EXPECT_EQ(field.label(), u"Country");
  EXPECT_EQ(field.label_source(),
            FormFieldData::LabelSource::kCustomFormControlScript);
  EXPECT_EQ(field.value(), u"CH");
  EXPECT_EQ(field.selected_option_text(), u"Switzerland");
}

// Tests that `CustomFormControlToFormField()` handles an unknown `field_type`
// and omitted optional fields.
TEST_F(CustomFormControlUtilTest,
       CustomFormControlToFormFieldWithoutOptionals) {
  LoadHTML(R"(<div id="c1"></div>)");
  blink::WebElement node = GetMainFrame()->GetDocument().GetElementById("c1");

  FormFieldData field = CustomFormControlToFormField(CustomFormControl{
      .canonical_dom_node = node,
      .field_type = u"UNKNOWN_TYPE",
  });
  EXPECT_EQ(field.parsed_autocomplete(), std::nullopt);
  EXPECT_TRUE(field.label().empty());
  EXPECT_TRUE(field.value().empty());
  EXPECT_EQ(field.selected_option_text(), std::nullopt);
}

}  // namespace
}  // namespace autofill
