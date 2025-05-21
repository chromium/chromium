// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/suggestion/autofill_ai_suggestions.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_ai/core/browser/autofill_ai_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_ai {

namespace {

using autofill::AttributeInstance;
using autofill::AttributeType;
using autofill::AutofillField;
using autofill::FieldGlobalId;
using autofill::FieldType;
using autofill::Suggestion;
using autofill::SuggestionType;

struct SuggestionWithMetadata {
  SuggestionWithMetadata(
      Suggestion suggestion,
      raw_ref<const autofill::EntityInstance> entity,
      base::flat_map<FieldGlobalId, std::u16string> field_to_value)
      : suggestion(std::move(suggestion)),
        entity(entity),
        field_to_value(std::move(field_to_value)) {}

  // A suggestion whose payload is of type `Suggestion::AutofillAiPayload`.
  Suggestion suggestion;

  // The entity used to build `suggestion`.
  raw_ref<const autofill::EntityInstance> entity;

  // The values that would be filled by `suggestion`, indexed by the underlying
  // field's ID.
  base::flat_map<FieldGlobalId, std::u16string> field_to_value;
};

// For each suggestion in `suggestions`, create its label.
// `labels_for_all_suggestions` contain for each suggestion all the strings that
// should be concatenated to generate the final label.
std::vector<Suggestion> AssignLabelsToSuggestions(
    EntitiesLabels labels_for_all_suggestions,
    std::vector<Suggestion> suggestions) {
  CHECK_EQ(labels_for_all_suggestions->size(), suggestions.size());

  size_t suggestion_index = 0;
  for (Suggestion& suggestion : suggestions) {
    suggestion.labels.push_back({Suggestion::Text(base::JoinString(
        (*labels_for_all_suggestions)[suggestion_index], kLabelSeparator))});
    suggestion_index++;
  }

  return suggestions;
}

// Returns all labels that can be used to disambiguate a list of entities used
// to build suggestions shown to a user, present in `suggestions_with_metadata`.
// The vector of labels for each suggestion is sorted from highest to lowest
// priority and only contain values that will be added to the second line of the
// suggestion UI (so not the main text). The available labels are generated
// based on the values held by the entity used to create a certain suggestion
// and also `other_entities_that_can_fill_form`.
// `other_entities_that_can_fill_form` while not being used by a suggestion, is
// used here because the values used to generate labels should be consistent in
// all fields in the `triggering_field_type` filling group.
//
// This function retrieves the available labels by doing the following:
//
// 1. Retrieves the list of entities used to build each suggestion
// (`SuggestionWithMetadata::entity`) and concatenates
// `other_entities_that_can_fill_form` to it.
//
// 2. Calls `GetLabelsForEntities()`, making sure use the
//    `triggering_field_attribute` as the attribute type to exclude from the
//    possible labels, since it will already be part of the suggestion main
//    text.
EntitiesLabels GetLabelsForSuggestions(
    base::span<const SuggestionWithMetadata> suggestions_with_metadata,
    base::span<const autofill::EntityInstance*>
        other_entities_that_can_fill_form,
    const std::string& app_locale) {
  CHECK(!suggestions_with_metadata.empty());
  std::vector<const autofill::EntityInstance*> entities =
      base::ToVector(suggestions_with_metadata,
                     [](SuggestionWithMetadata suggestion_with_metadata) {
                       return &suggestion_with_metadata.entity.get();
                     });
  entities.insert(entities.end(), other_entities_that_can_fill_form.begin(),
                  other_entities_that_can_fill_form.end());

  // Note that `all_entities_labels` are for all entities, including
  // that were not used in suggestions generation.
  EntitiesLabels all_entities_labels =
      GetLabelsForEntities(entities, /*allow_only_disambiguating_types=*/true,
                           /*return_at_least_one_label=*/false, app_locale);
  // Returns only the first N labels for entity, which refers to the N
  // suggestion in `suggestions_with_metadata`.
  return EntitiesLabels(std::vector<std::vector<std::u16string>>(
      all_entities_labels->begin(),
      all_entities_labels->begin() + suggestions_with_metadata.size()));
}

// Generates suggestions with labels given `suggestions_with_metadata` (which
// holds both the suggestions and their respective entities), triggering field
// of `AttributeType` and `other_entities_that_can_fill_form`. This function
// works as follows:
//
// 1. Initializes the output (a vector of suggestions) and its default labels.
//    The labels at this point are all the same (single string with the entity
//    name).
//
// 2. Get the disambiguating attributes to be used. Note that we take into
// account both the entities
//    Used to generate the suggestions and any other entities that can fill
//    other fields in the form (`other_entities_that_can_fill_form`). This is
//    because we want the labels to be consistent across all fields in the
//    filling group.
//
// 4. Assigns the labels acquired in step 2# and return the updated suggestions.
std::vector<Suggestion> GenerateFillingSuggestionWithLabels(
    AttributeType triggering_field_attribute,
    std::vector<SuggestionWithMetadata> suggestions_with_metadata,
    base::span<const autofill::EntityInstance*>
        other_entities_that_can_fill_form,
    const std::string& app_locale) {
  // Step 1#
  const size_t n_suggestions = suggestions_with_metadata.size();
  // Initialize the output using `suggestions_with_metadata`.
  std::vector<Suggestion> suggestions_with_labels;
  suggestions_with_labels.reserve(n_suggestions);
  for (SuggestionWithMetadata& s : suggestions_with_metadata) {
    suggestions_with_labels.push_back(std::move(s.suggestion));
  }

  // Initialize the final list of labels to be used by each suggestion. Note
  // that they always contain at least the entity name.
  EntitiesLabels suggestions_labels =
      EntitiesLabels(std::vector<std::vector<std::u16string>>(
          n_suggestions,
          {std::u16string(
              triggering_field_attribute.entity_type().GetNameForI18n())}));

  // Step 2#
  // Get the list of disambiguating labels, the list is created for the entities
  // used to build the suggestions, but it also uses other entity that can fill
  // a field in the form.
  EntitiesLabels disambiguating_labels_for_all_entities_that_fill_form =
      GetLabelsForSuggestions(suggestions_with_metadata,
                              other_entities_that_can_fill_form, app_locale);

  for (size_t i = 0; i < suggestions_labels->size(); i++) {
    std::vector<std::u16string>& suggestion_labels = (*suggestions_labels)[i];
    std::vector<std::u16string>& disambiguator_labels_for_suggestion =
        (*disambiguating_labels_for_all_entities_that_fill_form)[i];
    // Do not assign labels that are identical or derived from the triggering
    // field, as they are redundant.
    raw_ref<const autofill::EntityInstance> entity =
        suggestions_with_metadata[i].entity;
    base::optional_ref<const AttributeInstance> attribute =
        entity->attribute(triggering_field_attribute);
    // The entity used to build the suggestion should be able to fill the
    // triggering field.
    CHECK(attribute);
    std::erase_if(disambiguator_labels_for_suggestion,
                  [&](const std::u16string& label) {
                    return label == attribute->GetCompleteInfo(app_locale);
                  });

    suggestion_labels.insert(suggestion_labels.end(),
                             disambiguator_labels_for_suggestion.begin(),
                             disambiguator_labels_for_suggestion.end());
  }
  // Step 3#
  // Assign the labels.
  return AssignLabelsToSuggestions(std::move(suggestions_labels),
                                   std::move(suggestions_with_labels));
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

// Returns suggestions whose set of fields and values to be filled are not
// subsets of another.
std::vector<SuggestionWithMetadata> DedupeFillingSuggestions(
    std::vector<SuggestionWithMetadata> s) {
  for (auto it = s.cbegin(); it != s.cend();) {
    // Erase `it` iff
    // - `it` fills a proper subset of `jt` or
    // - `it` fills the same values as `jt` and comes before `jt` in `s`.
    bool erase_it = false;
    for (auto jt = s.cbegin(); !erase_it && jt != s.cend(); ++jt) {
      erase_it |= it != jt &&
                  std::ranges::includes(jt->field_to_value, it->field_to_value);
    }
    it = erase_it ? s.erase(it) : it + 1;
  }
  return s;
}

Suggestion::Icon GetSuggestionIcon(
    autofill::EntityType triggering_field_entity_type) {
  switch (triggering_field_entity_type.name()) {
    case autofill::EntityTypeName::kPassport:
      return Suggestion::Icon::kIdCard;
    case autofill::EntityTypeName::kDriversLicense:
      return Suggestion::Icon::kIdCard;
    case autofill::EntityTypeName::kVehicle:
      return Suggestion::Icon::kVehicle;
  }
  NOTREACHED();
}

}  // namespace

std::vector<Suggestion> CreateFillingSuggestions(
    const autofill::FormStructure& form,
    FieldGlobalId field_global_id,
    base::span<const autofill::EntityInstance> entities,
    const std::string& app_locale) {
  const AutofillField* autofill_field = form.GetFieldById(field_global_id);
  CHECK(autofill_field);

  const std::optional<FieldType> trigger_field_autofill_ai_type =
      autofill_field->GetAutofillAiServerTypePredictions();
  CHECK(trigger_field_autofill_ai_type);
  const std::optional<AttributeType> trigger_field_attribute_type =
      AttributeType::FromFieldType(*trigger_field_autofill_ai_type);
  // The triggering field should be of `FieldTypeGroup::kAutofillAi`
  // type and therefore mapping it to an `AttributeType` should always
  // return a value.
  CHECK(trigger_field_attribute_type);

  const FieldType trigger_field_autofill_type =
      autofill_field->Type().GetStorableType();

  // Suggestion and their fields to be filled metadata.
  std::vector<SuggestionWithMetadata> suggestions_with_metadata;
  // Used to know whether any other entity can fill the current fill group.
  autofill::DenseSet<AttributeType> attribute_types_in_form;
  std::set<base::Uuid> entities_used_to_build_suggestions;

  // Sort entities based on their frecency.
  std::vector<const autofill::EntityInstance*> sorted_entities = base::ToVector(
      entities, [](const autofill::EntityInstance& entity) { return &entity; });
  autofill::EntityInstance::RankingOrder comp(base::Time::Now());
  std::ranges::sort(sorted_entities, [&](const autofill::EntityInstance* lhs,
                                         const autofill::EntityInstance* rhs) {
    return comp(*lhs, *rhs);
  });

  for (const autofill::EntityInstance* entity : sorted_entities) {
    //  Only entities that match the triggering field entity should be used to
    //  generate suggestions.
    if (entity->type() != trigger_field_attribute_type->entity_type()) {
      continue;
    }
    base::optional_ref<const AttributeInstance> attribute_for_triggering_field =
        entity->attribute(*trigger_field_attribute_type);
    // Do not create suggestion if the triggering field cannot be filled.
    if (!attribute_for_triggering_field ||
        attribute_for_triggering_field
            ->GetInfo(trigger_field_autofill_type, app_locale, std::nullopt)
            .empty()) {
      continue;
    }

    // Obfuscated types are not prefix matched to avoid that a webpage can
    // use the existence of suggestions to guess a user's data.
    if (!trigger_field_attribute_type->is_obfuscated()) {
      const std::u16string normalized_attribute =
          autofill::AutofillProfileComparator::NormalizeForComparison(
              attribute_for_triggering_field->GetInfo(
                  trigger_field_autofill_type, app_locale,
                  autofill_field->format_string()));
      const std::u16string normalized_field_content =
          autofill::AutofillProfileComparator::NormalizeForComparison(
              autofill_field->value());
      if (!normalized_attribute.starts_with(normalized_field_content)) {
        continue;
      }
    }

    std::vector<std::pair<FieldGlobalId, std::u16string>> field_to_value;
    for (const std::unique_ptr<AutofillField>& field : form.fields()) {
      // Only fill fields that match the triggering field section.
      if (field->section() != autofill_field->section()) {
        continue;
      }
      std::optional<FieldType> field_autofill_ai_prediction =
          field->GetAutofillAiServerTypePredictions();
      if (!field_autofill_ai_prediction) {
        continue;
      }

      std::optional<AttributeType> attribute_type =
          AttributeType::FromFieldType(*field_autofill_ai_prediction);
      // Only fields that match the triggering field entity should be used to
      // generate suggestions.
      if (!attribute_type || trigger_field_attribute_type->entity_type() !=
                                 attribute_type->entity_type()) {
        continue;
      }

      attribute_types_in_form.insert(*attribute_type);
      base::optional_ref<const AttributeInstance> attribute =
          entity->attribute(*attribute_type);
      if (!attribute) {
        continue;
      }

      const std::u16string attribute_value =
          attribute->GetInfo(field->Type().GetStorableType(), app_locale,
                             autofill_field->format_string());

      if (attribute_value.empty()) {
        continue;
      }

      field_to_value.emplace_back(field->global_id(), attribute_value);
    }

    // Retrieve all entity values, this will be used to generate labels.
    std::vector<std::pair<AttributeType, std::u16string>>
        attribute_type_to_value;
    for (const AttributeInstance& attribute : entity->attributes()) {
      const std::u16string full_attribute_value =
          attribute.GetCompleteInfo(app_locale);
      if (full_attribute_value.empty()) {
        continue;
      }
      attribute_type_to_value.emplace_back(attribute.type(),
                                           full_attribute_value);
    }

    Suggestion suggestion =
        Suggestion(attribute_for_triggering_field->GetInfo(
                       autofill_field->Type().GetStorableType(), app_locale,
                       autofill_field->format_string()),
                   SuggestionType::kFillAutofillAi);
    suggestion.payload = Suggestion::AutofillAiPayload(entity->guid());
    suggestion.icon =
        GetSuggestionIcon(trigger_field_attribute_type->entity_type());
    suggestions_with_metadata.emplace_back(
        suggestion, raw_ref(*entity),
        base::flat_map(std::move(field_to_value)));
    entities_used_to_build_suggestions.insert(entity->guid());
  }

  if (suggestions_with_metadata.empty()) {
    return {};
  }

  // Labels need to be consistent across the whole fill group. Meaning, as the
  // user clicks around fields they need to see the same set attributes as a
  // combination of main text and labels. Therefore entities that does not
  // generate suggestions on a certain triggering field, still affect label
  // generation and should be taken into account.
  std::vector<const autofill::EntityInstance*>
      other_entities_that_can_fill_form;
  for (const autofill::EntityInstance* entity : sorted_entities) {
    // Do not add if it is already part of a suggestion
    if (entities_used_to_build_suggestions.contains(entity->guid())) {
      continue;
    }

    // Do not add if the entity does not match the triggering field type.
    if (entity->type() != trigger_field_attribute_type->entity_type()) {
      continue;
    }
    const bool can_entity_fill_any_field_in_form = std::ranges::any_of(
        attribute_types_in_form, [&](const AttributeType attribute) {
          base::optional_ref<const AttributeInstance> instance =
              entity->attribute(attribute);
          // If the entity can fill any field in the form, add it.
          return instance && !instance
                                  ->GetInfo(attribute.field_type(), app_locale,
                                            std::nullopt)
                                  .empty();
        });
    if (can_entity_fill_any_field_in_form) {
      other_entities_that_can_fill_form.push_back(entity);
    }
  }

  std::vector<Suggestion> suggestions = GenerateFillingSuggestionWithLabels(
      *trigger_field_attribute_type,
      DedupeFillingSuggestions(std::move(suggestions_with_metadata)),
      other_entities_that_can_fill_form, app_locale);

  // Footer suggestions.
  suggestions.emplace_back(SuggestionType::kSeparator);
  if (autofill_field->is_autofilled()) {
    suggestions.emplace_back(CreateUndoSuggestion());
  }
  suggestions.emplace_back(CreateManageSuggestion());
  return suggestions;
}

}  // namespace autofill_ai
