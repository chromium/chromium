// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/autofill_ai/autofill_ai_suggestion_generator.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ref.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/types/zip.h"
#include "build/buildflag.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_labels.h"
#include "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace autofill {
namespace {

int GetRecordTypePriority(EntityInstance::RecordType record_type) {
  switch (record_type) {
    case EntityInstance::RecordType::kServerWallet:
      return 2;
    case EntityInstance::RecordType::kLocal:
      return 1;
    case EntityInstance::RecordType::kPersonalContext:
      return 0;
  }
  NOTREACHED();
}

// Represents all the different UI sections for autofill ai data in Chrome
// Settings.
enum class AutofillAiUiSection {
  kTravel,
  kIdentityDocs,
  kShopping,
  kMaxValue = kShopping,
};

// Holds an assignment of AutofillFields to AttributeTypes.
//
// Note that an AutofillField may have multiple AttributeTypes of distinct
// EntityTypes assigned. That is, it may happen that both of the following are
// true:
//   std::ranges::contains(assignment.Find(EntityType(kVehicle)),
//                  {field, AttributeType(kVehicleOwner));
//   std::ranges::contains(assignment.Find(EntityType(kDriversLicense)),
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

// Returns a suggestion to manage all AutofillAi data.
Suggestion CreateManageAutofillAiSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_MANAGE_SUGGESTION_MAIN_TEXT),
      SuggestionType::kManageAutofillAi);
  suggestion.icon = Suggestion::Icon::kSettings;
  return suggestion;
}

// Returns a suggestion to manage AutofillAi identity dosc data.
Suggestion CreateManageIdentityDocsSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_MANAGE_IDENTITY_DOCS_SUGGESTION_MAIN_TEXT),
      SuggestionType::kManageAutofillAiIdentityDocs);
  suggestion.icon = Suggestion::Icon::kSettings;
  return suggestion;
}

// Returns a suggestion to manage AutofillAi travel data.
Suggestion CreateManageTravelSuggestion() {
  Suggestion suggestion(l10n_util::GetStringUTF16(
                            IDS_AUTOFILL_AI_MANAGE_TRAVEL_SUGGESTION_MAIN_TEXT),
                        SuggestionType::kManageAutofillAiTravel);
  suggestion.icon = Suggestion::Icon::kSettings;
  return suggestion;
}

