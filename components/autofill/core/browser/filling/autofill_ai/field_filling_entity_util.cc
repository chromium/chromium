// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_ai_form_rationalization.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_quality/addresses/address_normalizer.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/autofill_ai/select_date_matching.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

namespace {

// Returns the `AttributeType`, if any, that can be used to fill `field` using
// `entity`. `section_to_entity_and_field_and_types` is the result of calling
// `DetermineAttributeTypes()`. It is assumed that there is at most one such
// type per entity for any given field.
std::optional<AttributeType> GetAttributeTypeForEntityAndField(
    base::flat_map<
        Section,
        base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>>
        section_to_entity_and_field_and_types,
    const EntityInstance& entity,
    const AutofillField& field) {
  auto it = section_to_entity_and_field_and_types.find(field.section());
  if (it == section_to_entity_and_field_and_types.end()) {
    // The whole section of the field does not contain a field fillable
    // by AutofillAi, hence no appropriate `AttributeType` exists for `field`.
    return std::nullopt;
  }

  const base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>&
      entity_to_fields_and_types = it->second;
  auto jt = entity_to_fields_and_types.find(entity.type());
  if (jt == entity_to_fields_and_types.end()) {
    // The section isn't fillable by the current entity, hence no appropriate
    // `AttributeType` exists for `field`.
    return std::nullopt;
  }

  const std::vector<AutofillFieldWithAttributeType>& field_and_types =
      jt->second;
  auto kt = std::ranges::find(field_and_types, field.global_id(),
                              [](const AutofillFieldWithAttributeType& f) {
                                return f.field->global_id();
                              });
  if (kt == field_and_types.end()) {
    // The field isn't fillable by the current entity, hence no appropriate
    // `AttributeType` exists.
    return std::nullopt;
  }

  return kt->type;
}

std::u16string MaybeStripPrefix(const std::u16string& value,
                                size_t field_max_length) {
  return field_max_length == 0 || field_max_length > value.size()
             ? value
             : value.substr(value.size() - field_max_length);
}

// Looks for the day, month, or year from `attribute` to fill into `field`.
std::optional<std::u16string> GetValueForDateSelect(
    const AttributeInstance& attribute,
    const AutofillField& field,
    const std::string& app_locale) {
  const FieldType type =
      field.Type().GetAutofillAiType(attribute.type().entity_type());
  if (!IsDateFieldType(type)) {
    return std::nullopt;
  }

  auto get_part = [&](std::u16string format_string, uint32_t min = 0,
                      uint32_t max =
                          std::numeric_limits<uint32_t>::max()) -> uint32_t {
    std::u16string s = attribute.GetInfo(
        type, app_locale,
        AutofillFormatString(std::move(format_string), FormatString_Type_DATE));
    unsigned int i = 0;
    return base::StringToUint(s, &i) && min <= i && i <= max
               ? i
               : std::numeric_limits<uint32_t>::max();
  };

  if (base::optional_ref<const SelectOption> match =
          GetDayRange(field.options()).get_by_value(get_part(u"D", 1, 31))) {
    return match->value;
  }
  if (base::optional_ref<const SelectOption> match =
          GetMonthRange(field.options()).get_by_value(get_part(u"M", 1, 12))) {
    return match->value;
  }
  if (base::optional_ref<const SelectOption> match =
          GetYearRange(field.options()).get_by_value(get_part(u"YYYY"))) {
    return match->value;
  }
  return std::nullopt;
}

std::u16string GetValueForInput(const AttributeInstance& attribute,
                                const AutofillField& field,
                                const std::string& app_locale) {
  const FieldType type =
      field.Type().GetAutofillAiType(attribute.type().entity_type());
  // TODO(crbug.com/389625753): Investigate whether only passing the
  // field type is the right choice here. This would for example
  // fail the fill a PASSPORT_NUMBER field that gets a
  // PHONE_HOME_WHOLE_NUMBER classification from regular autofill
  // prediction logic.
  std::u16string value =
      attribute.GetInfo(type, app_locale, field.format_string());
  switch (type) {
    case DRIVERS_LICENSE_REGION:
    case VEHICLE_PLATE_STATE:
      // TODO(crbug.com/389625753): Support countries other than the US.
      return GetStateTextForInput(value, /*country_code=*/"US",
                                  field.max_length(),
                                  /*failure_to_fill=*/nullptr);
    case PASSPORT_NUMBER:
    case DRIVERS_LICENSE_NUMBER:
    case VEHICLE_LICENSE_PLATE:
    case VEHICLE_VIN:
      // Some websites ask for "Last X numbers" of a specific ID number, this
      // logic takes care of returning only the required suffix.
      return MaybeStripPrefix(value, field.max_length());
    default:
      return value;
  }
}

std::u16string GetValueForSelect(const AttributeInstance& attribute,
                                 const AutofillField& field,
                                 const std::string& app_locale,
                                 AddressNormalizer* address_normalizer) {
  const FieldType type =
      field.Type().GetAutofillAiType(attribute.type().entity_type());
  if (IsDateFieldType(type)) {
    return GetValueForDateSelect(attribute, field, app_locale).value_or(u"");
  }
  std::u16string fill_value = GetValueForInput(attribute, field, app_locale);
  if (fill_value.empty()) {
    return u"";
  }

  switch (type) {
    case PASSPORT_ISSUING_COUNTRY:
      return GetCountrySelectControlValue(fill_value, field.options(),
                                          /*failure_to_fill=*/nullptr);
    case DRIVERS_LICENSE_REGION:
    case VEHICLE_PLATE_STATE:
      // TODO(crbug.com/389625753): Support countries other than the US.
      return GetStateSelectControlValue(fill_value, field.options(),
                                        /*country_code=*/"US",
                                        address_normalizer,
                                        /*failure_to_fill=*/nullptr);
    default:
      return GetSelectControlValue(fill_value, field.options(),
                                   /*failure_to_fill=*/nullptr)
          .value_or(u"");
  }
}

}  // namespace

