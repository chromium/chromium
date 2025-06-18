// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_suggestions.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
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
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
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
  // Creates a type assignment that matches the legacy behavior where `*_TAG`
  // types still exist. In that case, every AutofillField is assigned at most
  // one AttributeType.
  AttributeTypeAssignment(
      base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
      const Section& trigger_section)
      : map_(DetermineAttributeTypes(fields, trigger_section)) {}

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

struct SuggestionWithMetadata {
  // A suggestion whose payload is of type `Suggestion::AutofillAiPayload`.
  Suggestion suggestion;

  // The entity used to build `suggestion`.
  raw_ref<const EntityInstance> entity;

  // The attribute (of `entity`) of the trigger field.
  AttributeType trigger_attribute_type;

  // The values that would be filled by `suggestion`, indexed by the underlying
  // field's ID.
  base::flat_map<FieldGlobalId, std::u16string> field_to_value;
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

std::vector<Suggestion> AssignLabelsToSuggestions(
    base::span<const EntityLabel> labels,
    std::vector<Suggestion> suggestions) {
  DCHECK_EQ(labels.size(), suggestions.size());
  for (auto [suggestion, label] : base::zip(suggestions, labels)) {
    DCHECK(suggestion.labels.empty());
    suggestion.labels.push_back(
        {Suggestion::Text(base::JoinString(label, kLabelSeparator))});
  }
  return suggestions;
}

// Returns a vector of EntityLabels, with one entry for each
// SuggestionWithMetadata in `suggestions`.
//
// That is, the `i`th element of the returned vector corresponds to
// `suggestions[i]`. The individual EntityLabels may be empty, but the strings
// they contain are non-empty.
//
// Labels are supposed to be shown by the UI in the second line of each
// suggestion (not the main text).
//
// Labels consist of the AttributeInstance values. Ideally, every suggestion is
// uniquely identifiable by its label.
//
// More precisely, two kinds of EntityInstances are taken into account:
// - `SuggestionWithMetadata::entity` for `suggestions`
// - `other_entities_that_can_fill_section`
// That is, a suggestion's label ideally not only uniquely identifies the
// suggestion's entity among the other suggestions' entities, but also among
// those entities that may be autofilled from some other field in the same
// section.
//
// In reality, labels may not uniquely identify the underlying entity: for one
// thing, the maximum length of the label is limited; for another, different
// entities may agree on the values of the disambiguating attributes.
std::vector<EntityLabel> GetLabelsForSuggestions(
    base::span<const SuggestionWithMetadata> suggestions,
    base::span<const EntityInstance*> other_entities_that_can_fill_section,
    const std::string& app_locale) {
  std::vector<const EntityInstance*> entities = base::ToVector(
      suggestions, [](const SuggestionWithMetadata& suggestions) {
        return &suggestions.entity.get();
      });
  entities.insert(entities.end(), other_entities_that_can_fill_section.begin(),
                  other_entities_that_can_fill_section.end());

  std::vector<EntityLabel> labels = GetLabelsForEntities(
      entities, /*allow_only_disambiguating_types=*/true,
      /*allow_only_disambiguating_values=*/true, app_locale);
  if (labels.size() > suggestions.size()) {
    // Drop the labels for the `other_entities_that_can_fill_section`.
    labels.resize(suggestions.size());
  }
  return labels;
}

// Populates `Suggestion::labels` of the given `suggestions` and returns the
// result.
//
// The size of the returned vector is that of `suggestions`.
//
// See GetLabelsForSuggestions() for details on the label generation.
std::vector<Suggestion> GenerateFillingSuggestionWithLabels(
    std::vector<SuggestionWithMetadata> suggestions,
    base::span<const EntityInstance*> other_entities_that_can_fill_section,
    const std::string& app_locale) {
  std::vector<EntityLabel> labels = GetLabelsForSuggestions(
      suggestions, other_entities_that_can_fill_section, app_locale);
  DCHECK_EQ(suggestions.size(), labels.size());

  // Postprocess the labels:
  // - Remove the trigger field's value (if present) because it's also shown in
  //   the suggestions top row.
  // - Prepend the entity type's name to each label.
  for (auto [suggestion, label] : base::zip(suggestions, labels)) {
    const EntityInstance& entity = *suggestion.entity;
    const AttributeType trigger_attribute_type =
        suggestion.trigger_attribute_type;
    base::optional_ref<const AttributeInstance> attribute =
        entity.attribute(trigger_attribute_type);
    CHECK(attribute);
    std::erase(label, attribute->GetCompleteInfo(app_locale));
    label.insert(label.begin(), std::u16string(entity.type().GetNameForI18n()));
  }

  return AssignLabelsToSuggestions(
      labels,
      base::ToVector(std::move(suggestions), [](SuggestionWithMetadata& s) {
        return std::move(s).suggestion;
      }));
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
      trigger_field.field->Type().GetStorableType(), app_locale,
      trigger_field.field->format_string());
  if (trigger_value.empty()) {
    return false;
  }

