// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/autofill_ai/autofill_ai_suggestion_generator.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_utils.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_labels.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// Holds an assignment of AutofillFields to AttributeTypes.
//
// Note that an AutofillField may have multiple AttributeTypes of distinct
// EntityTypes assigned. That is, it may happen that both of the following are
// true:
//   base::Contains(assignment.Find(EntityType(kVehicle)),
//                  {field, AttributeType(kVehicleOwner));
//   base::Contains(assignment.Find(EntityType(kDriversLicense)),
//                  {field, AttributeType(kDriversLicenseName));
class AttributeTypeAssignment {
 public:
  AttributeTypeAssignment(
      base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
      const Section& trigger_section)
      : map_(RationalizeAndDetermineAttributeTypes(fields, trigger_section)) {}

  AttributeTypeAssignment(const AttributeTypeAssignment&) = delete;
  AttributeTypeAssignment& operator=(const AttributeTypeAssignment&) = delete;
  AttributeTypeAssignment(AttributeTypeAssignment&&) = default;
  AttributeTypeAssignment& operator=(AttributeTypeAssignment&&) = default;
  ~AttributeTypeAssignment() = default;

  base::span<const AutofillFieldWithAttributeType> Find(EntityType entity) const
      LIFETIME_BOUND {
    auto it = map_.find(entity);
    if (it == map_.end()) {
      return {};
    }
    return it->second;
  }

 private:
  base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>> map_;
};

base::optional_ref<const AutofillFieldWithAttributeType> FindField(
    base::span<const AutofillFieldWithAttributeType> haystack LIFETIME_BOUND,
    const FieldGlobalId& needle) {
  auto it = std::ranges::find(haystack, needle,
                              [](const AutofillFieldWithAttributeType& f) {
                                return f.field->global_id();
                              });
  return it != haystack.end() ? &*it : nullptr;
}

DenseSet<AttributeType> FindAttributesForField(
    const AttributeTypeAssignment& assignment,
    FieldGlobalId field_id) {
  DenseSet<AttributeType> attributes;
  for (EntityType entity_type : DenseSet<EntityType>::all()) {
    base::span<const AutofillFieldWithAttributeType> fields_with_attributes =
        assignment.Find(entity_type);
    if (base::optional_ref<const AutofillFieldWithAttributeType>
            field_with_attribute =
                FindField(fields_with_attributes, field_id)) {
      attributes.insert(field_with_attribute->type);
    }
  }
  return attributes;
}

// Returns a suggestion to manage AutofillAi data.
Suggestion CreateManageSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_MANAGE_SUGGESTION_MAIN_TEXT),
      SuggestionType::kManageAutofillAi);
  suggestion.icon = Suggestion::Icon::kSettings;
  return suggestion;
}

// Returns a suggestion to "Undo" Autofill.
Suggestion CreateUndoSuggestion() {
  Suggestion suggestion(l10n_util::GetStringUTF16(IDS_AUTOFILL_UNDO_MENU_ITEM),
                        SuggestionType::kUndoOrClear);
  suggestion.icon = Suggestion::Icon::kUndo;
  suggestion.acceptance_a11y_announcement =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_CLEARED_FORM);
  return suggestion;
}

std::vector<Suggestion> GetFooterSuggestions(
    const FormFieldData& trigger_field) {
  std::vector<Suggestion> suggestions;
  suggestions.reserve(3);

  suggestions.emplace_back(SuggestionType::kSeparator);
  if (trigger_field.is_autofilled()) {
    suggestions.emplace_back(CreateUndoSuggestion());
  }
  suggestions.emplace_back(CreateManageSuggestion());
  return suggestions;
}