// Returns a suggestion to manage AutofillAi shopping data.
Suggestion CreateManageShoppingSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_MANAGE_SHOPPING_SUGGESTION_MAIN_TEXT),
      SuggestionType::kManageAutofillAiShopping);
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
    const FormFieldData& trigger_field,
    const DenseSet<AutofillAiUiSection>& ui_sections) {
  std::vector<Suggestion> suggestions;
  suggestions.reserve(3);

  suggestions.emplace_back(SuggestionType::kSeparator);
  // TODO(crbug.com/393114125): Change to use `AutofillField::field_modifiers_`.
  if (trigger_field.is_autofilled_according_to_renderer()) {
    suggestions.emplace_back(CreateUndoSuggestion());
  }

  if (base::FeatureList::IsEnabled(
          features::kSuggestionManageButtonSplitForEnhancedAutofill)) {
    CHECK(!ui_sections.empty());
    if (ui_sections.size() == 1) {
      switch (*ui_sections.begin()) {
        case AutofillAiUiSection::kTravel:
          suggestions.emplace_back(CreateManageTravelSuggestion());
          return suggestions;
        case AutofillAiUiSection::kIdentityDocs:
          suggestions.emplace_back(CreateManageIdentityDocsSuggestion());
          return suggestions;
        case AutofillAiUiSection::kShopping:
          suggestions.emplace_back(CreateManageShoppingSuggestion());
          return suggestions;
      }
    }
  }

  // Graceful fallback for everything else:
  // - Features disabled
  // - ui_sections has multiple items
  // - Unhandled AutofillAiUiSection enums
  suggestions.emplace_back(CreateManageAutofillAiSuggestion());
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
    std::string_view app_locale) {
  std::vector<const EntityInstance*> entities =
      base::ToVector(entities_to_suggest,
                     [](const EntityInstance& entity) { return &entity; });
  entities.insert(entities.end(), other_entities_that_can_fill_section.begin(),
                  other_entities_that_can_fill_section.end());

  std::vector<EntityLabel> labels =
      GetLabelsForEntities(entities, trigger_field_attributes,
                           /*only_disambiguating_types=*/true,
                           /*obfuscate_sensitive_types=*/false, app_locale);

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

// Returns a map of the minimum length of each masked attribute across all
// entities.
absl::flat_hash_map<AttributeType, size_t> GetAttributeMaskLengths(
    base::span<const EntityInstance* const> entities) {
  absl::flat_hash_map<AttributeType, size_t> lengths;
  for (const EntityInstance* const entity : entities) {
    for (const AttributeInstance& attribute : entity->attributes()) {
      if (!attribute.masked()) {
        continue;
      }
      const std::u16string value = attribute.GetCompleteRawInfo();
      if (value.empty()) {
        continue;
      }
      auto [it, inserted] = lengths.insert({attribute.type(), value.size()});
      if (!inserted) {
        it->second = std::min(it->second, value.size());
      }
    }
  }
  return lengths;
}

// Returns entities whose set of fields and values to be filled are not subsets
// of another. This function favors server entities, for example if two entities
// (one being local and one being server) are going to fill the same fields with
// the same values, this function will keep the server one. Note that `entities`
// is expected to be sorted by descending priority and favor higher-priority
// suggestions.
std::vector<const EntityInstance*> DedupedEntitiesForSuggestions(
    const std::vector<const EntityInstance*>& entities,
    const AttributeTypeAssignment& type_assignment,
    const std::string& app_locale) {
  // If any of the attributes are masked, only compare the suffixes or the
  // minimum length of the attributes.
  const absl::flat_hash_map<AttributeType, size_t> mask_lengths =
      GetAttributeMaskLengths(entities);
  // Returns the suffix of `value` whose length is `mask_length` if
  // `attribute_type` is masked, otherwise returns `value` unchanged.
  auto maybe_take_suffix = [&mask_lengths](AttributeType attribute_type,
                                           std::u16string value) {
    if (const auto it = mask_lengths.find(attribute_type);
        it != mask_lengths.end()) {
      const size_t mask_length = std::min(it->second, value.size());
      return value.substr(value.size() - mask_length);
    }
    return value;
  };

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

      field_to_values.emplace_back(
          field->global_id(),
          maybe_take_suffix(attribute_type, std::move(attribute_value)));
    }
  }

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
      // - `i` is equal to `j` and `j` has higher priority than `i`.
      // - `i` is equal to `j` and they have the same priority, but `j`
      //   appears earlier in the list (higher frecency).
      const bool i_is_proper_subset_of_j = j_includes_i && !j_equals_i;
      const int i_priority = GetRecordTypePriority(entities[i]->record_type());
      const int j_priority = GetRecordTypePriority(entities[j]->record_type());
      if (i_is_proper_subset_of_j ||
          (j_equals_i &&
           (j_priority > i_priority || (j_priority == i_priority && i > j)))) {
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

Suggestion::Icon GetSuggestionIcon(
    EntityType trigger_entity_type,
    EntityInstance::RecordType trigger_entity_record_type) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  if (base::FeatureList::IsEnabled(
          features::kAutofillAiNoFillingIconsExperiment)) {
    return Suggestion::Icon::kNoIcon;
  }
#endif
  const bool is_personal_context = trigger_entity_record_type ==
                                   EntityInstance::RecordType::kPersonalContext;
  switch (trigger_entity_type.name()) {
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kNationalIdCard:
      return is_personal_context ? Suggestion::Icon::kIdCardSpark
                                 : Suggestion::Icon::kIdCard;
    case EntityTypeName::kFlightReservation:
      return is_personal_context ? Suggestion::Icon::kFlightSpark
                                 : Suggestion::Icon::kFlight;
    case EntityTypeName::kOrder:
      return is_personal_context ? Suggestion::Icon::kOrderSpark
                                 : Suggestion::Icon::kOrder;
    case EntityTypeName::kPassport:
      if (is_personal_context) {
        return Suggestion::Icon::kPassportSpark;
      }
      return base::FeatureList::IsEnabled(
                 features::kAutofillAiWalletPrivatePasses)
                 ? Suggestion::Icon::kPassport
                 : Suggestion::Icon::kIdCard;
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      return is_personal_context ? Suggestion::Icon::kIdCard2Spark
                                 : Suggestion::Icon::kIdCard2;
    case EntityTypeName::kVehicle:
      return is_personal_context ? Suggestion::Icon::kVehicleSpark
                                 : Suggestion::Icon::kVehicle;
    case EntityTypeName::kShipment:
      return is_personal_context ? Suggestion::Icon::kShipmentSpark
                                 : Suggestion::Icon::kShipment;
  }
  NOTREACHED();
}

