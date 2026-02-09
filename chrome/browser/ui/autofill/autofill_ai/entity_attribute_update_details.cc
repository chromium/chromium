// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/entity_attribute_update_details.h"

#include <optional>
#include <ostream>
#include <ranges>

#include "base/feature_list.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_import_utils.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

std::ostream& operator<<(std::ostream& os,
                         EntityAttributeUpdateType update_type) {
  switch (update_type) {
    case EntityAttributeUpdateType::kNewEntityAttributeAdded:
      return os << "NewEntityAttributeAdded";
    case EntityAttributeUpdateType::kNewEntityAttributeUpdated:
      return os << "NewEntityAttributeUpdated";
    case EntityAttributeUpdateType::kNewEntityAttributeUnchanged:
      return os << "NewEntityAttributeUnchanged";
  }
  return os;
}

EntityAttributeUpdateDetails::EntityAttributeUpdateDetails(
    std::u16string attribute_name,
    std::u16string attribute_value,
    std::optional<std::u16string> old_attribute_value,
    EntityAttributeUpdateType update_type)
    : attribute_name_(std::move(attribute_name)),
      attribute_value_(std::move(attribute_value)),
      old_attribute_value_(std::move(old_attribute_value)),
      update_type_(update_type) {}

EntityAttributeUpdateDetails::EntityAttributeUpdateDetails() = default;

EntityAttributeUpdateDetails::EntityAttributeUpdateDetails(
    const EntityAttributeUpdateDetails&) = default;

EntityAttributeUpdateDetails::EntityAttributeUpdateDetails(
    EntityAttributeUpdateDetails&&) = default;

EntityAttributeUpdateDetails& EntityAttributeUpdateDetails::operator=(
    const EntityAttributeUpdateDetails&) = default;

EntityAttributeUpdateDetails& EntityAttributeUpdateDetails::operator=(
    EntityAttributeUpdateDetails&&) = default;

EntityAttributeUpdateDetails::~EntityAttributeUpdateDetails() = default;

bool EntityAttributeUpdateDetails::operator==(
    const EntityAttributeUpdateDetails& other) const = default;

std::vector<EntityAttributeUpdateDetails>
EntityAttributeUpdateDetails::GetUpdatedAttributesDetails(
    const EntityInstance& new_entity,
    base::optional_ref<const EntityInstance> old_entity,
    const std::string& app_locale) {
  std::vector<EntityAttributeUpdateDetails> details;

  auto get_attribute_update_type =
      [&](const AttributeInstance& new_entity_attribute) {
        if (!old_entity) {
          return EntityAttributeUpdateType::kNewEntityAttributeAdded;
        }

        base::optional_ref<const AttributeInstance> old_entity_attribute =
            old_entity->attribute(new_entity_attribute.type());
        if (!old_entity_attribute) {
          return EntityAttributeUpdateType::kNewEntityAttributeAdded;
        }

        return std::ranges::all_of(
                   new_entity_attribute.type().field_subtypes(),
                   [&](FieldType type) {
                     return old_entity_attribute->GetInfo(
                                type, app_locale,
                                /*format_string=*/std::nullopt) ==
                            new_entity_attribute.GetInfo(
                                type, app_locale,
                                /*format_string=*/std::nullopt);
                   })
                   ? EntityAttributeUpdateType::kNewEntityAttributeUnchanged
                   : EntityAttributeUpdateType::kNewEntityAttributeUpdated;
      };

  auto get_attribute_value = [](const AttributeInstance& attribute,
                                const std::string& app_locale) {
    if (std::optional<std::u16string> date = MaybeGetLocalizedDate(attribute)) {
      return *date;
    } else {
      return attribute.GetCompleteInfo(app_locale);
    }
  };

  for (const AttributeInstance& attribute : new_entity.attributes()) {
    EntityAttributeUpdateType update_type =
        get_attribute_update_type(attribute);
    std::u16string attribute_value = get_attribute_value(attribute, app_locale);
    std::optional<std::u16string> old_attribute_value =
        update_type == EntityAttributeUpdateType::kNewEntityAttributeUpdated
            ? std::optional(get_attribute_value(
                  *old_entity->attribute(attribute.type()), app_locale))
            : std::nullopt;
    if (!attribute_value.empty()) {
      details.emplace_back(attribute.type().GetNameForI18n(), attribute_value,
                           old_attribute_value, update_type);
    }
  }

  if (base::FeatureList::IsEnabled(features::kAutofillAiNewUpdatePrompt)) {
    return details;
  }

  // Move new entity values that were either added or updated to the top.
  std::ranges::stable_sort(details, [](const EntityAttributeUpdateDetails& a,
                                       const EntityAttributeUpdateDetails& b) {
    // Returns true if `attribute` is a new entity attribute that was either
    // added or updated.
    auto added_or_updated = [](const EntityAttributeUpdateDetails& attribute) {
      return attribute.update_type() ==
                 EntityAttributeUpdateType::kNewEntityAttributeAdded ||
             attribute.update_type() ==
                 EntityAttributeUpdateType::kNewEntityAttributeUpdated;
    };
    if (added_or_updated(a) && !added_or_updated(b)) {
      return true;
    }

    if (!added_or_updated(a) && added_or_updated(b)) {
      return false;
    }
    return false;
  });
  return details;
}

std::ostream& operator<<(std::ostream& os,
                         const EntityAttributeUpdateDetails& details) {
  os << "{attribute name = " << details.attribute_name()
     << ", attribute value = " << details.attribute_value();
  if (details.old_attribute_value()) {
    os << ", old attribute value = " << *details.old_attribute_value();
  }
  os << ", update type = " << details.update_type() << "}";
  return os;
}

}  // namespace autofill