// Given `entities_to_suggest`, returns, for each entity, a string label to be
// used for generating a `Suggestion` object from that entity.
//
// Labels are supposed to be shown by the UI in the second line of each
// suggestion (not the main text).
//
// Labels consist of the following values, separated by `kLabelSeparator`:
// - The entity name of the corresponding `EntityInstance` in
//   `entities_to_suggest`.
// - Values of some of the entity's attributes.
//
// Ideally, every suggestion is uniquely identifiable by its label. In reality,
// labels may not uniquely identify the underlying entity: for one thing, the
// maximum length of the label is limited; for another, different entities may
// agree on the values of the limited disambiguating attributes.
std::vector<std::u16string> GetLabelsForSuggestions(
    base::span<const EntityInstance> entities_to_suggest,
    base::span<const EntityInstance*> other_entities_that_can_fill_section,
    DenseSet<AttributeType> trigger_field_attributes,
    const std::string& app_locale) {
  std::vector<const EntityInstance*> entities =
      base::ToVector(entities_to_suggest,
                     [](const EntityInstance& entity) { return &entity; });
  entities.insert(entities.end(), other_entities_that_can_fill_section.begin(),
                  other_entities_that_can_fill_section.end());

  std::vector<EntityLabel> labels =
      GetLabelsForEntities(entities, trigger_field_attributes,
                           /*only_disambiguating_types=*/true, app_locale);

  // Drop the labels for the `other_entities_that_can_fill_section`.
  if (labels.size() > entities_to_suggest.size()) {
    labels.resize(entities_to_suggest.size());
  }

  // Prepend the entity type's name to each label.
  for (auto [entity, label] : base::zip(entities, labels)) {
    label.insert(label.begin(),
                 std::u16string(entity->type().GetNameForI18n()));
  }

  // Join the label pieces into a single label with the appropriate separator.
  return base::ToVector(labels, [](const std::vector<std::u16string>& label) {
    return base::JoinString(label, kLabelSeparator);
  });
}

// Returns entities whose set of fields and values to be filled are not subsets
// of another. This function favors server entities, for example if
// two entities (one being local and one being server) are going to fill the
// same fields with the same values, this function will keep the server one.
// Note that `s` is expected to be sorted by descending priority and favor
// higher-priority suggestions.
std::vector<const EntityInstance*> DedupedEntitiesForSuggestions(
    const std::vector<const EntityInstance*>& entities,
    const AttributeTypeAssignment& type_assignment,
    const std::string& app_locale) {
  std::vector<std::vector<std::pair<FieldGlobalId, std::u16string>>>
      fields_to_values(entities.size());
  for (auto [entity, field_to_values] : base::zip(entities, fields_to_values)) {
    for (const auto& [field, attribute_type] :
         type_assignment.Find(entity->type())) {
      base::optional_ref<const AttributeInstance> attribute =
          entity->attribute(attribute_type);
      if (!attribute) {
        continue;
      }
      std::u16string attribute_value =
          attribute->GetInfo(field->Type().GetAutofillAiType(entity->type()),
                             app_locale, field->format_string());
      if (attribute_value.empty()) {
        continue;
      }

      field_to_values.emplace_back(field->global_id(),
                                   std::move(attribute_value));
    }
  }

  auto is_server_entity = [](const EntityInstance& entity) {
    switch (entity.record_type()) {
      case EntityInstance::RecordType::kServerWallet:
        return true;
      case EntityInstance::RecordType::kLocal:
        return false;
    }
    NOTREACHED();
  };
  std::vector<const EntityInstance*> deduped_entities;
  for (size_t i = 0; i < entities.size(); ++i) {
    bool erase_i = false;
    for (size_t j = 0; j < entities.size(); ++j) {
      if (i == j) {
        continue;
      }
      const bool j_includes_i =
          std::ranges::includes(fields_to_values[j], fields_to_values[i]);
      const bool j_equals_i = j_includes_i && fields_to_values[i].size() ==
                                                  fields_to_values[j].size();
      // Erase `i` iff:
      // - `i` is a proper subset of `j` for some `j`.
      // - `i` is equal to `j` and `i` is not a server entity while `j` is.
      // - `i` is equal to `j` for some j < i and `i` is not a server entity.
      // - `i` is equal to `j` for some j < i and both `i` and `j` are server
      // entities.
      const bool i_is_proper_subset_of_j = j_includes_i && !j_equals_i;
      const bool i_is_server_entity = is_server_entity(*entities[i]);
      const bool j_is_server_entity = is_server_entity(*entities[j]);
      const bool i_and_j_are_server_entities =
          i_is_server_entity && j_is_server_entity;
      if (i_is_proper_subset_of_j ||
          (j_equals_i &&
           ((!i_is_server_entity && j_is_server_entity) ||
            (i > j && (!i_is_server_entity || i_and_j_are_server_entities))))) {
        erase_i = true;
        break;
      }
    }

    if (!erase_i) {
      deduped_entities.push_back(entities[i]);
    }
  }
  return deduped_entities;
}

