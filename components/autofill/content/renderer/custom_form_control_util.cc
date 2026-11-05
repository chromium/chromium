// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/custom_form_control_util.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_element.h"

namespace autofill {

namespace {

// Custom form control scripts provide field types as (string representations
// of) Autofill FieldTypes. For simplicity, these type predictions are sent to
// the browser as HtmlFieldTypes.
// TODO(crbug.com/582135788): Don't treat these types as autocomplete types.
std::optional<HtmlFieldType> ParseFieldType(std::u16string_view field_type) {
  // Currently only these four types are supported for custom form controls.
  if (field_type == u"ADDRESS_HOME_COUNTRY") {
    return HtmlFieldType::kCountryName;
  }
  if (field_type == u"ADDRESS_HOME_STATE") {
    return HtmlFieldType::kAddressLevel1;
  }
  if (field_type == u"ADDRESS_HOME_CITY") {
    return HtmlFieldType::kAddressLevel2;
  }
  if (field_type == u"PHONE_HOME_COUNTRY_CODE") {
    return HtmlFieldType::kTelCountryCode;
  }
  return std::nullopt;
}

}  // namespace

base::flat_map<blink::WebFormElement, std::vector<CustomFormControl>>
GroupCustomFormControlsByForm(std::vector<CustomFormControl> controls) {
  base::flat_map<blink::WebFormElement, std::vector<CustomFormControl>> result;
  for (CustomFormControl& control : controls) {
    blink::WebFormElement form =
        form_util::GetOutermostAncestorFormElement(control.canonical_dom_node);
    result[form].push_back(std::move(control));
  }
  return result;
}

FormFieldData CustomFormControlToFormField(const CustomFormControl& control) {
  const blink::WebElement& element = control.canonical_dom_node;
  FormFieldData field;
  field.set_renderer_id(form_util::GetFieldRendererId(element));
  field.set_host_form_id(form_util::GetFormRendererId(
      form_util::GetOutermostAncestorFormElement(element)));
  field.set_form_control_type(FormControlType::kSyntheticSelectOne);

  if (control.current_option) {
    field.set_value(control.current_option->value.substr(0, kMaxStringLength));
    field.set_selected_option_text(
        control.current_option->text.substr(0, kMaxStringLength));
  }
  // TODO(crbug.com/581337039): Consider running label inference as a fallback.
  if (control.label) {
    field.set_label(control.label->substr(0, kMaxStringLength));
    field.set_label_source(
        FormFieldData::LabelSource::kCustomFormControlScript);
  }

  field.set_id_attribute(element.GetIdAttribute().Utf16());
  field.set_name_attribute(element.GetAttribute("name").Utf16());
  field.set_name(!field.name_attribute().empty() ? field.name_attribute()
                                                 : field.id_attribute());
  field.set_css_classes(element.GetAttribute("class").Utf16());
  field.set_aria_label(form_util::GetAriaLabel(element.GetDocument(), element));
  field.set_aria_description(
      form_util::GetAriaDescription(element.GetDocument(), element));

  if (std::optional<HtmlFieldType> html_field_type =
          ParseFieldType(control.field_type)) {
    field.set_parsed_autocomplete(
        AutocompleteParsingResult{.field_type = *html_field_type});
  }

  field.set_is_focusable(control.is_active);
  field.set_is_visible(control.is_active);
  field.set_is_enabled(control.is_active);
  // Standard <select> elements don't support the readonly attribute. It is thus
  // kept false for synthetic selects too.
  field.set_is_readonly(false);
  field.set_bounds(form_util::GetBoundsInWindow(element));
  field.set_should_autocomplete(false);
  field.set_max_length(0);
  return field;
}

}  // namespace autofill
