// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_import_utils.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/autofill_ai_form_rationalization.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/filling/autofill_ai/select_date_matching.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

namespace {

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
  AutofillFormatString format_string;
};

// Returns the value and format string of `field` for import by Autofill AI.
ValueAndFormatString GetValueAndFormatString(const AutofillField& field,
                                             AttributeType attribute_type) {
  if (attribute_type.data_type() != AttributeType::DataType::kDate ||
      !field.IsSelectElement()) {
    std::u16string value = field.value_for_import();
    base::TrimWhitespace(value, base::TRIM_ALL, &value);
    return {.value = std::move(value),
            .format_string = field.format_string() ? *field.format_string()
                                                   : AutofillFormatString()};
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

  auto make_date_format = [](std::u16string fs) {
    return AutofillFormatString(std::move(fs), FormatString_Type_DATE);
  };

  std::u16string value;
  if (!(value = get_value(GetYearRange(field.options()))).empty()) {
    return {.value = std::move(value),
            .format_string = make_date_format(u"YYYY")};
  } else if (!(value = get_value(GetMonthRange(field.options()))).empty()) {
    return {.value = std::move(value), .format_string = make_date_format(u"M")};
  } else if (!(value = get_value(GetDayRange(field.options()))).empty()) {
    return {.value = std::move(value), .format_string = make_date_format(u"D")};
  }
  return {};
}

std::vector<EntityInstance> GetPossibleEntitiesFromSubmittedForm(
    base::span<const std::unique_ptr<AutofillField>> fields,
    const AutofillClient& client) {
  const GeoIpCountryCode& country_code = client.GetVariationConfigCountryCode();
  const std::string& app_locale = client.GetAppLocale();

  std::map<Section,
           std::map<EntityType, std::map<AttributeType, AttributeInstance>>>
      section_to_entity_types_attributes;

  // DetermineAttributeTypes() effectively gives us a map
  // Section -> EntityType -> AttributeType
  // and to build section_to_entity_types_attributes we want a map
  // Section -> EntityType -> AttributeType -> AttributeInstance.
  for (auto& [section, entities_with_fields_and_types] :
       RationalizeAndDetermineAttributeTypes(fields)) {
    base::EraseIf(
        entities_with_fields_and_types,
        [&country_code](
            const std::pair<EntityType,
                            std::vector<AutofillFieldWithAttributeType>>&
                entry) { return !entry.first.enabled(country_code); });
    std::map<FieldGlobalId, size_t> num_occurrences;
    for (const auto& [entity, fields_with_types] :
         entities_with_fields_and_types) {
      for (const auto& [field, attribute_type] : fields_with_types) {
        num_occurrences[field->global_id()] +=
            attribute_type.data_type() != AttributeType::DataType::kName;
      }
    }

    for (const auto& [entity, fields_with_types] :
         entities_with_fields_and_types) {
      for (const auto& [field, attribute_type] : fields_with_types) {
        if (num_occurrences[field->global_id()] >= 2) {
          continue;
        }
        DCHECK_EQ(entity, attribute_type.entity_type());
        const FieldType field_type = field->Type().GetAutofillAiType(entity);
        const ValueAndFormatString value =
            GetValueAndFormatString(*field, attribute_type);

        // At the moment, AutofillAI attributes can never save an email. At the
        // same time, in some countries fields that accept either an AutofillAI
        // type or an email address are common. This avoids mistakenly offering
        // to save those.
        if (value.value.empty() || IsValidEmailAddress(value.value)) {
          continue;
        }

        // Do not import entities that have an attribute whose value is a proper
        // prefix or suffix.
        if (IsAffixFormatStringEnabledForType(field_type) &&
            value.format_string.type == FormatString_Type_AFFIX &&
            data_util::IsValidAffixFormat(value.format_string.value,
                                          /*exclude_full_value=*/true)) {
          if (auto it = section_to_entity_types_attributes.find(section);
              it != section_to_entity_types_attributes.end()) {
            it->second.erase(entity);
          }
          break;
        }

        std::map<AttributeType, AttributeInstance>& entity_attributes =
            section_to_entity_types_attributes[section][entity];
        auto attribute_it =
            entity_attributes.try_emplace(attribute_type, attribute_type).first;
        attribute_it->second.SetInfo(field_type, value.value, app_locale,
                                     value.format_string,
                                     VerificationStatus::kObserved);
      }
    }
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
      // Some entities can be stored in Google Wallet servers. This depends
      // on whether the user is eligible (pref state and feature enabled)
      // and whether the entity type is "walletable". If the entity cannot be
      // stored on the Wallet servers, it is stored locally.
      const EntityInstance::RecordType record_type =
          MayPerformAutofillAiAction(client, AutofillAiAction::kImportToWallet,
                                     entity_name)
              ? EntityInstance::RecordType::kServerWallet
              : EntityInstance::RecordType::kLocal;
      EntityInstance entity = EntityInstance(
          entity_name,
          base::ToVector(
              attributes,
              &std::pair<const AttributeType, AttributeInstance>::second),
          EntityInstance::EntityId(base::Uuid::GenerateRandomV4()),
          /*nickname=*/std::string(""), base::Time::Now(), /*use_count=*/0,
          /*use_date=*/base::Time::Now(), record_type,
          EntityInstance::AreAttributesReadOnly(false),
          /*frecency_override=*/"");
      if (!EntitySatisfiesImportConstraints(entity)) {
        continue;
      }
      entities_found_in_form.push_back(std::move(entity));
    }
  }

  return entities_found_in_form;
}

std::optional<std::u16string> MaybeGetLocalizedDate(
    const AttributeInstance& attribute) {
  FieldType field_type = attribute.type().field_type();
  if (!IsDateFieldType(field_type)) {
    return std::nullopt;
  }
  auto get_part = [&](std::u16string format) {
    int part = 0;
    // The app_locale is irrelevant for dates.
    bool success = base::StringToInt(
        attribute.GetInfo(
            field_type, /*app_locale=*/"",
            AutofillFormatString(std::move(format), FormatString_Type_DATE)),
        &part);
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

}  // namespace autofill
