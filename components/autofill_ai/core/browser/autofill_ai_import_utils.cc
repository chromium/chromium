// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_import_utils.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/filling/autofill_ai/select_date_matching.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill_ai {

namespace {

using autofill::AttributeInstance;
using autofill::AttributeType;
using autofill::AutofillField;
using autofill::DatePartRange;
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

struct ValueAndFormatString {
  std::u16string value;
  std::u16string format_string;
};

// Returns the value and format string of `field` for import by Autofill AI.
ValueAndFormatString GetValueAndFormatString(const AutofillField& field) {
  std::optional<FieldType> field_type =
      field.GetAutofillAiServerTypePredictions();
  if (!field_type) {
    return {};
  }

  if (!IsDateFieldType(*field_type) || !field.IsSelectElement()) {
    std::u16string value = field.value_for_import();
    base::TrimWhitespace(value, base::TRIM_ALL, &value);
    return {
        .value = std::move(value),
        .format_string = field.format_string() ? *field.format_string() : u""};
  }

  auto get_value = [&](DatePartRange range) {
    // TODO(crbug.com/415805985): Consider adding a heuristic to decide what
    // value to extract for date select options (value vs label vs index).
    const std::u16string& value = field.value();
    uint32_t index = 0;
    while (index < range.options.size() &&
           value != range.options[index].value) {
      ++index;
    }
    if (index < range.options.size()) {
      return base::NumberToString16(range.first_value + index);
    }
    return std::u16string();
  };
  std::u16string value;
  if (!(value = get_value(GetYearRange(field.options()))).empty()) {
    return {.value = std::move(value), .format_string = u"YYYY"};
  } else if (!(value = get_value(GetMonthRange(field.options()))).empty()) {
    return {.value = std::move(value), .format_string = u"M"};
  } else if (!(value = get_value(GetDayRange(field.options()))).empty()) {
    return {.value = std::move(value), .format_string = u"D"};
  }
  return {};
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

    ValueAndFormatString value = GetValueAndFormatString(*field);
    if (value.value.empty()) {
      continue;
    }

    std::optional<AttributeType> attribute_type =
        AttributeType::FromFieldType(*field_type);

    std::map<AttributeType, AttributeInstance>& entity_attributes =
        section_to_entity_types_attributes[field->section()]
                                          [attribute_type->entity_type()];
    auto attribute_it =
        entity_attributes.try_emplace(*attribute_type, *attribute_type).first;
    attribute_it->second.SetInfo(field->Type().GetStorableType(), value.value,
                                 app_locale, value.format_string,
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
          /*nickname=*/std::string(""), base::Time::Now(), /*use_count=*/0,
          /*use_date=*/base::Time::Now());
      if (!EntitySatisfiesImportConstraints(entity)) {
        continue;
      }
      entities_found_in_form.push_back(std::move(entity));
    }
  }

  return entities_found_in_form;
}

std::optional<std::u16string> MaybeGetLocalizedDate(
    const autofill::AttributeInstance& attribute) {
  autofill::FieldType field_type = attribute.type().field_type();
  if (!IsDateFieldType(field_type)) {
    return std::nullopt;
  }
  auto get_part = [&](std::u16string format) {
    int part = 0;
    // The app_locale is irrelevant for dates.
    bool success = base::StringToInt(
        attribute.GetInfo(field_type, /*app_locale=*/"", format), &part);
    return success ? part : 0;
  };
  base::Time time;
  bool success = base::Time::FromLocalExploded(
      base::Time::Exploded{.year = get_part(u"YYYY"),
                           .month = get_part(u"M"),
                           .day_of_month = get_part(u"D")},
      &time);
  if (!success) {
    return std::nullopt;
  }
  return base::LocalizedTimeFormatWithPattern(time, "yMMMd");
}

}  // namespace autofill_ai
