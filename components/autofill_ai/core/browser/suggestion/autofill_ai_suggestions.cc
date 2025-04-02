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

constexpr size_t kMaxNumberOfLabels = 3;
// Arbitrary delimiter to user when concatenating labels to decide whether
// a series of labels for different entities are unique.
constexpr char16_t kLabelsDelimiter[] = u" - - ";

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

// Returns all labels that can be used to disambiguate a list of suggestions
// for each suggestion in `suggestions_with_metadata`. The vector
// of labels for each suggestion is sorted from lowest to highest priority and
// only contain values that will be added to the second line of the suggestion
// UI (so not the main text). The available labels are generated based on the
// values held by the entity used to create a certain suggestion.
//
// This function retrieves the available labels by doing the following:
//
// 1. Retrieves the list of entities used to build each suggestion
// (`SuggestionWithMetadata::entity`).
//
// 2. Calls `GetLabelsForEntities()`, making sure use the
//    `triggering_field_attribute` as the attribute type to exclude from the
//    possible labels, since it will already be part of the suggestion main
//    text.
EntitiesLabels GetAvailableLabelsForSuggestions(
    AttributeType triggering_field_attribute,
    base::span<const SuggestionWithMetadata> suggestions_with_metadata,
    const std::string& app_locale) {
  CHECK(!suggestions_with_metadata.empty());
  // Step 1#
  std::vector<const autofill::EntityInstance*> entities =
      base::ToVector(suggestions_with_metadata,
                     [](SuggestionWithMetadata suggestion_with_metadata) {
                       return &suggestion_with_metadata.entity.get();
                     });
  // Step 2#
  return GetLabelsForEntities(entities, {triggering_field_attribute},
                              app_locale);
}