AutofillAiUiSection GetAutofillAiUiSection(EntityType trigger_entity_type) {
  switch (trigger_entity_type.name()) {
    case EntityTypeName::kFlightReservation:
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
    case EntityTypeName::kVehicle:
      return AutofillAiUiSection::kTravel;
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kNationalIdCard:
    case EntityTypeName::kPassport:
      return AutofillAiUiSection::kIdentityDocs;
    case EntityTypeName::kOrder:
    case EntityTypeName::kShipment:
      return AutofillAiUiSection::kShopping;
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
    const FormStructure& form,
    const EntityInstance& entity,
    base::span<const AutofillFieldWithAttributeType> fields,
    const AutofillFieldWithAttributeType& trigger_field,
    std::u16string label,
    std::string_view app_locale) {
  // The dereference is guaranteed by EntityShouldProduceSuggestion().
  const AttributeInstance& trigger_attribute =
      *entity.attribute(trigger_field.type);
  std::u16string main_text = trigger_attribute.GetInfo(
      trigger_field.field->Type().GetAutofillAiType(
          trigger_attribute.type().entity_type()),
      app_locale, trigger_field.field->format_string());
  if (trigger_attribute.type().is_obfuscated()) {
    main_text = GetObfuscatedValue(main_text, /*visible_suffix_length=*/4);
  }

  Suggestion suggestion =
      Suggestion(main_text, SuggestionType::kFillAutofillAi);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (entity.record_type() == EntityInstance::RecordType::kPersonalContext) {
    suggestion.labels = {{Suggestion::Text(std::move(label))},
                         {Suggestion::Text(l10n_util::GetStringUTF16(
                             IDS_AUTOFILL_AI_SUGGESTED_BY_GEMINI))}};
  } else {
    suggestion.labels = {{Suggestion::Text(std::move(label))}};
  }
#else
  suggestion.labels = {{Suggestion::Text(std::move(label))}};
#endif

  const bool requires_server_fetch = WillRequireServerFetch(
      entity, form, trigger_field.field->section(), app_locale);
  suggestion.payload =
      Suggestion::AutofillAiPayload(entity.guid(), requires_server_fetch);
  suggestion.icon = GetSuggestionIcon(entity.type(), entity.record_type());
  if (entity.record_type() == EntityInstance::RecordType::kServerWallet) {
    suggestion.iph_metadata = Suggestion::IPHMetadata(
        &feature_engagement::kIPHAutofillAiValuablesFeature);
  }
  return suggestion;
}

// Compares two entities of the same type and record type.
//
// Personal Context entities are sorted by entity-type-specific
// attributes, preferring entities that have the attribute.
// Other record types are sorted by `FrecencyOrder`.
bool CompareSameTypeEntities(const EntityInstance& lhs,
                             const EntityInstance& rhs) {
  const EntityInstance::FrecencyOrder frecency_order(base::Time::Now());

  if (lhs.record_type() != EntityInstance::RecordType::kPersonalContext) {
    return frecency_order(lhs, rhs);
  }

  auto get_attr_value =
      [](const EntityInstance& entity,
         AttributeTypeName attr) -> std::optional<std::u16string> {
    if (base::optional_ref<const AttributeInstance> attr_inst =
            entity.attribute(AttributeType(attr))) {
      return attr_inst->GetRawInfo(attr_inst->type().field_type());
    }
    return std::nullopt;
  };

  auto compare_by_attribute = [&frecency_order, &lhs, &rhs, &get_attr_value](
                                  AttributeTypeName attr_name,
                                  auto string_comp) -> bool {
    const std::optional<std::u16string> lhs_val =
        get_attr_value(lhs, attr_name);
    const std::optional<std::u16string> rhs_val =
        get_attr_value(rhs, attr_name);
    if (lhs_val.has_value() != rhs_val.has_value()) {
      return lhs_val.has_value();
    }
    if (lhs_val.has_value() && *lhs_val != *rhs_val) {
      return string_comp(*lhs_val, *rhs_val);
    }
    return frecency_order(lhs, rhs);
  };


  auto compare_greater = [&compare_by_attribute](AttributeTypeName attr_name) {
    return compare_by_attribute(attr_name, std::greater<std::u16string>());
  };

  auto compare_less_case_insensitive =
      [&compare_by_attribute](AttributeTypeName attr_name) {
        return compare_by_attribute(
            attr_name, [](const std::u16string& a, const std::u16string& b) {
              return base::i18n::FoldCase(a) < base::i18n::FoldCase(b);
            });
      };

  switch (lhs.type().name()) {
    case EntityTypeName::kPassport:
      return compare_greater(AttributeTypeName::kPassportExpirationDate);
    case EntityTypeName::kDriversLicense:
      return compare_greater(AttributeTypeName::kDriversLicenseExpirationDate);
    case EntityTypeName::kNationalIdCard:
      return compare_greater(AttributeTypeName::kNationalIdCardExpirationDate);
    case EntityTypeName::kFlightReservation:
      return compare_greater(
          AttributeTypeName::kFlightReservationDepartureDate);
    case EntityTypeName::kVehicle:
      return compare_less_case_insensitive(
          AttributeTypeName::kVehiclePlateNumber);
    case EntityTypeName::kOrder:
      return compare_greater(AttributeTypeName::kOrderDate);
    case EntityTypeName::kShipment:
      return compare_greater(AttributeTypeName::kShipmentShippedDate);
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      return frecency_order(lhs, rhs);
  }
}
// The desired ordering criteria are the following:
// - Entities are first partitioned into groups sorted by their record type
//   priority.
// - Within each partition, entities of the same type appear together.
// - Within each partition, entities of type A appear before entities of type B
//   if the most "frecent" entity of type A is more frecent than the most
//   frecent entity of type B.
//
// In other terms, entities are grouped by record type priority and then entity
// type so that the most “frecent” suggestion in each partition is shown first,
// followed by all suggestions of the same type, then the next most “frecent”
// suggestion, and so on.
std::vector<const EntityInstance*> OrderedEntitiesForSuggestion(
    std::vector<const EntityInstance*> entities) {
  if (entities.empty()) {
    return entities;
  }

  const EntityInstance::FrecencyOrder frecency_order(base::Time::Now());
  using EntityGroup = std::pair<EntityInstance::RecordType, EntityType>;
  std::map<EntityGroup, const EntityInstance*> most_frecent_per_group;
  for (const EntityInstance* entity : entities) {
    const EntityGroup key = {entity->record_type(), entity->type()};
    const EntityInstance* best = most_frecent_per_group[key];
    if (!best || frecency_order(*entity, *best)) {
      most_frecent_per_group[key] = entity;
    }
  }
  std::ranges::sort(
      entities, [&frecency_order, &most_frecent_per_group](
                    const EntityInstance* lhs, const EntityInstance* rhs) {
        if (lhs->record_type() != rhs->record_type()) {
          return GetRecordTypePriority(lhs->record_type()) >
                 GetRecordTypePriority(rhs->record_type());
        }

        if (lhs->type() != rhs->type()) {
          const EntityInstance* lhs_best =
              most_frecent_per_group.at({lhs->record_type(), lhs->type()});
          const EntityInstance* rhs_best =
              most_frecent_per_group.at({rhs->record_type(), rhs->type()});
          return frecency_order(*lhs_best, *rhs_best);
        }

        return CompareSameTypeEntities(*lhs, *rhs);
      });

  return entities;
}

// Returns a valid GURL parsed from a domain string. If no scheme is present in
// `domain`, prepends "https://" to allow GURL to parse it and retrieve a host.
GURL GetGURLFromDomain(std::u16string_view domain) {
  const std::string domain_str = base::UTF16ToUTF8(domain);
  if (!domain_str.contains("://")) {
    return GURL(base::StrCat(
        {url::kHttpsScheme, url::kStandardSchemeSeparator, domain_str}));
  }
  return GURL(domain_str);
}

// Returns the domain-specific AttributeType for domain-constrained entity types
// (e.g., kOrder, kShipment), or std::nullopt if the entity type is not
// domain-constrained.
std::optional<AttributeType> GetDomainFilterAttributeType(
    const EntityType& type) {
  switch (type.name()) {
    case EntityTypeName::kOrder:
      return AttributeType(AttributeTypeName::kOrderMerchantDomain);
    case EntityTypeName::kShipment:
      return AttributeType(AttributeTypeName::kShipmentCarrierDomain);
    case EntityTypeName::kPassport:
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kVehicle:
    case EntityTypeName::kNationalIdCard:
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
    case EntityTypeName::kFlightReservation:
      return std::nullopt;
  }
}

// Returns whether the `entity` is allowed to be suggested on `page_url`.
// For domain-constrained entity types (such as `kOrder` or `kShipment`), this
// requires their associated domain to match the domain of `page_url` (via PSL
// matching). For other entity types, returns true.
bool IsAllowedForPageUrl(const EntityInstance& entity, const GURL& page_url) {
  std::optional<AttributeType> domain_attr_type =
      GetDomainFilterAttributeType(entity.type());
  if (!domain_attr_type) {
    return true;
  }
  base::optional_ref<const AttributeInstance> domain_attr =
      entity.attribute(*domain_attr_type);
  if (!domain_attr) {
    return false;
  }
  const std::u16string domain_val = domain_attr->GetCompleteRawInfo();
  if (domain_val.empty()) {
    return false;
  }
  return affiliations::IsExtendedPublicSuffixDomainMatch(
      GetGURLFromDomain(domain_val), page_url, {});
}

std::vector<const EntityInstance*> GetEntitiesForSuggestion(
    std::vector<const EntityInstance*> entities,
    const AttributeTypeAssignment& assignment,
    const FieldGlobalId& trigger_field_id,
    const std::string& app_locale,
    const GURL& page_url) {
  std::erase_if(entities, [&](const EntityInstance* entity) {
    base::optional_ref<const AutofillFieldWithAttributeType>
        trigger_field_with_type =
            FindField(assignment.Find(entity->type()), trigger_field_id);
    return !trigger_field_with_type ||
           !EntityShouldProduceSuggestion(*entity, *trigger_field_with_type,
                                          app_locale) ||
           !IsAllowedForPageUrl(*entity, page_url);
  });
  return DedupedEntitiesForSuggestions(
      OrderedEntitiesForSuggestion(std::move(entities)), assignment,
      app_locale);
}

std::vector<Suggestion> CreateFetchingAmbientSuggestions() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_FETCHING_AMBIENT_DATA),
      SuggestionType::kFetchingAmbientData);
  return PrepareLoadingStateSuggestions({suggestion}, suggestion);
}

