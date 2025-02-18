// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/entity_instance.h"

#include <algorithm>
#include <ranges>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/entity_type.h"

namespace autofill {

AttributeInstance::Context::Context() = default;
AttributeInstance::Context::Context(const AttributeInstance::Context&) =
    default;
AttributeInstance::Context& AttributeInstance::Context::operator=(
    const AttributeInstance::Context&) = default;
AttributeInstance::Context::Context(AttributeInstance::Context&&) = default;
AttributeInstance::Context& AttributeInstance::Context::operator=(
    AttributeInstance::Context&&) = default;
AttributeInstance::Context::~Context() = default;

AttributeInstance::AttributeInstance(AttributeType type,
                                     std::u16string value,
                                     Context context)
    : type_(type), value_(std::move(value)), context_(std::move(context)) {}

AttributeInstance::AttributeInstance(const AttributeInstance&) = default;
AttributeInstance& AttributeInstance::operator=(const AttributeInstance&) =
    default;
AttributeInstance::AttributeInstance(AttributeInstance&&) = default;
AttributeInstance& AttributeInstance::operator=(AttributeInstance&&) = default;
AttributeInstance::~AttributeInstance() = default;

std::u16string AttributeInstance::GetRawInfo(FieldType type) const {
  return value_;
}

void AttributeInstance::SetRawInfoWithVerificationStatus(
    FieldType type,
    const std::u16string& value,
    VerificationStatus status) {
  value_ = value;
}

std::u16string AttributeInstance::GetInfo(const AutofillType& type,
                                          const std::string& app_locale) const {
  return value_;
}

VerificationStatus AttributeInstance::GetVerificationStatus(
    FieldType type) const {
  return VerificationStatus::kNoStatus;
}

bool AttributeInstance::SetInfoWithVerificationStatus(
    const AutofillType& type,
    const std::u16string& value,
    const std::string& app_locale,
    const VerificationStatus status) {
  value_ = value;
  return true;
}

FieldTypeSet AttributeInstance::GetSupportedTypes() const {
  return {type_.field_type()};
}

bool operator==(const AttributeInstance& lhs, const AttributeInstance& rhs) {
  return lhs.context_ == rhs.context_ && lhs.type_ == rhs.type_ &&
         lhs.value_ == rhs.value_;
}

std::u16string AttributeInstance::NormalizedValue() const {
  return AutofillProfileComparator::NormalizeForComparison(value());
}

EntityInstance::EntityInstance(
    EntityType type,
    base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
        attributes,
    base::Uuid guid,
    std::string nickname,
    base::Time date_modified)
    : type_(type),
      attributes_(std::move(attributes)),
      guid_(std::move(guid)),
      nickname_(std::move(nickname)),
      date_modified_(date_modified) {
  DCHECK(!attributes_.empty());
  DCHECK(std::ranges::all_of(attributes_, [this](const AttributeInstance& a) {
    return type_ == a.type().entity_type();
  }));
}

EntityInstance::EntityInstance(const EntityInstance&) = default;
EntityInstance& EntityInstance::operator=(const EntityInstance&) = default;
EntityInstance::EntityInstance(EntityInstance&&) = default;
EntityInstance& EntityInstance::operator=(EntityInstance&&) = default;
EntityInstance::~EntityInstance() = default;

bool EntityInstance::ImportOrder(const EntityInstance& lhs,
                                 const EntityInstance& rhs) {
  return EntityType::ImportOrder(lhs.type(), rhs.type());
}

std::ostream& operator<<(std::ostream& os, const AttributeInstance& a) {
  os << a.type() << ": " << '"' << a.value() << '"';
  return os;
}

std::ostream& operator<<(std::ostream& os, const EntityInstance& e) {
  os << "- name: " << '"' << e.type() << '"' << std::endl;
  os << "- nickname: " << '"' << e.nickname() << '"' << std::endl;
  os << "- guid: " << '"' << e.guid().AsLowercaseString() << '"' << std::endl;
  os << "- date modified: " << '"' << e.date_modified() << '"' << std::endl;
  for (const AttributeInstance& a : e.attributes()) {
    os << "- attribute " << a << std::endl;
  }
  return os;
}

EntityInstance::EntityMergeability::EntityMergeability(
    std::vector<AttributeInstance> mergeable_attributes,
    bool is_subset)
    : mergeable_attributes(std::move(mergeable_attributes)),
      is_subset(is_subset) {}

EntityInstance::EntityMergeability::EntityMergeability() = default;

EntityInstance::EntityMergeability::EntityMergeability(
    const EntityInstance::EntityMergeability&) = default;

EntityInstance::EntityMergeability::EntityMergeability(
    EntityInstance::EntityMergeability&&) = default;

EntityInstance::EntityMergeability&
EntityInstance::EntityMergeability::operator=(
    const EntityInstance::EntityMergeability&) = default;

EntityInstance::EntityMergeability&
EntityInstance::EntityMergeability::operator=(
    EntityInstance::EntityMergeability&&) = default;

EntityInstance::EntityMergeability::~EntityMergeability() = default;

EntityInstance::EntityMergeability EntityInstance::GetEntityMergeability(
    const EntityInstance& newer) const {
  CHECK_EQ(type_, newer.type());

  enum class AttributeMergeabilityResult {
    // A new entity has an attribute that the old entity
    // (caller) does not have.
    kNewEntityHasNewAttribute,
    // A new and an old entity have an attribute with the
    // same value.
    kNewAndOldEntitiesHaveSameAttribute,
    // A new entity does not have an attribute while the old one has.
    kOldEntityHasAttribute,
    // A new and an old entity have an attribute with
    // different values.
    kNewAndOldEntitiesHaveDifferentAttribute,
  };

  auto get_attribute_mergeability = [&](AttributeType attribute_type) {
    base::optional_ref<const AttributeInstance> attribute_1 =
        attribute(attribute_type);
    base::optional_ref<const AttributeInstance> attribute_2 =
        newer.attribute(attribute_type);

    auto is_attribute_empty =
        [](base::optional_ref<const AttributeInstance> attribute_instance) {
          return !attribute_instance ||
                 attribute_instance->NormalizedValue().empty();
        };
    const bool is_attribute_1_empty = is_attribute_empty(attribute_1);
    const bool is_attribute_2_empty = is_attribute_empty(attribute_2);

    // attribute does not exist on either entity.
    if (is_attribute_1_empty && is_attribute_2_empty) {
      return AttributeMergeabilityResult::kNewAndOldEntitiesHaveSameAttribute;
    }

    // Attribute exists on `this` but not in `newer`.
    if (!is_attribute_1_empty && is_attribute_2_empty) {
      return AttributeMergeabilityResult::kOldEntityHasAttribute;
    }

    // Attribute exists on `newer` but not on `this`.
    if (is_attribute_1_empty && !is_attribute_2_empty) {
      return AttributeMergeabilityResult::kNewEntityHasNewAttribute;
    }

    const std::u16string attribute_value_1 = attribute_1->NormalizedValue();
    const std::u16string attribute_value_2 = attribute_2->NormalizedValue();
    // Returns 1 if the attributes are different, which ultimately means no
    // merge should happen.
    return attribute_value_1 == attribute_value_2
               ? AttributeMergeabilityResult::
                     kNewAndOldEntitiesHaveSameAttribute
               : AttributeMergeabilityResult::
                     kNewAndOldEntitiesHaveDifferentAttribute;
  };

  // If a certain set of mergeable constraints for both entities have the same
  // values, we consider them to be the same entity. This affects how we handle
  // attributes with different values. For entities that are not the same, this
  // will lead to  `newer` being a fresh new entity, otherwise we chose the
  // attribute of `newer` as a mergeable attribute to eventually override the
  // value of `this`.
  bool is_same_entity = [&]() {
    return std::ranges::any_of(
        type_.merge_constraints(),
        [&](const DenseSet<AttributeType>& constraints) {
          return std::ranges::all_of(constraints, [&](AttributeType type) {
            base::optional_ref<const AttributeInstance> attribute_1 =
                attribute(type);
            base::optional_ref<const AttributeInstance> attribute_2 =
                newer.attribute(type);
            return attribute_1 && attribute_2 &&
                   (attribute_1->NormalizedValue() ==
                    attribute_2->NormalizedValue());
          });
        });
  }();
  bool is_subset = true;
  std::vector<AttributeInstance> mergeable_attributes;
  for (const AttributeType type : type_.attributes()) {
    AttributeMergeabilityResult attribute_mergeability =
        get_attribute_mergeability(type);

    is_subset &=
        (attribute_mergeability ==
         AttributeMergeabilityResult::kNewAndOldEntitiesHaveSameAttribute) ||
        (attribute_mergeability ==
         AttributeMergeabilityResult::kOldEntityHasAttribute);
    if (attribute_mergeability ==
        AttributeMergeabilityResult::kNewEntityHasNewAttribute) {
      base::optional_ref<const AttributeInstance> new_attribute =
          newer.attribute(type);
      CHECK(new_attribute);
      mergeable_attributes.emplace_back(*new_attribute);
    } else if (attribute_mergeability ==
               AttributeMergeabilityResult::
                   kNewAndOldEntitiesHaveDifferentAttribute) {
      if (!is_same_entity) {
        // If both entities are not the same and an attribute was found in
        // `newer`, which DOES exist in `this` but is
        // different, `newer` is neither a subset, nor mergeable. This should
        // lead to a save prompt.
        mergeable_attributes.clear();
        break;
      } else {
        // If the entities are the same, always chooses the `newer` entity type
        // as the new attribute.
        base::optional_ref<const AttributeInstance> new_attribute =
            newer.attribute(type);
        CHECK(new_attribute);
        mergeable_attributes.emplace_back(*new_attribute);
      }
    }
  }

  return {std::move(mergeable_attributes), is_subset};
}

}  // namespace autofill
