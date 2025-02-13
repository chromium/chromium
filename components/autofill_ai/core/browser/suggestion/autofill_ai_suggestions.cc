// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/suggestion/autofill_ai_suggestions.h"

#include <string>

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/data_model/entity_type.h"
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
std::vector<Suggestion> DedupeFillingSuggestions(
    std::vector<Suggestion> suggestions) {
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
  std::vector<Suggestion> deduped_filling_suggestions;
  std::set<size_t> duplicated_filling_payloads_to_skip;
  for (size_t i = 0; i < suggestions.size(); i++) {
    if (duplicated_filling_payloads_to_skip.contains(i)) {
      continue;
    }
    bool is_proper_subset_of_another_suggestion = false;
    for (size_t j = 0; j < suggestions.size(); j++) {
      if (i == j) {
        continue;
      }

      int subset_status = check_suggestions_filling_payload_subset_status(
          suggestions[i], suggestions[j]);
      if (subset_status == -1) {
        is_proper_subset_of_another_suggestion = true;
      } else if (subset_status == 0) {
        duplicated_filling_payloads_to_skip.insert(j);
      }
    }
    if (!is_proper_subset_of_another_suggestion) {
      deduped_filling_suggestions.push_back(suggestions[i]);
    }
  }

  return deduped_filling_suggestions;
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

  std::vector<Suggestion> suggestions;
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
    // TODO(crbug.com/389629573): Handle label generation.
    suggestions.emplace_back(
        base::UTF8ToUTF16(attribute_for_triggering_field->value()),
        SuggestionType::kFillAutofillAi);

    std::vector<std::pair<FieldGlobalId, std::u16string>> values_to_fill;
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
      if (!attribute) {
        continue;
      }

      values_to_fill.emplace_back(field->global_id(),
                                  base::UTF8ToUTF16(attribute->value()));
    }
    auto payload = Suggestion::AutofillAiPayload(values_to_fill);
    suggestions.back().payload = payload;
  }

  if (suggestions.empty()) {
    return {};
  }

  suggestions = DedupeFillingSuggestions(std::move(suggestions));

  // Footer suggestions.
  suggestions.emplace_back(SuggestionType::kSeparator);
  if (autofill_field->is_autofilled()) {
    suggestions.emplace_back(CreateUndoSuggestion());
  }
  suggestions.emplace_back(CreateManageSuggestion());
  return suggestions;
}

}  // namespace autofill_ai