// The Personal Context Notice suggestion is only supported on Desktop.
constexpr bool IsPersonalContextNoticeSuggestionSupported() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return false;
#else
  return true;
#endif
}

// Returns a list of `Suggestion` generated for the given
// `entities_to_suggest`.
std::vector<Suggestion> CreateSuggestionsForEntities(
    const FormStructure& form,
    const AutofillField& trigger_field,
    base::span<const EntityInstance> entities_to_suggest,
    base::span<const EntityInstance> all_entities,
    const AttributeTypeAssignment& assignment,
    AutofillClient& client) {
  if (entities_to_suggest.empty()) {
    return {};
  }

  auto entities_to_suggest_ids = base::MakeFlatSet<EntityInstance::EntityId>(
      entities_to_suggest, {}, &EntityInstance::guid);

  // Labels need to be consistent across the whole fill group. That is, as the
  // user clicks around fields they need to see the same set of attributes as
  // a combination of main text and labels. Therefore, entities that do not
  // generate suggestions on a certain triggering field still affect label
  // generation and should be taken into account.
  std::vector<const EntityInstance*> other_entities_that_can_fill_section;
  for (const EntityInstance& entity : all_entities) {
    if (!entities_to_suggest_ids.contains(entity.guid()) &&
        CanFillSomeField(entity, assignment.Find(entity.type()),
                         std::string(client.GetAppLocale()))) {
      other_entities_that_can_fill_section.push_back(&entity);
    }
  }

  std::vector<std::u16string> labels = GetLabelsForSuggestions(
      entities_to_suggest, other_entities_that_can_fill_section,
      FindAttributesForField(assignment, trigger_field.global_id()),
      client.GetAppLocale());

  std::vector<Suggestion> suggestions;
  suggestions.reserve(entities_to_suggest.size());
  CHECK_EQ(entities_to_suggest.size(), labels.size());
  for (auto [entity, label] : base::zip(entities_to_suggest, labels)) {
    base::span<const AutofillFieldWithAttributeType> fields_with_types =
        assignment.Find(entity.type());
    base::optional_ref<const AutofillFieldWithAttributeType>
        trigger_field_with_type =
            FindField(fields_with_types, trigger_field.global_id());
    suggestions.push_back(GetSuggestionForEntity(
        form, entity, fields_with_types, *trigger_field_with_type,
        std::move(label), client.GetAppLocale()));
  }
  return suggestions;
}

