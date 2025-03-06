// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/entities/field_filling_entity_util.h"

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

namespace {
std::u16string GetValueForSelectControl(const std::u16string& value,
                                        const AutofillField& field) {
  switch (field.Type().GetStorableType()) {
    case ADDRESS_HOME_COUNTRY:
      return GetCountrySelectControlValue(value, field.options(),
                                          /*failure_to_fill=*/nullptr);
    default:
      return GetSelectControlValue(value, field.options(),
                                   /*failure_to_fill=*/nullptr)
          .value_or(u"");
  }
}
}  // namespace

base::flat_set<FieldGlobalId> GetFieldsFillableByAutofillAi(
    const FormStructure& form,
    const EntityDataManager& edm) {
  auto fillable_by_autofill_ai =
      [&, fillable_types =
              std::optional<FieldTypeSet>()](FieldType field_type) mutable {
        if (!fillable_types) {
          fillable_types.emplace();
          for (const EntityInstance& entity : edm.GetEntityInstances()) {
            for (const AttributeInstance& attribute : entity.attributes()) {
              fillable_types->insert(attribute.type().field_type());
            }
          }
        }
        return fillable_types->contains(field_type);
      };

  std::vector<FieldGlobalId> fields;
  for (const auto& field : form.fields()) {
    std::optional<FieldType> field_type =
        field->GetAutofillAiServerTypePredictions();
    if (field_type && fillable_by_autofill_ai(*field_type)) {
      fields.push_back(field->global_id());
    }
  }
  return std::move(fields);
}

std::pair<std::u16string, std::optional<FieldType>>
GetFillValueAndTypeForEntity(const EntityInstance& entity,
                             const AutofillField& field,
                             mojom::ActionPersistence action_persistence,
                             const std::string& app_locale) {
  std::optional<FieldType> field_type =
      field.GetAutofillAiServerTypePredictions();
  if (!field_type) {
    return {u"", std::nullopt};
  }
  std::optional<AttributeType> attribute_type =
      AttributeType::FromFieldType(*field_type);
  if (!attribute_type) {
    return {u"", std::nullopt};
  }
  base::optional_ref<const AttributeInstance> attribute_instance =
      entity.attribute(*attribute_type);
  if (!attribute_instance) {
    return {u"", std::nullopt};
  }
  const bool should_obfuscate =
      action_persistence != mojom::ActionPersistence::kFill &&
      !field.IsSelectElement() && attribute_instance->type().is_obfuscated();

  // TODO(crbug.com/389625753): Investigate whether only passing the
  // field type is the right choice here. This would for example
  // fail the fill a PASSPORT_NUMBER field that gets a
  // PHONE_HOME_WHOLE_NUMBER classification from regular autofill
  // prediction logic.
  std::u16string attribute_value = attribute_instance->GetInfo(
      field.Type().GetStorableType(), app_locale, field.format_string());

  if (!attribute_value.empty() && field.IsSelectElement()) {
    attribute_value = GetValueForSelectControl(attribute_value, field);
  }
  // TODO(crbug.com/397620383): Which type should we return here?
  // TODO(crbug.com/394011769): Investigate whether the obfuscation should
  // should include some of the attribute's value, e.g. the last x characters.
  return {
      should_obfuscate ? GetObfuscatedValue(attribute_value) : attribute_value,
      std::nullopt};
}

}  // namespace autofill