Suggestion::Icon GetSuggestionIcon(EntityType trigger_entity_type) {
  switch (trigger_entity_type.name()) {
    case EntityTypeName::kDriversLicense:
      return Suggestion::Icon::kIdCard;
    case EntityTypeName::kFlightReservation:
      return Suggestion::Icon::kFlight;
    case EntityTypeName::kNationalIdCard:
      return Suggestion::Icon::kIdCard;
    case EntityTypeName::kPassport:
      return Suggestion::Icon::kIdCard;
    case EntityTypeName::kKnownTravelerNumber:
      return Suggestion::Icon::kPersonCheck;
    case EntityTypeName::kRedressNumber:
      return Suggestion::Icon::kPersonCheck;
    case EntityTypeName::kVehicle:
      return Suggestion::Icon::kVehicle;
  }
  NOTREACHED();
}

// Indicates whether `entity` is relevant for suggestion generation.
//
// If so, `entity` is guaranteed to define a non-empty value for
// `trigger_field`'s Autofill AI FieldType.
bool EntityShouldProduceSuggestion(
    const EntityInstance& entity,
    const AutofillFieldWithAttributeType& trigger_field,
    const std::string& app_locale) {
  DCHECK_EQ(entity.type(), trigger_field.type.entity_type());
  base::optional_ref<const AttributeInstance> trigger_attribute =
      entity.attribute(trigger_field.type);
  // Do not create a suggestion if the triggering field cannot be filled.
  if (!trigger_attribute) {
    return false;
  }
  std::u16string trigger_value = trigger_attribute->GetInfo(
      trigger_field.field->Type().GetAutofillAiType(entity.type()), app_locale,
      trigger_field.field->format_string());
  if (trigger_value.empty()) {
    return false;
  }

  // Obfuscated types are not prefix matched to avoid that a webpage can
  // use the existence of suggestions to guess a user's data.
  if (!trigger_field.type.is_obfuscated()) {
    const std::u16string normalized_attribute =
        normalization::NormalizeForComparison(trigger_value);
    const std::u16string normalized_field_content =
        normalization::NormalizeForComparison(trigger_field.field->value());
    if (!normalized_attribute.starts_with(normalized_field_content)) {
      return false;
    }
  }
  return true;
}

// Returns true if `entity` has a non-empty value to fill for some field of
// `section` in `fields`.
//
// The AttributeTypes of `fields` must all belong to `entity`.
bool CanFillSomeField(const EntityInstance& entity,
                      base::span<const AutofillFieldWithAttributeType> fields,
                      const std::string& app_locale) {
  return std::ranges::any_of(
      fields, [&](const AutofillFieldWithAttributeType& f) {
        DCHECK_EQ(entity.type(), f.type.entity_type());
        base::optional_ref<const AttributeInstance> attribute =
            entity.attribute(f.type);
        return attribute &&
               !attribute
                    ->GetInfo(f.field->Type().GetAutofillAiType(entity.type()),
                              app_locale, f.field->format_string())
                    .empty();
      });
}

Suggestion GetSuggestionForEntity(
    const EntityInstance& entity,
    base::span<const AutofillFieldWithAttributeType> fields,
    const AutofillFieldWithAttributeType& trigger_field,
    std::u16string label,
    const std::string& app_locale) {
  // The dereference is guaranteed by EntityShouldProduceSuggestion().
  const AttributeInstance& trigger_attribute =
      *entity.attribute(trigger_field.type);
  std::u16string main_text = trigger_attribute.GetInfo(
      trigger_field.field->Type().GetAutofillAiType(
          trigger_attribute.type().entity_type()),
      app_locale, trigger_field.field->format_string());
  Suggestion suggestion =
      Suggestion(std::move(main_text), SuggestionType::kFillAutofillAi);
  suggestion.labels = {{Suggestion::Text(std::move(label))}};
  suggestion.payload = Suggestion::AutofillAiPayload(entity.guid());
  suggestion.icon = GetSuggestionIcon(entity.type());
  if (entity.record_type() == EntityInstance::RecordType::kServerWallet) {
    suggestion.iph_metadata = Suggestion::IPHMetadata(
        &feature_engagement::kIPHAutofillAiValuablesFeature);
  }
  return suggestion;
}