// Returns true if the trigger field can be filled by an entity of type
// `entity_type`. This is determined by checking if the
// `AttributeTypeAssignment` maps the field to any attribute belonging to that
// entity type.
bool CanFieldBeFilledByEntityType(const AttributeTypeAssignment& assignment,
                                  const FieldGlobalId& field_id,
                                  EntityType entity_type) {
  base::span<const AutofillFieldWithAttributeType> fields =
      assignment.Find(entity_type);
  return FindField(fields, field_id).has_value();
}

// Returns true if any suggestion in `suggestions` (or recursively in their
// children) was generated from an entity stored in Personal Context.
bool HasPersonalContextSuggestion(
    base::span<const Suggestion> suggestions,
    base::span<const EntityInstance> all_entities) {
  for (const Suggestion& suggestion : suggestions) {
    if (const auto* payload =
            std::get_if<Suggestion::AutofillAiPayload>(&suggestion.payload)) {
      auto it =
          std::ranges::find(all_entities, payload->guid, &EntityInstance::guid);
      if (it != all_entities.end() &&
          it->record_type() == EntityInstance::RecordType::kPersonalContext) {
        return true;
      }
    }
    if (HasPersonalContextSuggestion(suggestion.children, all_entities)) {
      return true;
    }
  }
  return false;
}

