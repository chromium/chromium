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
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/data_model/entity_type.h"
#include "components/autofill/core/browser/data_model/entity_type_names.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
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
using autofill::Suggestion;
using autofill::SuggestionType;

constexpr char16_t kLabelSeparator[] = u" · ";

// A map from an attribute type associated with a form field and the root value
// stored in the entity instance for such attribute. The value is always based
// on the "top level type" for the attribute, this means that for both field
// types such as NAME_FIRST and NAME_LAST, the root value will be NAME_FULL,
// similarly for date types. This is used to generate labels, where we want to
// use only complete values.
using FillingSuggestionMetadata =
    std::map<autofill::AttributeType, std::u16string>;
// Represents a list of suggestions and a map which contains metadata about the
// attributes types and their values stored in the entity used to generate the
// suggestion. This is a set as a way to guarantee unique label values for a
// given attribute.
using SuggestionsWithFillingMetadata =
    std::vector<std::pair<autofill::Suggestion, FillingSuggestionMetadata>>;

// For each suggestion in `suggestions`, create its label.
// `labels_for_all_suggestions` contain for each suggestion all the strings that
// should be concatenated to generate the final label.
std::vector<autofill::Suggestion> GetSuggestionsWithLabels(
    std::vector<std::vector<std::u16string>> labels_for_all_suggestions,
    std::vector<autofill::Suggestion> suggestions) {
  CHECK_EQ(labels_for_all_suggestions.size(), suggestions.size());

  size_t suggestion_index = 0;
  for (autofill::Suggestion& suggestion : suggestions) {
    suggestion.labels.push_back({autofill::Suggestion::Text(base::JoinString(
        labels_for_all_suggestions[suggestion_index], kLabelSeparator))});
    suggestion_index++;
  }

  return suggestions;
}