  // Obfuscated types are not prefix matched to avoid that a webpage can
  // use the existence of suggestions to guess a user's data.
  if (!trigger_field.type.is_obfuscated()) {
    const std::u16string normalized_attribute =
        AutofillProfileComparator::NormalizeForComparison(trigger_value);
    const std::u16string normalized_field_content =
        AutofillProfileComparator::NormalizeForComparison(
            trigger_field.field->value());
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
        return attribute && !attribute
                                 ->GetInfo(f.field->Type().GetStorableType(),
                                           app_locale, f.field->format_string())
                                 .empty();
      });
}

SuggestionWithMetadata GetSuggestionForEntity(
    const EntityInstance& entity,
    base::span<const AutofillFieldWithAttributeType> fields,
    const AutofillFieldWithAttributeType& trigger_field,
    const std::string& app_locale) {
  // The dereference is guaranteed by EntityShouldProduceSuggestion().
  const AttributeInstance& trigger_attribute =
      *entity.attribute(trigger_field.type);

  std::vector<std::pair<FieldGlobalId, std::u16string>> field_to_value;
  for (const auto& [field, attribute_type] : fields) {
    DCHECK_EQ(entity.type(), attribute_type.entity_type());
    base::optional_ref<const AttributeInstance> attribute =
        entity.attribute(attribute_type);
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

  Suggestion suggestion =
      Suggestion(trigger_attribute.GetInfo(
                     trigger_field.field->Type().GetStorableType(), app_locale,
                     trigger_field.field->format_string()),
                 SuggestionType::kFillAutofillAi);
  suggestion.payload = Suggestion::AutofillAiPayload(entity.guid());
  suggestion.icon = GetSuggestionIcon(entity.type());
  return SuggestionWithMetadata(suggestion, raw_ref(entity), trigger_field.type,
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

  AttributeTypeAssignment assignment =
      AttributeTypeAssignment(form.fields(), trigger_field->section());

  // Sort entities based on their frecency.
  std::vector<const EntityInstance*> sorted_entities = base::ToVector(
      entities, [](const EntityInstance& entity) { return &entity; });
  std::ranges::sort(sorted_entities,
                    [comp = EntityInstance::FrecencyOrder(base::Time::Now())](
                        const EntityInstance* lhs, const EntityInstance* rhs) {
                      return comp(*lhs, *rhs);
                    });

  std::vector<SuggestionWithMetadata> suggestions_with_metadata;
  for (const EntityInstance* entity : sorted_entities) {
    base::span<const AutofillFieldWithAttributeType> fields_with_types =
        assignment.Find(entity->type());
    base::optional_ref<const AutofillFieldWithAttributeType>
        trigger_field_with_type =
            FindField(fields_with_types, trigger_field->global_id());
    if (!trigger_field_with_type ||
        !EntityShouldProduceSuggestion(*entity, *trigger_field_with_type,
                                       app_locale)) {
      continue;
    }
    suggestions_with_metadata.push_back(GetSuggestionForEntity(
        *entity, fields_with_types, *trigger_field_with_type, app_locale));
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
    if (!entities_used_to_build_suggestions.contains(entity->guid()) &&
        CanFillSomeField(*entity, assignment.Find(entity->type()),
                         app_locale)) {
      other_entities_that_can_fill_section.push_back(entity);
    }
  }

  std::vector<Suggestion> suggestions = GenerateFillingSuggestionWithLabels(
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