// Returns the parent `Suggestion` for a fallback menu which displays entities
// that do not match the current domain.
Suggestion CreateParentFallbackSuggestion(EntityType entity_type,
                                          bool has_primary_suggestions,
                                          std::vector<Suggestion> children) {
  switch (entity_type.name()) {
    case EntityTypeName::kOrder: {
      Suggestion suggestion(
          l10n_util::GetStringUTF16(has_primary_suggestions
                                        ? IDS_AUTOFILL_AI_OTHER_ORDERS
                                        : IDS_AUTOFILL_AI_ALL_ORDERS),
          SuggestionType::kAutofillAiOtherOrders);
      suggestion.acceptability =
          Suggestion::Acceptability::kSelectableButUnacceptable;
      suggestion.children = std::move(children);
      return suggestion;
    }
    case EntityTypeName::kShipment: {
      Suggestion suggestion(
          l10n_util::GetStringUTF16(has_primary_suggestions
                                        ? IDS_AUTOFILL_AI_OTHER_SHIPMENTS
                                        : IDS_AUTOFILL_AI_ALL_SHIPMENTS),
          SuggestionType::kAutofillAiOtherShipments);
      suggestion.acceptability =
          Suggestion::Acceptability::kSelectableButUnacceptable;
      suggestion.children = std::move(children);
      return suggestion;
    }
    case EntityTypeName::kPassport:
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kVehicle:
    case EntityTypeName::kNationalIdCard:
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
    case EntityTypeName::kFlightReservation:
      break;
  }
  NOTREACHED();
}

