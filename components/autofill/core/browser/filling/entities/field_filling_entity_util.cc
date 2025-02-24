// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/entities/field_filling_entity_util.h"

#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

std::u16string GetObfuscatedAttributeValue(const AttributeInstance& attribute) {
  // Same obfuscation symbol as used for credit cards - see also credit_card.h.
  //  - \u2022 - Bullet.
  //  - \u2006 - SIX-PER-EM SPACE (small space between bullets).
  //  - \u2060 - WORD-JOINER (makes obfuscated string indivisible).
  static constexpr char16_t kDot[] = u"\u2022\u2060\u2006\u2060";
  // This is only an approximation of the number of the actual unicode
  // characters - if we want to match the length exactly, we would need to use
  // `base::CountUnicodeCharacters`.
  const size_t obfuscation_length = attribute.value().size();
  std::u16string result;
  // TODO(crbug.com/394011769): Investigate whether the obfuscation should
  // should include some of the attribute's value, e.g. the last x characters.
  result.reserve(sizeof(kDot) * obfuscation_length);
  for (size_t i = 0; i < obfuscation_length; ++i) {
    result.append(kDot);
  }
  return result;
}

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
                             mojom::ActionPersistence action_persistence) {
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
      attribute_instance->type().is_obfuscated();
  // TODO(crbug.com/397620383): Which type should we return here?
  return {should_obfuscate ? GetObfuscatedAttributeValue(*attribute_instance)
                           : attribute_instance->value(),
          std::nullopt};
}

}  // namespace autofill
