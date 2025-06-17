// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_suggestions.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
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
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_utils.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

struct SuggestionWithMetadata {
  SuggestionWithMetadata(
      Suggestion suggestion,
      raw_ref<const EntityInstance> entity,
      base::flat_map<FieldGlobalId, std::u16string> field_to_value)
      : suggestion(std::move(suggestion)),
        entity(entity),
        field_to_value(std::move(field_to_value)) {}

  // A suggestion whose payload is of type `Suggestion::AutofillAiPayload`.
  Suggestion suggestion;

  // The entity used to build `suggestion`.
  raw_ref<const EntityInstance> entity;

  // The values that would be filled by `suggestion`, indexed by the underlying
  // field's ID.
  base::flat_map<FieldGlobalId, std::u16string> field_to_value;
};

// For each suggestion in `suggestions`, create its label.
// `labels_for_all_suggestions` contain for each suggestion all the strings that
// should be concatenated to generate the final label.
std::vector<Suggestion> AssignLabelsToSuggestions(
    std::vector<EntityLabel> labels_for_all_suggestions,
    std::vector<Suggestion> suggestions) {
  CHECK_EQ(labels_for_all_suggestions.size(), suggestions.size());

  size_t suggestion_index = 0;
  for (Suggestion& suggestion : suggestions) {
    suggestion.labels.push_back({Suggestion::Text(base::JoinString(
        labels_for_all_suggestions[suggestion_index], kLabelSeparator))});
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
// and also `other_entities_that_can_fill_section`.
// `other_entities_that_can_fill_section` while not being used by a suggestion,
// is used here because the values used to generate labels should be consistent
// in all fields in the `triggering_field_type` filling group.
//
// This function retrieves the available labels by doing the following:
//
// 1. Retrieves the list of entities used to build each suggestion
//    (`SuggestionWithMetadata::entity`) and concatenates
//    `other_entities_that_can_fill_section` to it.
//
// 2. Calls `GetLabelsForEntities()`, making sure use the
//    `triggering_field_attribute` as the attribute type to exclude from the
//    possible labels, since it will already be part of the suggestion main
//    text.
std::vector<EntityLabel> GetLabelsForSuggestions(
    base::span<const SuggestionWithMetadata> suggestions_with_metadata,
    base::span<const EntityInstance*> other_entities_that_can_fill_section,
    const std::string& app_locale) {
  CHECK(!suggestions_with_metadata.empty());
  std::vector<const EntityInstance*> entities = base::ToVector(
      suggestions_with_metadata,
      [](const SuggestionWithMetadata& suggestion_with_metadata) {
        return &suggestion_with_metadata.entity.get();
      });
  entities.insert(entities.end(), other_entities_that_can_fill_section.begin(),
                  other_entities_that_can_fill_section.end());

  // Note that `all_entities_labels` are for all entities, including
  // those that were not used in suggestion generation.
  std::vector<EntityLabel> all_entities_labels =
      GetLabelsForEntities(entities, /*allow_only_disambiguating_types=*/true,
                           /*return_at_least_one_label=*/false, app_locale);
  if (all_entities_labels.size() > suggestions_with_metadata.size()) {
    all_entities_labels.resize(suggestions_with_metadata.size());
  }
  return all_entities_labels;
}

// Generates suggestions with labels given `suggestions_with_metadata` (which
// holds both the suggestions and their respective entities), triggering field
// of `AttributeType` and `other_entities_that_can_fill_section`. This function
// works as follows:
//
// 1. Initializes the output (a vector of suggestions) and its default labels.
//    The labels at this point are all the same (single string with the entity
//    name).
//
// 2. Get the disambiguating attributes to be used. Note that we take into
//    account both the entities used to generate the suggestions and any other
//    entities that can fill other fields in the form
//    (`other_entities_that_can_fill_section`). This is because we want the
//    labels to be consistent across all fields in the filling group.
//
// 4. Assigns the labels acquired in step #2 and return the updated suggestions.
std::vector<Suggestion> GenerateFillingSuggestionWithLabels(
    AttributeType trigger_attribute,
    std::vector<SuggestionWithMetadata> suggestions_with_metadata,
    base::span<const EntityInstance*> other_entities_that_can_fill_section,
    const std::string& app_locale) {
  // Step #1
  const size_t num_suggestions = suggestions_with_metadata.size();
  // Initialize the output using `suggestions_with_metadata`.
  std::vector<Suggestion> suggestions_with_labels;
  suggestions_with_labels.reserve(num_suggestions);
  for (SuggestionWithMetadata& s : suggestions_with_metadata) {
    suggestions_with_labels.push_back(std::move(s.suggestion));
  }

  // Step #2
  // Get the list of disambiguating labels. The list is created for the entities
  // used to build the suggestions, but it also uses other entities that can
  // fill a field in the form.
  std::vector<EntityLabel> suggestions_labels =
      GetLabelsForSuggestions(suggestions_with_metadata,
                              other_entities_that_can_fill_section, app_locale);

  for (size_t i = 0; i < num_suggestions; i++) {
    // Do not assign labels that are identical or derived from the triggering
    // field, as they are redundant.
    const EntityInstance& entity = *suggestions_with_metadata[i].entity;
    base::optional_ref<const AttributeInstance> attribute =
        entity.attribute(trigger_attribute);
    // The entity used to build the suggestion must be able to fill the
    // triggering field.
    CHECK(attribute);

    EntityLabel& disambiguating_labels = suggestions_labels[i];
    std::erase_if(disambiguating_labels, [&](const std::u16string& label) {
      return label == attribute->GetCompleteInfo(app_locale);
    });

    disambiguating_labels.insert(
        disambiguating_labels.begin(),
        std::u16string(entity.type().GetNameForI18n()));
  }

  // Step #3:
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

Suggestion::Icon GetSuggestionIcon(EntityType trigger_entity_type) {
  switch (trigger_entity_type.name()) {
    case EntityTypeName::kPassport:
      return Suggestion::Icon::kIdCard;
    case EntityTypeName::kDriversLicense:
      return Suggestion::Icon::kIdCard;
    case EntityTypeName::kVehicle:
      return Suggestion::Icon::kVehicle;
  }
  NOTREACHED();
}

// Indicates whether `entity` is relevant for suggestion generation.
//
// If so, `entity` is guaranteed to define a non-empty value for
// `trigger_field`'s Autofill AI FieldType.
bool EntityShouldProduceSuggestion(const AutofillField& trigger_field,
                                   const EntityInstance& entity,
                                   const std::string& app_locale) {
  const std::optional<FieldType> trigger_autofill_ai_field_type =
      trigger_field.GetAutofillAiServerTypePredictions();
  if (!trigger_autofill_ai_field_type) {
    return false;
  }
  const std::optional<AttributeType> trigger_attribute_type =
      AttributeType::FromFieldType(*trigger_autofill_ai_field_type);
  if (!trigger_attribute_type) {
    return false;
  }

  // Only entities that match the triggering field entity should be used to
  // generate suggestions.
  if (entity.type() != trigger_attribute_type->entity_type()) {
    return false;
  }
  base::optional_ref<const AttributeInstance> trigger_attribute =
      entity.attribute(*trigger_attribute_type);
  // Do not create a suggestion if the triggering field cannot be filled.
  if (!trigger_attribute) {
    return false;
  }
  std::u16string trigger_value =
      trigger_attribute->GetInfo(trigger_field.Type().GetStorableType(),
                                 app_locale, trigger_field.format_string());
  if (trigger_value.empty()) {
    return false;
  }

  // Obfuscated types are not prefix matched to avoid that a webpage can
  // use the existence of suggestions to guess a user's data.
  if (!trigger_attribute_type->is_obfuscated()) {
    const std::u16string normalized_attribute =
        AutofillProfileComparator::NormalizeForComparison(trigger_value);
    const std::u16string normalized_field_content =
        AutofillProfileComparator::NormalizeForComparison(
            trigger_field.value());
    if (!normalized_attribute.starts_with(normalized_field_content)) {
      return false;
    }
  }
  return true;
}

// Returns true if `entity` has a non-empty value to fill for some field of
// `section` in `form`.
bool CanFillSomeField(const EntityInstance& entity,
                      const FormStructure& form,
                      const Section& section,
                      const std::string& app_locale) {
  return std::ranges::any_of(
      form.fields(), [&](const std::unique_ptr<AutofillField>& field) {
        // Only fill fields that match the triggering field section.
        if (field->section() != section) {
          return false;
        }
        std::optional<FieldType> field_autofill_ai_prediction =
            field->GetAutofillAiServerTypePredictions();
        if (!field_autofill_ai_prediction) {
          return false;
        }
        std::optional<AttributeType> attribute_type =
            AttributeType::FromFieldType(*field_autofill_ai_prediction);
        // Only fields that match the triggering field entity should be used to
        // generate suggestions.
        if (!attribute_type || entity.type() != attribute_type->entity_type()) {
          return false;
        }
        base::optional_ref<const AttributeInstance> attribute =
            entity.attribute(*attribute_type);
        return attribute && !attribute
                                 ->GetInfo(field->Type().GetStorableType(),
                                           app_locale, field->format_string())
                                 .empty();
      });
}

SuggestionWithMetadata GetSuggestionForEntity(
    const FormStructure& form,
    const AutofillField& trigger_field,
    const EntityInstance& entity,
    const std::string& app_locale) {
  DCHECK(EntityShouldProduceSuggestion(trigger_field, entity, app_locale));
  // The dereferences are guaranteed by EntityShouldProduceSuggestion().
  const AttributeInstance& trigger_attribute =
      *entity.attribute(*AttributeType::FromFieldType(
          *trigger_field.GetAutofillAiServerTypePredictions()));

  std::vector<std::pair<FieldGlobalId, std::u16string>> field_to_value;
  for (const std::unique_ptr<AutofillField>& field : form.fields()) {
    // Only fill fields that match the triggering field section.
    if (field->section() != trigger_field.section()) {
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
    if (!attribute_type || entity.type() != attribute_type->entity_type()) {
      continue;
    }

    base::optional_ref<const AttributeInstance> attribute =
        entity.attribute(*attribute_type);
    if (!attribute) {
      continue;
    }

    std::u16string attribute_value = attribute->GetInfo(
        field->Type().GetStorableType(), app_locale, field->format_string());

    if (attribute_value.empty()) {
      continue;
    }

    field_to_value.emplace_back(field->global_id(), std::move(attribute_value));
  }

  // Retrieve all entity values used to generate labels later on.
  std::vector<std::pair<AttributeType, std::u16string>> attribute_type_to_value;
  for (const AttributeInstance& attribute : entity.attributes()) {
    std::u16string full_attribute_value = attribute.GetCompleteInfo(app_locale);
    if (full_attribute_value.empty()) {
      continue;
    }
    attribute_type_to_value.emplace_back(attribute.type(),
                                         std::move(full_attribute_value));
  }

  Suggestion suggestion = Suggestion(
      trigger_attribute.GetInfo(trigger_field.Type().GetStorableType(),
                                app_locale, trigger_field.format_string()),
      SuggestionType::kFillAutofillAi);
  suggestion.payload = Suggestion::AutofillAiPayload(entity.guid());
  suggestion.icon = GetSuggestionIcon(entity.type());
  return SuggestionWithMetadata(suggestion, raw_ref(entity),
                                base::flat_map(std::move(field_to_value)));
}

}  // namespace

std::vector<Suggestion> CreateFillingSuggestions(
    const FormStructure& form,
    const FormFieldData& trigger_field_data,
    base::span<const EntityInstance> entities,
    const std::string& app_locale) {
  const AutofillField* trigger_field =
      form.GetFieldById(trigger_field_data.global_id());
  CHECK(trigger_field);

  const std::optional<FieldType> trigger_autofill_ai_type =
      trigger_field->GetAutofillAiServerTypePredictions();
  CHECK(trigger_autofill_ai_type);
  const std::optional<AttributeType> trigger_attribute_type =
      AttributeType::FromFieldType(*trigger_autofill_ai_type);
  // The triggering field must be of `FieldTypeGroup::kAutofillAi` type and
  // therefore mapping it to an `AttributeType` always returns a value.
  CHECK(trigger_attribute_type);

  // Sort entities based on their frecency.
  std::vector<const EntityInstance*> sorted_entities = base::ToVector(
      entities, [](const EntityInstance& entity) { return &entity; });
  EntityInstance::RankingOrder comp(base::Time::Now());
  std::ranges::sort(sorted_entities,
                    [&](const EntityInstance* lhs, const EntityInstance* rhs) {
                      return comp(*lhs, *rhs);
                    });

  // Suggestion and their fields to be filled metadata.
  std::vector<SuggestionWithMetadata> suggestions_with_metadata;
  for (const EntityInstance* entity : sorted_entities) {
    if (!EntityShouldProduceSuggestion(*trigger_field, *entity, app_locale)) {
      continue;
    }
    suggestions_with_metadata.push_back(
        GetSuggestionForEntity(form, *trigger_field, *entity, app_locale));
  }

  if (suggestions_with_metadata.empty()) {
    return {};
  }

  auto entities_used_to_build_suggestions = base::MakeFlatSet<base::Uuid>(
      suggestions_with_metadata, {}, [](const SuggestionWithMetadata& s) {
        return s.suggestion.GetPayload<Suggestion::AutofillAiPayload>().guid;
      });

  // Labels need to be consistent across the whole fill group. That is, as the
  // user clicks around fields they need to see the same set of attributes as a
  // combination of main text and labels. Therefore, entities that do not
  // generate suggestions on a certain triggering field still affect label
  // generation and should be taken into account.
  std::vector<const EntityInstance*> other_entities_that_can_fill_section;
  for (const EntityInstance* entity : sorted_entities) {
    if (entity->type() == trigger_attribute_type->entity_type() &&
        !entities_used_to_build_suggestions.contains(entity->guid()) &&
        CanFillSomeField(*entity, form, trigger_field->section(), app_locale)) {
      other_entities_that_can_fill_section.push_back(entity);
    }
  }

  std::vector<Suggestion> suggestions = GenerateFillingSuggestionWithLabels(
      *trigger_attribute_type,
      DedupeFillingSuggestions(std::move(suggestions_with_metadata)),
      other_entities_that_can_fill_section, app_locale);

  // Footer suggestions.
  suggestions.emplace_back(SuggestionType::kSeparator);
  // TODO(crbug.com/420455175): Use `autofill_field` when `is_autofilled` starts
  // meaning the same thing in both `AutofillField` and `FormFieldData`.
  if (trigger_field_data.is_autofilled()) {
    suggestions.emplace_back(CreateUndoSuggestion());
  }
  suggestions.emplace_back(CreateManageSuggestion());
  return suggestions;
}

}  // namespace autofill