// Generates the fallback suggestion menu for entities of `entity_type`
// from other domains. This suggestion is marked as unacceptable
// (non-clickable) but contains children suggestions for all the fallback
// entities, which are regular acceptable suggestions.
// Updates `ui_sections` for any fallback entities that produce valid child
// suggestions.
// Returns `std::nullopt` if no valid fallback suggestions could be generated.
std::optional<Suggestion> CreateDomainFallbackSuggestion(
    const FormStructure& form,
    const AutofillField& trigger_field,
    EntityType entity_type,
    bool has_primary_suggestions,
    base::span<const EntityInstance> all_entities,
    const AttributeTypeAssignment& assignment,
    const GURL& page_url,
    AutofillClient& client,
    DenseSet<AutofillAiUiSection>* ui_sections) {
  if (!CanFieldBeFilledByEntityType(assignment, trigger_field.global_id(),
                                    entity_type)) {
    return std::nullopt;
  }

  std::vector<const EntityInstance*> fallback_entities;
  for (const EntityInstance& entity : all_entities) {
    if (entity.type() != entity_type || IsAllowedForPageUrl(entity, page_url)) {
      continue;
    }

    base::span<const AutofillFieldWithAttributeType> fields_with_types =
        assignment.Find(entity.type());
    base::optional_ref<const AutofillFieldWithAttributeType>
        trigger_field_with_type =
            FindField(fields_with_types, trigger_field.global_id());
    if (!trigger_field_with_type) {
      continue;
    }

    base::optional_ref<const AttributeInstance> trigger_attribute =
        entity.attribute(trigger_field_with_type->type);
    if (!trigger_attribute) {
      continue;
    }

    std::u16string trigger_value = trigger_attribute->GetInfo(
        trigger_field_with_type->field->Type().GetAutofillAiType(entity.type()),
        std::string(client.GetAppLocale()),
        trigger_field_with_type->field->format_string());
    if (!trigger_value.empty()) {
      fallback_entities.push_back(&entity);
    }
  }

  fallback_entities = DedupedEntitiesForSuggestions(
      OrderedEntitiesForSuggestion(std::move(fallback_entities)), assignment,
      std::string(client.GetAppLocale()));

  if (fallback_entities.empty()) {
    return std::nullopt;
  }

  std::vector<Suggestion> children = CreateSuggestionsForEntities(
      form, trigger_field,
      base::ToVector(fallback_entities,
                     [](const EntityInstance* entity) { return *entity; }),
      all_entities, assignment, client);
  if (children.empty()) {
    return std::nullopt;
  }

  for (const EntityInstance* entity : fallback_entities) {
    ui_sections->insert(GetAutofillAiUiSection(entity->type()));
  }

  return CreateParentFallbackSuggestion(entity_type, has_primary_suggestions,
                                        std::move(children));
}

void AppendDomainFallbackSuggestions(
    std::vector<Suggestion>& suggestions,
    const FormStructure& form,
    const AutofillField& trigger_field,
    base::span<const EntityInstance> entities_to_suggest,
    base::span<const EntityInstance> all_entities,
    const AttributeTypeAssignment& assignment,
    AutofillClient& client,
    DenseSet<AutofillAiUiSection>& ui_sections) {
  for (EntityType entity_type : DenseSet<EntityType>::all()) {
    switch (entity_type.name()) {
      case EntityTypeName::kOrder:
      case EntityTypeName::kShipment: {
        const bool has_primary_suggestions = std::ranges::any_of(
            entities_to_suggest, [&](const EntityInstance& entity) {
              return entity.type() == entity_type;
            });
        if (std::optional<Suggestion> fallback_suggestion =
                CreateDomainFallbackSuggestion(
                    form, trigger_field, entity_type, has_primary_suggestions,
                    all_entities, assignment,
                    client.GetLastCommittedPrimaryMainFrameURL(), client,
                    &ui_sections)) {
          suggestions.push_back(std::move(*fallback_suggestion));
        }
        break;
      }
      case EntityTypeName::kPassport:
      case EntityTypeName::kDriversLicense:
      case EntityTypeName::kVehicle:
      case EntityTypeName::kNationalIdCard:
      case EntityTypeName::kKnownTravelerNumber:
      case EntityTypeName::kRedressNumber:
      case EntityTypeName::kFlightReservation:
        break;
    }
  }
}

