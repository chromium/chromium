// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_TEST_UTILS_AUTOFILL_FORM_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_TEST_UTILS_AUTOFILL_FORM_TEST_UTILS_H_

#include <concepts>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/types/is_instantiation.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill::test {

namespace internal {

// A description which allows the construction of `FormFieldData` and type
// predictions according to user specification.
//
// It also allows creating fields that are initialized as a certain `FieldType`
// by setting their `role`. However, since `FieldType` is a browser/ concept,
// this functionality cannot be supported generically in common/. To enable the
// use of a variant without a role in common/, the implementation is
// parameterized by a `TypeInjection` template type. This allows instantiating
// `FieldDescription`s with role support in browser/ and disabling this concept
// in common/ without duplicating the code.
template <typename TypeInjection>
  requires requires(TypeInjection::FieldType role) {
    // The type that should be used for the `role` member. `std::monostate` can
    // be used to (effectively) remove the member.
    typename TypeInjection::FieldType;
    // A function creating a field from a given role.
    { TypeInjection{}(role) } -> std::same_as<FormFieldData>;
    // The default role if none is specified.
    {
      TypeInjection::RoleDefaultValue
    } -> std::convertible_to<typename TypeInjection::FieldType>;
  }
struct FieldDescription {
  using RoleHandler = TypeInjection;

  // A `role` that defines the initial state of `FormFieldData` that is then
  // adapted according to the other fields of the `FieldDescription`. Only
  // available in layers that have access to browser/.
  TypeInjection::FieldType role = TypeInjection::RoleDefaultValue;
  // If the server type is not set explicitly, it is assumed to be given by the
  // `role`. Only available in layers that have access to browser/.
  std::optional<typename TypeInjection::FieldType> server_type;
  // If the heuristic type is not set explicitly, it is assumed to be given by
  // the `role`. Only available in layers that have access to browser/.
  std::optional<typename TypeInjection::FieldType> heuristic_type;

  std::optional<LocalFrameToken> host_frame;
  std::optional<FormSignature> host_form_signature;
  std::optional<FieldRendererId> renderer_id;
  bool is_focusable = true;
  bool is_visible = true;
  std::optional<std::u16string> label;
  std::optional<std::u16string> name;
  std::optional<std::u16string> name_attribute;
  std::optional<std::u16string> id_attribute;
  std::optional<std::u16string> nonce;
  std::optional<std::u16string> value;
  std::optional<std::u16string> placeholder;
  std::optional<std::u16string> aria_label;
  std::optional<std::u16string> aria_description;
  std::optional<uint64_t> max_length;
  const std::string autocomplete_attribute;
  std::optional<AutocompleteParsingResult> parsed_autocomplete;
  const FormControlType form_control_type = FormControlType::kInputText;
  bool should_autocomplete = true;
  std::optional<bool> is_autofilled_according_to_renderer;
  std::optional<url::Origin> origin;
  std::vector<SelectOption> select_options;
  std::vector<SelectOption> datalist_options;
  FieldPropertiesMask properties_mask = 0;
  bool checked = false;
  std::optional<int32_t> form_control_ax_id;
  std::optional<FormFieldData::LabelSource> label_source;
};

struct CreateFormFieldData {
  using FieldType = std::monostate;

  static constexpr FieldType RoleDefaultValue{};

  constexpr FormFieldData operator()(const FieldType&) const { return {}; }
};

// Attributes provided to the test form.
template <typename FieldDescriptionType>
  requires base::is_instantiation<FieldDescriptionType, FieldDescription>
struct FormDescription {
  const std::string description_for_logging;
  std::vector<FieldDescriptionType> fields;
  std::optional<LocalFrameToken> host_frame;
  std::optional<FormRendererId> renderer_id;
  const std::u16string name = u"TestForm";
  const std::string url = "https://example.com/form.html";
  const std::string action = "https://example.com/submit.html";
  std::optional<url::Origin> main_frame_origin;

  static constexpr std::string_view kDefaultTestOrigin =
      "https://example.test/";
};

}  // namespace internal

// A `FieldDescription` which can be used in layers below browser/. An instance
// of this can be passed to `GetFormFieldData` to generate test data. If the
// browser/ layer is available, the corresponding type `FieldDescription` (no
// template) should be used instead.
using CommonFieldDescription =
    internal::FieldDescription<internal::CreateFormFieldData>;
// Same as above but for `GetFormData`.
using CommonFormDescription = internal::FormDescription<CommonFieldDescription>;

// Creates a `FormFieldData` to be fed to the parser.
template <typename FieldDescriptionType = CommonFieldDescription>
  requires base::is_instantiation<FieldDescriptionType,
                                  internal::FieldDescription>
FormFieldData GetFormFieldData(const FieldDescriptionType& description);

// Creates a `FormData` to be fed to the parser.
template <typename FormDescriptionType = CommonFormDescription>
  requires base::is_instantiation<FormDescriptionType,
                                  internal::FormDescription>
