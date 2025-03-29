// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_import_utils.h"

#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill_ai {

namespace {

using autofill::AttributeInstance;
using autofill::AttributeType;
using autofill::AutofillField;
using autofill::DenseSet;
using autofill::EntityInstance;
using autofill::EntityType;
using autofill::FieldType;

bool EntitySatisfiesImportConstraints(const EntityInstance& entity) {
  return AttributesMeetImportConstraints(
      entity.type(), DenseSet(entity.attributes(), &AttributeInstance::type));
}

}  // namespace

bool AttributesMeetImportConstraints(EntityType entity_type,
                                     DenseSet<AttributeType> attributes) {
  return std::ranges::any_of(entity_type.import_constraints(),
                             [&](const DenseSet<AttributeType>& constraint) {
                               return attributes.contains_all(constraint);
                             });
}

std::vector<EntityInstance> GetPossibleEntitiesFromSubmittedForm(
    base::span<const std::unique_ptr<autofill::AutofillField>> fields,
    const std::string& app_locale) {
  std::map<autofill::Section,
           std::map<EntityType, std::map<AttributeType, AttributeInstance>>>
      section_to_entity_types_attributes;
  for (const std::unique_ptr<AutofillField>& field : fields) {
    std::optional<FieldType> field_type =
        field->GetAutofillAiServerTypePredictions();
    if (!field_type) {
      continue;
    }
    std::optional<AttributeType> attribute_type =
        AttributeType::FromFieldType(*field_type);
    CHECK(attribute_type);
    // TODO(crbug.com/389629676): Save data format.
    std::u16string value = field->value(autofill::ValueSemantics::kCurrent);
    base::TrimWhitespace(value, base::TRIM_ALL, &value);
    if (value.empty()) {
      continue;
    }

    std::map<AttributeType, AttributeInstance>& entity_attributes =
        section_to_entity_types_attributes[field->section()]
                                          [attribute_type->entity_type()];
    auto attribute_it =
        entity_attributes.try_emplace(*attribute_type, *attribute_type).first;
    attribute_it->second.SetInfo(
        field->Type().GetStorableType(), value, app_locale,
        field->format_string() ? *field->format_string() : u"",
        autofill::VerificationStatus::kObserved);
  }

  for (auto& [section, entities] : section_to_entity_types_attributes) {
    for (auto& [entity_type, attributes] : entities) {
      for (auto& [attribute_type, attribute] : attributes) {
        attribute.FinalizeInfo();
      }
      std::erase_if(attributes, [&](const auto& pair) {
        const AttributeInstance& attribute = pair.second;
        return attribute.GetCompleteInfo(app_locale).empty();
      });
    }
  }

  std::vector<EntityInstance> entities_found_in_form;
  for (auto& [section, entity_to_attributes] :
       section_to_entity_types_attributes) {
    for (auto& [entity_name, attributes] : entity_to_attributes) {
      if (attributes.empty()) {
        continue;
      }
      EntityInstance entity = EntityInstance(
          EntityType(entity_name),
          base::ToVector(
              attributes,
              &std::pair<const AttributeType, AttributeInstance>::second),
          base::Uuid::GenerateRandomV4(),
          /*nickname=*/std::string(""), base::Time::Now());
      if (!EntitySatisfiesImportConstraints(entity)) {
        continue;
      }
      entities_found_in_form.push_back(std::move(entity));
    }
  }

  return entities_found_in_form;
}

}  // namespace autofill_ai