std::vector<Suggestion> CreateAutofillAiFillingSuggestions(
    const FormStructure& form,
    const AutofillField& trigger_field,
    base::span<const EntityInstance> entities_to_suggest,
    base::span<const EntityInstance> all_entities,
    const AttributeTypeAssignment& assignment,
    AutofillClient& client) {
  const DenseSet<EntityType> entity_types_being_fetched =
      GetEntityTypesBeingFetched(trigger_field, client);
  const bool should_show_fetching_suggestions =
      !entity_types_being_fetched.empty();

  std::vector<Suggestion> suggestions;
  DenseSet<AutofillAiUiSection> ui_sections;

  if (!entities_to_suggest.empty()) {
    suggestions =
        CreateSuggestionsForEntities(form, trigger_field, entities_to_suggest,
                                     all_entities, assignment, client);

    for (const EntityInstance& entity : entities_to_suggest) {
      ui_sections.insert(GetAutofillAiUiSection(entity.type()));
    }
  }

  AppendDomainFallbackSuggestions(suggestions, form, trigger_field,
                                  entities_to_suggest, all_entities, assignment,
                                  client, ui_sections);

  if (suggestions.empty() && !should_show_fetching_suggestions) {
    return {};
  }

  if (IsPersonalContextNoticeSuggestionSupported() &&
      HasPersonalContextSuggestion(suggestions, all_entities) &&
      client.ShouldShowPersonalContextAmbientAutofillNotice()) {
    Suggestion& suggestion =
        suggestions.emplace_back(SuggestionType::kPersonalContextNotice);
    suggestion.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  }

  if (should_show_fetching_suggestions) {
    base::Extend(suggestions, CreateFetchingAmbientSuggestions());
    for (EntityType entity_type : entity_types_being_fetched) {
      ui_sections.insert(GetAutofillAiUiSection(entity_type));
    }
  }

  base::Extend(suggestions, GetFooterSuggestions(trigger_field, ui_sections));
  return suggestions;
}

}  // namespace

AutofillAiSuggestionGenerator::AutofillAiSuggestionGenerator() = default;
AutofillAiSuggestionGenerator::~AutofillAiSuggestionGenerator() = default;

void AutofillAiSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void AutofillAiSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  const EntityDataManager* entity_manager = client.GetEntityDataManager();
  if (!entity_manager || !form_structure || !trigger_autofill_field) {
    callback({SuggestionDataSource::kAutofillAi, {}});
    return;
  }

  const bool is_fillable =
      GetFieldsFillableByAutofillAi(*form_structure, client)
          .contains(trigger_field.global_id());
  const bool is_fetching_data_for_field =
      !GetEntityTypesBeingFetched(*trigger_autofill_field, client).empty();

  if ((!is_fillable && !is_fetching_data_for_field) ||
      SuppressSuggestionsForAutocompleteUnrecognizedField(
          *trigger_autofill_field, GetAcUnrecognizedBehavior(client))) {
    callback({SuggestionDataSource::kAutofillAi, {}});
    return;
  }

  std::vector<const EntityInstance*> entities = GetEntitiesForSuggestion(
      GetFillableEntityInstances(client),
      AttributeTypeAssignment(form_structure->fields(),
                              trigger_autofill_field->section()),
      trigger_field.global_id(), client.GetAppLocale(),
      client.GetLastCommittedPrimaryMainFrameURL());

  std::vector<Suggestion> suggestions = CreateAutofillAiFillingSuggestions(
      *form_structure, *trigger_autofill_field,
      base::ToVector(entities,
                     [](const EntityInstance* entity) { return *entity; }),
      entity_manager->GetEntityInstances(),
      AttributeTypeAssignment(form_structure->fields(),
                              trigger_autofill_field->section()),
      client);

  callback({SuggestionDataSource::kAutofillAi, std::move(suggestions)});
}

}  // namespace autofill