FormData GetFormData(const FormDescriptionType& description);

// Template implementations below.

template <typename FieldDescriptionType>
  requires base::is_instantiation<FieldDescriptionType,
                                  internal::FieldDescription>
FormFieldData GetFormFieldData(const FieldDescriptionType& description) {
  FormFieldData field_data =
      typename FieldDescriptionType::RoleHandler{}(description.role);

  field_data.set_form_control_type(description.form_control_type);
  if (field_data.form_control_type() == FormControlType::kSelectOne &&
      !description.select_options.empty()) {
    field_data.set_options(description.select_options);
  }
  if (!description.datalist_options.empty()) {
    field_data.set_datalist_options(description.datalist_options);
  }
  field_data.set_renderer_id(
      description.renderer_id.value_or(MakeFieldRendererId()));
  field_data.set_host_form_id(MakeFormRendererId());
  field_data.set_is_focusable(description.is_focusable);
  field_data.set_is_visible(description.is_visible);
  if (!description.autocomplete_attribute.empty()) {
    field_data.set_autocomplete_attribute(description.autocomplete_attribute);
    field_data.set_parsed_autocomplete(
        ParseAutocompleteAttribute(description.autocomplete_attribute));
  }
  if (description.host_frame) {
    field_data.set_host_frame(*description.host_frame);
  }
  if (description.host_form_signature) {
    field_data.set_host_form_signature(*description.host_form_signature);
  }
  if (description.label) {
    field_data.set_label(*description.label);
  }
  if (description.name) {
    field_data.set_name(*description.name);
  }
  if (description.name_attribute) {
    field_data.set_name_attribute(*description.name_attribute);
  }
  if (description.id_attribute) {
    field_data.set_id_attribute(*description.id_attribute);
  }
  if (description.nonce) {
    field_data.set_nonce(*description.nonce);
  }
  if (description.value) {
    field_data.set_value(*description.value);
  }
  if (description.placeholder) {
    field_data.set_placeholder(*description.placeholder);
  }
  if (description.aria_label) {
    field_data.set_aria_label(*description.aria_label);
  }
  if (description.aria_description) {
    field_data.set_aria_description(*description.aria_description);
  }
  if (description.max_length) {
    field_data.set_max_length(*description.max_length);
  } else if (field_data.IsSelectElement()) {
    field_data.set_max_length(0);
  }
  if (description.origin) {
    field_data.set_origin(*description.origin);
  }
  field_data.set_is_autofilled_according_to_renderer(
      description.is_autofilled_according_to_renderer.value_or(false));
  field_data.set_should_autocomplete(description.should_autocomplete);
  field_data.set_properties_mask(description.properties_mask);
  if (field_data.form_control_type() == FormControlType::kInputCheckbox ||
      field_data.form_control_type() == FormControlType::kInputRadio) {
    field_data.set_check_status(
        description.checked
            ? FormFieldData::CheckStatus::kChecked
            : FormFieldData::CheckStatus::kCheckableButUnchecked);
  }
  if (description.form_control_ax_id) {
    field_data.set_form_control_ax_id(*description.form_control_ax_id);
  }
  if (description.label_source) {
    field_data.set_label_source(*description.label_source);
  }
  CHECK(!description.checked ||
        field_data.form_control_type() == FormControlType::kInputCheckbox ||
        field_data.form_control_type() == FormControlType::kInputRadio)
      << "Only <input type=checkbox> and <input type=radio> are checkable";
  return field_data;
}

template <typename FormDescriptionType>
  requires base::is_instantiation<FormDescriptionType,
                                  internal::FormDescription>
FormData GetFormData(const FormDescriptionType& description) {
  FormData form;
  form.set_url(GURL(description.url));
  form.set_action(GURL(description.action));
  form.set_name(description.name);
  form.set_host_frame(description.host_frame.value_or(MakeLocalFrameToken()));
  form.set_renderer_id(description.renderer_id.value_or(MakeFormRendererId()));
  if (description.main_frame_origin) {
    form.set_main_frame_origin(*description.main_frame_origin);
  } else {
    form.set_main_frame_origin(
        url::Origin::Create(GURL(FormDescriptionType::kDefaultTestOrigin)));
  }
  form.set_fields(base::ToVector(
      description.fields, [&form](const auto& field_description) {
        FormFieldData field_data = GetFormFieldData(field_description);
        // If the field description did not specify these members, use the data
        // of the form the field is in.
        if (!field_description.host_frame) {
          field_data.set_host_frame(form.host_frame());
        }
        if (!field_description.origin) {
          field_data.set_origin(form.main_frame_origin());
        }
        // Always overwrite the `host_form_id` to correctly associate the field
        // with the form.
        field_data.set_host_form_id(form.renderer_id());
        return field_data;
      }));
  return form;
}

}  // namespace autofill::test

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_TEST_UTILS_AUTOFILL_FORM_TEST_UTILS_H_