// The desired ordering criteria are the following:
// - Entities of the same type should appear together.
// - Entities of type A should appear before entities of type B if the most
//   "frecent" entity of type A is more frecent than the most frecent entity of
//   type B.
//
// In other terms, entities are grouped so that the most “frecent” suggestion
// will be shown first, then all suggestions of the same type, then the next
// most “frecent” suggestion, and so on.
std::vector<const EntityInstance*> OrderedEntitiesForSuggestion(
    std::vector<const EntityInstance*> entities) {
  // Sort entities based on their frecency.
  std::ranges::sort(entities,
                    [comp = EntityInstance::FrecencyOrder(base::Time::Now())](
                        const EntityInstance* lhs, const EntityInstance* rhs) {
                      return comp(*lhs, *rhs);
                    });
  // Group entities based on their entity type. Note that by doing so after the
  // first sorting step, it is guaranteed that each individual vector in the map
  // is also sorted accordingly.
  std::map<EntityType, std::vector<const EntityInstance*>>
      sorted_entities_by_type;
  for (const EntityInstance* entity : entities) {
    sorted_entities_by_type[entity->type()].push_back(entity);
  }

  std::vector<const EntityInstance*> sorted_entities;
  sorted_entities.reserve(entities.size());
  // By iterating over `entities`, sorted by frecency, the desired ordering is
  // achieved.
  for (const EntityInstance* entity : entities) {
    base::Extend(sorted_entities,
                 std::move(sorted_entities_by_type[entity->type()]));
    sorted_entities_by_type[entity->type()].clear();
  }
  return sorted_entities;
}

std::vector<const EntityInstance*> GetEntitiesForSuggestion(
    std::vector<const EntityInstance*> entities,
    const AttributeTypeAssignment& assignment,
    const FieldGlobalId& trigger_field_id,
    const std::string& app_locale) {
  std::erase_if(entities, [&](const EntityInstance* entity) {
    base::optional_ref<const AutofillFieldWithAttributeType>
        trigger_field_with_type =
            FindField(assignment.Find(entity->type()), trigger_field_id);
    return !trigger_field_with_type ||
           !EntityShouldProduceSuggestion(*entity, *trigger_field_with_type,
                                          app_locale);
  });
  return DedupedEntitiesForSuggestions(
      OrderedEntitiesForSuggestion(std::move(entities)), assignment,
      app_locale);
}

std::vector<Suggestion> CreateAutofillAiFillingSuggestions(
    const FormStructure& form,
    const FormFieldData& trigger_field_data,
    base::span<const EntityInstance> entities_to_suggest,
    base::span<const EntityInstance> all_entities,
    const AttributeTypeAssignment& assignment,
    const std::string& app_locale) {
  CHECK(!entities_to_suggest.empty());
  const AutofillField& trigger_field =
      CHECK_DEREF(form.GetFieldById(trigger_field_data.global_id()));

  auto entities_to_suggest_ids = base::MakeFlatSet<EntityInstance::EntityId>(
      entities_to_suggest, {}, &EntityInstance::guid);

  // Labels need to be consistent across the whole fill group. That is, as the
  // user clicks around fields they need to see the same set of attributes as a
  // combination of main text and labels. Therefore, entities that do not
  // generate suggestions on a certain triggering field still affect label
  // generation and should be taken into account.
  std::vector<const EntityInstance*> other_entities_that_can_fill_section;
  for (const EntityInstance& entity : all_entities) {
    if (!entities_to_suggest_ids.contains(entity.guid()) &&
        CanFillSomeField(entity, assignment.Find(entity.type()), app_locale)) {
      other_entities_that_can_fill_section.push_back(&entity);
    }
  }

  std::vector<std::u16string> labels = GetLabelsForSuggestions(
      entities_to_suggest, other_entities_that_can_fill_section,
      FindAttributesForField(assignment, trigger_field.global_id()),
      app_locale);

  std::vector<Suggestion> suggestions;
  suggestions.reserve(entities_to_suggest.size());
  CHECK_EQ(entities_to_suggest.size(), labels.size());
  for (auto [entity, label] : base::zip(entities_to_suggest, labels)) {
    base::span<const AutofillFieldWithAttributeType> fields_with_types =
        assignment.Find(entity.type());
    base::optional_ref<const AutofillFieldWithAttributeType>
        trigger_field_with_type =
            FindField(fields_with_types, trigger_field.global_id());
    suggestions.push_back(GetSuggestionForEntity(entity, fields_with_types,
                                                 *trigger_field_with_type,
                                                 std::move(label), app_locale));
  }

  base::Extend(suggestions, GetFooterSuggestions(trigger_field_data));
  return suggestions;
}

}  // namespace