// Generates all labels that it can use to disambiguate a list of suggestions
// for each suggestion in `suggestions_with_filling_metadata`. The vector
// of labels for each suggestion is sorted from lowest to highest priority. The
// available labels are generated based on the values a suggestion would fill.
std::vector<std::vector<std::u16string>> GetAvailableLabelsForSuggestions(
    autofill::AttributeType triggering_field_attribute,
    const SuggestionsWithFillingMetadata& suggestions_with_filling_metadata) {
  CHECK(!suggestions_with_filling_metadata.empty());
  const size_t n_suggestions = suggestions_with_filling_metadata.size();

  // Stores for all suggestions all attributes associated with the fields it
  // will fill and their respective values.
  std::vector<std::vector<std::pair<autofill::AttributeType, std::u16string>>>
      attribute_types_and_values_available_for_suggestions;
  attribute_types_and_values_available_for_suggestions.reserve(n_suggestions);

  // Used to determine whether a certain attribute and value pair repeats across
  // all suggestions. In this case, adding a label for this value is
  // redundant.
  std::map<std::pair<autofill::AttributeType, std::u16string>, size_t>
      attribute_type_and_value_occurrences;

  // Go over each suggestion and its filling metadata. Store all attribute
  // types associated with it and their values.
  for (const auto& [suggestion, filling_metadata] :
       suggestions_with_filling_metadata) {
    const autofill::Suggestion::AutofillAiPayload* payload =
        absl::get_if<autofill::Suggestion::AutofillAiPayload>(
            &suggestion.payload);
    CHECK(payload);

    // For a certain suggestion, initialize a vector containing all attribute
    // types and their respective values.
    std::vector<std::pair<autofill::AttributeType, std::u16string>>
        suggestion_attribute_types_and_labels(filling_metadata.begin(),
                                              filling_metadata.end());
    // The triggering field type is never used as a possible label. This is
    // because its value is already used as the suggestion's main text.
    std::erase_if(suggestion_attribute_types_and_labels,
                  [&](const std::pair<autofill::AttributeType, std::u16string>&
                          attribute_and_label) {
                    return attribute_and_label.first ==
                           triggering_field_attribute;
                  });
    // Sort so that the label with highest priority comes last.
    // For each suggestion, stores all the attribute types found in the form it
    // will fill, together with their respective values. Note that these are
    // only top level values, such as NAME_FULL (as opposed to NAME_FIRST,
    // NAME_LAST etc).
    std::ranges::sort(
        suggestion_attribute_types_and_labels,
        std::not_fn(AttributeType::DisambiguationOrder),
        &std::pair<autofill::AttributeType, std::u16string>::first);

    for (const auto& [attribute_type, entity_value] :
         suggestion_attribute_types_and_labels) {
      attribute_type_and_value_occurrences[{attribute_type, entity_value}]++;
    }

    attribute_types_and_values_available_for_suggestions.push_back(
        std::move(suggestion_attribute_types_and_labels));
  }

  // The output of this method.
  std::vector<std::vector<std::u16string>> labels_available_for_suggestions;
  labels_available_for_suggestions.reserve(
      suggestions_with_filling_metadata.size());

  // Now remove the redundant values from
  // `attribute_types_and_values_available_for_suggestions` and generate the
  // output. A value is considered redundant if it repeats across all
  // suggestions for the same attribute type.
  for (std::vector<std::pair<autofill::AttributeType, std::u16string>>&
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

// Generate labels for suggestions in `suggestions_with_filling_metadata` given
// a triggering field of `AttributeType`.
std::vector<autofill::Suggestion> GenerateFillingSuggestionLabels(
    AttributeType triggering_field_attribute,
    SuggestionsWithFillingMetadata suggestions_with_filling_metadata) {
  // Get all label strings each suggestion can concatenate to build the final
  // label. Already sorted based on priority.
  std::vector<std::vector<std::u16string>> labels_available_for_suggestions =
      GetAvailableLabelsForSuggestions(triggering_field_attribute,
                                       suggestions_with_filling_metadata);

  const size_t n_suggestions = suggestions_with_filling_metadata.size();
  // Initialize the output using `suggestions_with_filling_metadata`.
  std::vector<autofill::Suggestion> suggestions_with_labels;
  suggestions_with_labels.reserve(n_suggestions);
  for (auto& [suggestion, filling_metadata] :
       suggestions_with_filling_metadata) {
    suggestions_with_labels.push_back(std::move(suggestion));
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

// Returns suggestions and their metadata whose set of fields and values
// to be filled are not subsets of another.
SuggestionsWithFillingMetadata DedupeFillingSuggestions(
    SuggestionsWithFillingMetadata suggestions_with_filling_metadata) {
  // Returns -1 if the filling payload of `suggestion_a` is a proper subset of
  // the one from `suggestion_b`. Returns 0 if the filling payload of
  // `suggestion_a` is identical to the one from `suggestion_b`. Returns 1
  // otherwise.
  auto check_suggestions_filling_payload_subset_status =
      [](const Suggestion& suggestion_a, const Suggestion& suggestion_b) {
        const Suggestion::AutofillAiPayload* payload_a =
            absl::get_if<Suggestion::AutofillAiPayload>(&suggestion_a.payload);
        CHECK(payload_a);
        const Suggestion::AutofillAiPayload* payload_b =
            absl::get_if<Suggestion::AutofillAiPayload>(&suggestion_b.payload);
        CHECK(payload_b);

        for (auto& [field_global_id, value_to_fill] :
             payload_a->values_to_fill) {
          if (!payload_b->values_to_fill.contains(field_global_id) ||
              value_to_fill != payload_b->values_to_fill.at(field_global_id)) {
            return 1;
          }
        }

        return payload_b->values_to_fill.size() >
                       payload_a->values_to_fill.size()
                   ? -1
                   : 0;
      };

  // Remove those that are subsets of some other suggestion.
  SuggestionsWithFillingMetadata deduped_filling_suggestions;
  std::set<size_t> duplicated_filling_payloads_to_skip;
  for (size_t i = 0; i < suggestions_with_filling_metadata.size(); i++) {
    if (duplicated_filling_payloads_to_skip.contains(i)) {
      continue;
    }
    bool is_proper_subset_of_another_suggestion = false;
    for (size_t j = 0; j < suggestions_with_filling_metadata.size(); j++) {
      if (i == j) {
        continue;
      }

      int subset_status = check_suggestions_filling_payload_subset_status(
          suggestions_with_filling_metadata[i].first,
          suggestions_with_filling_metadata[j].first);
      if (subset_status == -1) {
        is_proper_subset_of_another_suggestion = true;
      } else if (subset_status == 0) {
        duplicated_filling_payloads_to_skip.insert(j);
      }
    }
    if (!is_proper_subset_of_another_suggestion) {
      deduped_filling_suggestions.push_back(
          suggestions_with_filling_metadata[i]);
    }
  }

  return deduped_filling_suggestions;
}

autofill::Suggestion::Icon GetSuggestionIcon(
    autofill::EntityType triggering_field_entity_type) {
  switch (triggering_field_entity_type.name()) {
    case autofill::EntityTypeName::kPassport:
      return autofill::Suggestion::Icon::kIdCard;
    case autofill::EntityTypeName::kLoyaltyCard:
      return autofill::Suggestion::Icon::kLoyalty;
    case autofill::EntityTypeName::kDriversLicense:
      return autofill::Suggestion::Icon::kIdCard;
    case autofill::EntityTypeName::kVehicle:
      return autofill::Suggestion::Icon::kVehicle;
  }
  NOTREACHED();
}

}  // namespace

std::vector<Suggestion> CreateLoadingSuggestions() {
  Suggestion loading_suggestion(SuggestionType::kAutofillAiLoadingState);
  loading_suggestion.trailing_icon = Suggestion::Icon::kAutofillAi;
  loading_suggestion.acceptability = Suggestion::Acceptability::kUnacceptable;
  return {loading_suggestion};
}

std::vector<Suggestion> CreateFillingSuggestions(
    const autofill::FormStructure& form,
    FieldGlobalId field_global_id,
    base::span<const autofill::EntityInstance> entities) {
  const AutofillField* autofill_field = form.GetFieldById(field_global_id);
  CHECK(autofill_field);

  std::optional<autofill::FieldType>
      triggering_field_autofill_ai_type_prediction =
          autofill_field->GetAutofillAiServerTypePredictions();
  CHECK(triggering_field_autofill_ai_type_prediction);
  std::optional<AttributeType> triggering_field_attribute_type =
      AttributeType::FromFieldType(
          *triggering_field_autofill_ai_type_prediction);
  // The triggering field should be of `FieldTypeGroup::kAutofillAi`
  // type and therefore mapping it to an `AttributeType` should always
  // return a value.
  CHECK(triggering_field_attribute_type);

  // Suggestion and their fields to be filled metadata.
  SuggestionsWithFillingMetadata suggestions_with_filling_metadata;
  for (const autofill::EntityInstance& entity : entities) {
    //  Only entities that match the triggering field entity should be used to
    //  generate suggestions.
    if (entity.type() != triggering_field_attribute_type->entity_type()) {
      continue;
    }
    base::optional_ref<const AttributeInstance> attribute_for_triggering_field =
        entity.attribute(*triggering_field_attribute_type);
    // Do not create suggestion if the triggering field cannot be filled.
    if (!attribute_for_triggering_field) {
      continue;
    }
    const std::u16string main_text = attribute_for_triggering_field->value();
    std::u16string normalized_main_text =
        autofill::AutofillProfileComparator::NormalizeForComparison(main_text);
    const std::u16string normalized_triggering_field_content =
        autofill::AutofillProfileComparator::NormalizeForComparison(
            autofill_field->value(autofill::ValueSemantics::kCurrent));
    // TODO(crbug.com/394011769): Do not prefix match data that should be obfuscated.
    if (!normalized_main_text.starts_with(
            normalized_triggering_field_content)) {
      continue;
    }
    autofill::Suggestion suggestion(main_text, SuggestionType::kFillAutofillAi);

    std::vector<std::pair<FieldGlobalId, std::u16string>> values_to_fill;
    FillingSuggestionMetadata filling_suggestion_metadata;
    for (const std::unique_ptr<AutofillField>& field : form.fields()) {
      // Only fill fields that match the triggering field section.
      if (field->section() != autofill_field->section()) {
        continue;
      }
      std::optional<autofill::FieldType> field_autofill_ai_prediction =
          field->GetAutofillAiServerTypePredictions();
      if (!field_autofill_ai_prediction) {
        continue;
      }

      std::optional<AttributeType> field_attribute_type =
          AttributeType::FromFieldType(*field_autofill_ai_prediction);
      CHECK(field_attribute_type);
      // Only fields that match the triggering field entity should be used to
      // generate suggestions.
      if (!field_attribute_type ||
          triggering_field_attribute_type->entity_type() !=
              field_attribute_type->entity_type()) {
        continue;
      }

      base::optional_ref<const AttributeInstance> attribute =
          entity.attribute(*field_attribute_type);
      if (!attribute || attribute->value().empty()) {
        continue;
      }

      values_to_fill.emplace_back(field->global_id(), attribute->value());
      filling_suggestion_metadata[*field_attribute_type] = attribute->value();
    }
    auto payload = Suggestion::AutofillAiPayload(values_to_fill);
    suggestion.payload = payload;
    suggestion.icon =
        GetSuggestionIcon(triggering_field_attribute_type->entity_type());
    suggestions_with_filling_metadata.emplace_back(
        std::move(suggestion), std::move(filling_suggestion_metadata));
  }

  if (suggestions_with_filling_metadata.empty()) {
    return {};
  }

  std::vector<Suggestion> suggestions;
  suggestions = GenerateFillingSuggestionLabels(
      *triggering_field_attribute_type,
      DedupeFillingSuggestions(std::move(suggestions_with_filling_metadata)));

  // Footer suggestions.
  suggestions.emplace_back(SuggestionType::kSeparator);
  if (autofill_field->is_autofilled()) {
    suggestions.emplace_back(CreateUndoSuggestion());
  }
  suggestions.emplace_back(CreateManageSuggestion());
  return suggestions;
}

}  // namespace autofill_ai