std::vector<const EntityInstance*> GetFillableEntityInstances(
    const AutofillClient& client) {
  const EntityDataManager* const edm = client.GetEntityDataManager();
  if (!edm) {
    return {};
  }

  base::span<const EntityInstance> all_entities = edm->GetEntityInstances();

  DenseSet<EntityType> enabled_types;
  for (EntityType type : DenseSet(all_entities, &EntityInstance::type)) {
    if (MayPerformAutofillAiAction(client, AutofillAiAction::kFilling, type)) {
      enabled_types.insert(type);
    }
  }

  std::vector<const EntityInstance*> enabled_entities;
  enabled_entities.reserve(all_entities.size());
  for (const EntityInstance& entity : all_entities) {
    if (enabled_types.contains(entity.type())) {
      enabled_entities.push_back(&entity);
    }
  }
  return enabled_entities;
}

base::flat_set<FieldGlobalId> GetFieldsFillableByAutofillAi(
    const FormStructure& form,
    const AutofillClient& client) {
  std::vector<const EntityInstance*> entities =
      GetFillableEntityInstances(client);
  if (entities.empty()) {
    return {};
  }

  base::flat_map<
      Section,
      base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>>
      section_to_entity_and_field_and_types =
          RationalizeAndDetermineAttributeTypes(form.fields());

  // Returns true if there is data present that could fill the `field`.
  auto is_fillable = [&](const AutofillField& field) {
    return std::ranges::any_of(entities, [&](const EntityInstance* entity) {
      std::optional<AttributeType> type = GetAttributeTypeForEntityAndField(
          section_to_entity_and_field_and_types, *entity, field);
      return type && entity->attribute(*type).has_value();
    });
  };

  std::vector<FieldGlobalId> fillable_fields;
  for (const std::unique_ptr<AutofillField>& field : form.fields()) {
    if (is_fillable(*field)) {
      fillable_fields.push_back(field->global_id());
    }
  }
  return fillable_fields;
}

std::u16string GetFillValueForEntity(
    const EntityInstance& entity,
    base::span<const AutofillFieldWithAttributeType> fields_and_types,
    const AutofillField& field,
    mojom::ActionPersistence action_persistence,
    const std::string& app_locale,
    AddressNormalizer* address_normalizer) {
  auto attribute = [&]() -> base::optional_ref<const AttributeInstance> {
    auto it = std::ranges::find(fields_and_types, field.global_id(),
                                [](const AutofillFieldWithAttributeType& f) {
                                  return f.field->global_id();
                                });
    if (it == fields_and_types.end()) {
      return std::nullopt;
    }
    AttributeType type = it->type;
    if (type.entity_type() != entity.type()) {
      return std::nullopt;
    }
    return entity.attribute(it->type);
  }();

  if (!attribute) {
    return u"";
  }

  std::u16string fill_value =
      field.IsSelectElement()
          ? GetValueForSelect(*attribute, field, app_locale, address_normalizer)
          : GetValueForInput(*attribute, field, app_locale);

  const bool should_obfuscate =
      action_persistence != mojom::ActionPersistence::kFill &&
      !field.IsSelectElement() && attribute->type().is_obfuscated();

  // TODO(crbug.com/394011769): Investigate whether the obfuscation should
  // should include some of the attribute's value, e.g. the last x characters.
  return should_obfuscate ? GetObfuscatedValue(fill_value) : fill_value;
}

}  // namespace autofill