// Generates suggestions with labels given `suggestions_with_metadata` (which
// holds both the suggestions and their respective entities)
// and a triggering field of `AttributeType`. This function works as follows:
//
// 1. Initializes the output (a vector of suggestions) and its default labels.
//    The labels at this point are all the same (single string with the entity
//    name).
//
// 2. If there is only one suggestion, this method returns this single
//    suggestion with the label initialized in step 1#. Its label will be simply
//    the entity name.
//
// 3. Checks whether the triggering field type is part of the disambiguation
//    order. If true, it is used as disambiguation criteria
//    (for example the user passport name but not the number).
//
//    If the main texts of all suggestions are different, also return early and
//    keep only the entity name as suggestions labels (initialized in step 1#).
//    The rationale here is that information such as the user name is already
//    valuable for disambiguation, but a passport number is not.
//
// 4. At this point further labels besides only the entity name are required.
//    This step retrieves all available labels for each suggestion given
//   `suggestions_with_metadata`, see `GetAvailableLabelsForSuggestions()`.
//
// 5. Goes over all labels available for each suggestion and add them to a final
//    list of labels to be used. This step makes sure that no more labels
//    are added when unique labels across all suggestions are found.
//
// 6. Assigns the labels acquired in step 5# and return the updated suggestions.
std::vector<Suggestion> GenerateFillingSuggestionWithLabels(
    AttributeType triggering_field_attribute,
    std::vector<SuggestionWithMetadata> suggestions_with_metadata,
    const std::string& app_locale) {
  // Step 1#
  const size_t n_suggestions = suggestions_with_metadata.size();
  // Initialize the output using `suggestions_with_metadata`.
  std::vector<Suggestion> suggestions_with_labels;
  suggestions_with_labels.reserve(n_suggestions);
  for (SuggestionWithMetadata& s : std::move(suggestions_with_metadata)) {
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
  // For a single suggestion, no further label is needed (only the entity name).
  if (n_suggestions == 1) {
    return AssignLabelsToSuggestions(std::move(suggestions_labels),
                                     std::move(suggestions_with_labels));
  }

  // Step 3
  // If the attribute type used to create the suggestion's main text is already
  // part of an entity disambiguation order, we do not need to add further
  // labels if all main texts are unique.
  if (triggering_field_attribute.is_disambiguation_type()) {
    auto unique_main_texts = base::MakeFlatSet<std::u16string>(
        suggestions_with_labels, {},
        [](const Suggestion& s) { return s.main_text.value; });

    if (unique_main_texts.size() == n_suggestions) {
      return AssignLabelsToSuggestions(std::move(suggestions_labels),
                                       std::move(suggestions_with_labels));
    }
  }

  // Step 4#
  // Get all label strings each suggestion can concatenate to build the final
  // label. Already sorted based on priority.
  EntitiesLabels labels_available_for_suggestions =
      GetAvailableLabelsForSuggestions(triggering_field_attribute,
                                       suggestions_with_metadata, app_locale);
  size_t max_number_of_labels = 0;
  for (const std::vector<std::u16string>& suggestion_labels_available :
       *labels_available_for_suggestions) {
    max_number_of_labels =
        std::max(max_number_of_labels, suggestion_labels_available.size());
  }
  max_number_of_labels = std::min(max_number_of_labels, kMaxNumberOfLabels);

  // Step 5#
  // Creates the concatenation of all labels used by the suggestion in index
  // `suggestion_index` (and a possible main text). This is stored in a Set to
  // track when unique labels are found across suggestions. Note that if the
  // triggering field is not part of the entity disambiguation attributes, the
  // main text is not taken into account and we simply use an empty string in
  // the concatenation.
  auto make_label_string = [&](size_t suggestion_index) {
    const Suggestion& suggestion = suggestions_with_labels[suggestion_index];
    const std::vector<std::u16string> labels =
        (*suggestions_labels)[suggestion_index];
    const std::u16string& main_text =
        triggering_field_attribute.is_disambiguation_type()
            ? suggestion.main_text.value
            : base::EmptyString16();
    return base::StrCat({main_text, kLabelsDelimiter,
                         base::JoinString(labels, kLabelsDelimiter)});
  };
  for (size_t label_count = 1; label_count <= max_number_of_labels;
       label_count++) {
    // Used to check whether a suggestion main text and label are unique.
    // Note that the main text is only relevant when it is part of the
    // disambiguation order for the entity (therefore
    // "possible_main_text_and_labels").
    std::set<std::u16string> possible_main_text_and_labels;

    // Iterate over the available labels for each suggestion and add labels to
    // the final `suggestions_labels`, until the concatenation of all labels
    // leads to unique strings across all suggestions. Note that
    // `suggestions_labels` contains for each suggestion a vector of labels
    // (strings) to be used.
    CHECK_EQ(labels_available_for_suggestions->size(),
             suggestions_labels->size());
    for (size_t i = 0; i < labels_available_for_suggestions->size(); ++i) {
      std::vector<std::u16string>& suggestion_labels_available =
          (*labels_available_for_suggestions)[i];
      std::vector<std::u16string>& suggestion_labels_output =
          (*suggestions_labels)[i];
      // If there is no more available label for a suggestion, simply add the
      // concatenation of all labels already used and the main text to the Set.
      if (suggestion_labels_available.empty()) {
        possible_main_text_and_labels.insert(make_label_string(i));
        continue;
      }

      // Otherwise add the current top label, update the set and remove the
      // label from the available list. Note that the labels are sorted from
      // lowest to highest priority.
      suggestion_labels_output.push_back(suggestion_labels_available.back());
      possible_main_text_and_labels.insert(make_label_string(i));

      // Label uniqueness was reached if the number of unique main_text + labels
      // concatenated strings is same as the suggestions size
      if (possible_main_text_and_labels.size() ==
          suggestions_with_labels.size()) {
        goto found_unique_labels;
      }
      suggestion_labels_available.pop_back();
    }
  }

found_unique_labels:
  // Step 6#
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
  for (const autofill::EntityInstance& entity : entities) {
    //  Only entities that match the triggering field entity should be used to
    //  generate suggestions.
    if (entity.type() != trigger_field_attribute_type->entity_type()) {
      continue;
    }
    base::optional_ref<const AttributeInstance> attribute_for_triggering_field =
        entity.attribute(*trigger_field_attribute_type);
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
              autofill_field->value(autofill::ValueSemantics::kCurrent));
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

      base::optional_ref<const AttributeInstance> attribute =
          entity.attribute(*attribute_type);
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
    for (const AttributeInstance& attribute : entity.attributes()) {
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
    suggestion.payload = Suggestion::AutofillAiPayload(entity.guid());
    suggestion.icon =
        GetSuggestionIcon(trigger_field_attribute_type->entity_type());
    suggestions_with_metadata.emplace_back(
        suggestion, raw_ref(entity), base::flat_map(std::move(field_to_value)));
  }

  if (suggestions_with_metadata.empty()) {
    return {};
  }

  std::vector<Suggestion> suggestions = GenerateFillingSuggestionWithLabels(
      *trigger_field_attribute_type,
      DedupeFillingSuggestions(std::move(suggestions_with_metadata)),
      app_locale);

  // Footer suggestions.
  suggestions.emplace_back(SuggestionType::kSeparator);
  if (autofill_field->is_autofilled()) {
    suggestions.emplace_back(CreateUndoSuggestion());
  }
  suggestions.emplace_back(CreateManageSuggestion());
  return suggestions;
}

}  // namespace autofill_ai
