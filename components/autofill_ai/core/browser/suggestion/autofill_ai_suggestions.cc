// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/suggestion/autofill_ai_suggestions.h"

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/entities/field_filling_entity_util.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
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

constexpr char16_t kLabelSeparator[] = u" · ";

struct SuggestionWithMetadata {
  // A suggestion whose payload is of type `Suggestion::AutofillAiPayload`.
  Suggestion suggestion;

  // The values that would be filled by `suggestion`, indexed by the underlying
  // attribute's type. The value is always based on the "top level type" for the
  // attribute, this means that for both field types such as NAME_FIRST and
  // NAME_LAST, the root value will be NAME_FULL, similarly for date types. This
  // is used to generate labels, where we want to use only complete values.
  base::flat_map<AttributeType, std::u16string> attribute_type_to_value;

  // The values that would be filled by `suggestion`, indexed by the underlying
  // field's ID.
  base::flat_map<FieldGlobalId, std::u16string> field_to_value;
};

// For each suggestion in `suggestions`, create its label.
// `labels_for_all_suggestions` contain for each suggestion all the strings that
// should be concatenated to generate the final label.
std::vector<Suggestion> GetSuggestionsWithLabels(
    std::vector<std::vector<std::u16string>> labels_for_all_suggestions,
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

// Generates all labels that it can use to disambiguate a list of suggestions
// for each suggestion in `suggestions_with_metadata`. The vector
// of labels for each suggestion is sorted from lowest to highest priority. The
// available labels are generated based on the values a suggestion would fill.
std::vector<std::vector<std::u16string>> GetAvailableLabelsForSuggestions(
    AttributeType triggering_field_attribute,
    base::span<const SuggestionWithMetadata> suggestions_with_metadata) {
  CHECK(!suggestions_with_metadata.empty());
  const size_t n_suggestions = suggestions_with_metadata.size();

  // Stores for all suggestions all attributes associated with the fields it
  // will fill and their respective values.
  std::vector<std::vector<std::pair<AttributeType, std::u16string>>>
      attribute_types_and_values_available_for_suggestions;
  attribute_types_and_values_available_for_suggestions.reserve(n_suggestions);

  // Used to determine whether a certain attribute and value pair repeats across
  // all suggestions. In this case, adding a label for this value is
  // redundant.
  std::map<std::pair<AttributeType, std::u16string>, size_t>
      attribute_type_and_value_occurrences;

  // Go over each suggestion and its filling metadata. Store all attribute
  // types associated with it and their values.
  for (const SuggestionWithMetadata& s : suggestions_with_metadata) {
    // For a certain suggestion, initialize a vector containing all attribute
    // types and their respective values.
    std::vector<std::pair<AttributeType, std::u16string>>
        suggestion_attribute_types_and_labels(s.attribute_type_to_value.begin(),
                                              s.attribute_type_to_value.end());
    // The triggering field type is never used as a possible label. This is
    // because its value is already used as the suggestion's main text.
    std::erase_if(suggestion_attribute_types_and_labels,
                  [&](const std::pair<AttributeType, std::u16string>&
                          attribute_and_label) {
                    return attribute_and_label.first ==
                           triggering_field_attribute;
                  });
    // Sort so that the label with highest priority comes last.
    // For each suggestion, stores all the attribute types found in the form it
    // will fill, together with their respective values. Note that these are
    // only top level values, such as NAME_FULL (as opposed to NAME_FIRST,
    // NAME_LAST etc).
    std::ranges::sort(suggestion_attribute_types_and_labels,
                      std::not_fn(AttributeType::DisambiguationOrder),
                      &std::pair<AttributeType, std::u16string>::first);

    for (const auto& [attribute_type, entity_value] :
         suggestion_attribute_types_and_labels) {
      ++attribute_type_and_value_occurrences[{attribute_type, entity_value}];
    }

    attribute_types_and_values_available_for_suggestions.push_back(
        std::move(suggestion_attribute_types_and_labels));
  }

  // The output of this method.
  std::vector<std::vector<std::u16string>> labels_available_for_suggestions;
  labels_available_for_suggestions.reserve(suggestions_with_metadata.size());

  // Now remove the redundant values from
  // `attribute_types_and_values_available_for_suggestions` and generate the
  // output. A value is considered redundant if it repeats across all
  // suggestions for the same attribute type.
  for (std::vector<std::pair<AttributeType, std::u16string>>&
           suggestion_attribute_types_and_value :
       attribute_types_and_values_available_for_suggestions) {
    std::vector<std::u16string> labels_for_suggestion;
    for (auto& [attribute_type, value] : suggestion_attribute_types_and_value) {
      // The label is the same for all suggestions and has no differentiation
      // value.
      if (attribute_type_and_value_occurrences[{attribute_type, value}] ==
          n_suggestions) {
        continue;
      }
      labels_for_suggestion.push_back(std::move(value));
    }
    // At least one label should exist, even if it repeats in other suggestions.
    // This is because labels also have descriptive value.
    if (labels_for_suggestion.empty() &&
        !suggestion_attribute_types_and_value.empty()) {
      // Take the last value because it is the one with highest priority.
      labels_for_suggestion.push_back(
          suggestion_attribute_types_and_value.back().second);
    }
    labels_available_for_suggestions.push_back(
        std::move(labels_for_suggestion));
  }

  return labels_available_for_suggestions;
}

// Generate labels for suggestions in `suggestions_with_metadata` given
// a triggering field of `AttributeType`.
std::vector<Suggestion> GenerateFillingSuggestionLabels(
    AttributeType triggering_field_attribute,
    std::vector<SuggestionWithMetadata> suggestions_with_metadata) {
  // Get all label strings each suggestion can concatenate to build the final
  // label. Already sorted based on priority.
  std::vector<std::vector<std::u16string>> labels_available_for_suggestions =
      GetAvailableLabelsForSuggestions(triggering_field_attribute,
                                       suggestions_with_metadata);

  const size_t n_suggestions = suggestions_with_metadata.size();
  // Initialize the output using `suggestions_with_metadata`.
  std::vector<Suggestion> suggestions_with_labels;
  suggestions_with_labels.reserve(n_suggestions);
  for (SuggestionWithMetadata& s : std::move(suggestions_with_metadata)) {
    suggestions_with_labels.push_back(std::move(s.suggestion));
  }

  // The maximum number of labels is defined based on the suggestion with the
  // largest number of available labels.
  size_t max_number_of_labels = 0;
  for (const std::vector<std::u16string>& suggestion_labels_available :
       labels_available_for_suggestions) {
    max_number_of_labels =
        std::max(max_number_of_labels, suggestion_labels_available.size());
  }
  constexpr size_t kMinimumNumberOfLabelsToUse = 1;

  // Initialize the final list of labels to be used by each suggestions. Note
  // that they always contain at least the entity name.
  std::vector<std::vector<std::u16string>> suggestions_labels(
      n_suggestions,
      {std::u16string(
          triggering_field_attribute.entity_type().GetNameForI18n())});

  // Try to generate suggestions with unique labels, starting from the first
  // available label for each suggestion. Note that the uniqueness check only
  // happens at the end of each label count iteration, so we optimize for labels
  // that have similar length (not always possible because some entities might
  // simply not have enough data).
  for (size_t label_count = 1; label_count <= max_number_of_labels;
       label_count++) {
    size_t suggestion_index = 0;
    // Used to check whether a suggestion main text and label are unique.
    std::set<std::u16string> main_text_and_labels;

    // Iterate over the available labels for each suggestion.
    for (std::vector<std::u16string>& suggestion_labels_available :
         labels_available_for_suggestions) {
      const std::u16string& main_text =
          suggestions_with_labels[suggestion_index].main_text.value;
      std::u16string current_label_and_main_text =
          base::StrCat({main_text, kLabelSeparator,
                        base::JoinString(suggestions_labels[suggestion_index],
                                         kLabelSeparator)});
      // If there is no more available label for a suggestion, simply add the
      // concatenation of all labels already used and the main text to the Set.
      if (suggestion_labels_available.empty()) {
        main_text_and_labels.insert(std::move(current_label_and_main_text));
        suggestion_index++;
        continue;
      }

      // Otherwise add the current top label, update the set and remove the
      // label from the available list. Note that the labels are sorted from
      // lowest to highest priority.
      suggestions_labels[suggestion_index].push_back(
          suggestion_labels_available.back());
      main_text_and_labels.insert(
          base::StrCat({current_label_and_main_text, kLabelSeparator,
                        suggestion_labels_available.back()}));
      suggestion_labels_available.pop_back();
      suggestion_index++;
    }

    // Label uniqueness was reached if the number of unique main_text + labels
    // concatenated strings is same as the suggestions size.
    const bool are_all_labels_unique =
        main_text_and_labels.size() == suggestions_with_labels.size();
    if (are_all_labels_unique && label_count >= kMinimumNumberOfLabelsToUse) {
      break;
    }
  }

  return GetSuggestionsWithLabels(std::move(suggestions_labels),
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

    std::vector<std::pair<AttributeType, std::u16string>>
        attribute_type_to_value;
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

      const std::u16string full_attribute_value =
          attribute->GetCompleteInfo(app_locale);
      const std::u16string attribute_value =
          attribute->GetInfo(field->Type().GetStorableType(), app_locale,
                             autofill_field->format_string());

      if (full_attribute_value.empty() || attribute_value.empty()) {
        continue;
      }

      attribute_type_to_value.emplace_back(*attribute_type,
                                           full_attribute_value);
      field_to_value.emplace_back(field->global_id(), attribute_value);
    }

    SuggestionWithMetadata& s = suggestions_with_metadata.emplace_back();
    s.suggestion = Suggestion(attribute_for_triggering_field->GetInfo(
                                  autofill_field->Type().GetStorableType(),
                                  app_locale, autofill_field->format_string()),
                              SuggestionType::kFillAutofillAi);
    s.suggestion.payload = Suggestion::AutofillAiPayload(entity.guid());
    s.suggestion.icon =
        GetSuggestionIcon(trigger_field_attribute_type->entity_type());
    s.attribute_type_to_value =
        base::flat_map(std::move(attribute_type_to_value));
    s.field_to_value = base::flat_map(std::move(field_to_value));
  }

  if (suggestions_with_metadata.empty()) {
    return {};
  }

  std::vector<Suggestion> suggestions = GenerateFillingSuggestionLabels(
      *trigger_field_attribute_type,
      DedupeFillingSuggestions(std::move(suggestions_with_metadata)));

  // Footer suggestions.
  suggestions.emplace_back(SuggestionType::kSeparator);
  if (autofill_field->is_autofilled()) {
    suggestions.emplace_back(CreateUndoSuggestion());
  }
  suggestions.emplace_back(CreateManageSuggestion());
  return suggestions;
}

}  // namespace autofill_ai