AutofillAiSuggestionGenerator::AutofillAiSuggestionGenerator() = default;
AutofillAiSuggestionGenerator::~AutofillAiSuggestionGenerator() = default;

void AutofillAiSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::OnceCallback<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  FetchSuggestionData(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      [&callback](std::pair<SuggestionDataSource,
                            std::vector<SuggestionGenerator::SuggestionData>>
                      suggestion_data) {
        std::move(callback).Run(std::move(suggestion_data));
      });
}

void AutofillAiSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      all_suggestion_data,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void AutofillAiSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::FunctionRef<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  const EntityDataManager* entity_manager = client.GetEntityDataManager();
  if (!entity_manager || !form_structure || !trigger_autofill_field) {
    callback({SuggestionDataSource::kAutofillAi, {}});
    return;
  }

  if (!GetFieldsFillableByAutofillAi(*form_structure, client)
           .contains(trigger_field.global_id()) ||
      SuppressSuggestionsForAutocompleteUnrecognizedField(
          *trigger_autofill_field)) {
    callback({SuggestionDataSource::kAutofillAi, {}});
    return;
  }

  std::vector<const EntityInstance*> entities = GetEntitiesForSuggestion(
      GetFillableEntityInstances(client),
      AttributeTypeAssignment(form_structure->fields(),
                              trigger_autofill_field->section()),
      trigger_field.global_id(), client.GetAppLocale());

  std::vector<SuggestionData> suggestion_data = base::ToVector(
      std::move(entities),
      [](const EntityInstance* entity) { return SuggestionData(*entity); });

  callback({SuggestionDataSource::kAutofillAi, std::move(suggestion_data)});
}

void AutofillAiSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  const EntityDataManager* entity_manager = client.GetEntityDataManager();
  if (!entity_manager || !form_structure || !trigger_autofill_field) {
    callback({FillingProduct::kAutofillAi, {}});
    return;
  }

  auto it = all_suggestion_data.find(SuggestionDataSource::kAutofillAi);
  std::vector<SuggestionData> autofill_ai_suggestion_data =
      it != all_suggestion_data.end() ? it->second
                                      : std::vector<SuggestionData>();
  if (autofill_ai_suggestion_data.empty()) {
    callback({FillingProduct::kAutofillAi, {}});
    return;
  }

  std::vector<EntityInstance> entities_to_suggest = base::ToVector(
      std::move(autofill_ai_suggestion_data),
      [](SuggestionData& suggestion_data) {
        return std::get<EntityInstance>(std::move(suggestion_data));
      });
  std::vector<Suggestion> suggestions = CreateAutofillAiFillingSuggestions(
      *form_structure, *trigger_autofill_field, entities_to_suggest,
      entity_manager->GetEntityInstances(),
      AttributeTypeAssignment(form_structure->fields(),
                              trigger_autofill_field->section()),
      client.GetAppLocale());
  callback({FillingProduct::kAutofillAi, std::move(suggestions)});
}

}  // namespace autofill
