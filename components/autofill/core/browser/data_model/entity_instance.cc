// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/entity_instance.h"

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

  // Returns -1 if `attribute_type` of `newer` should be set into `this`. This
  // would mean `newer` having an attribute that `this` does not. Returns 0 if
  // `attribute_type` of `newer` is empty but set on `this` or if they are the
  // have the same value, which means no change to `this` should be made for the
  // attribute. Returns 1 if `attribute_type` of `newer` is different from
  // `this`, which ultimately means an actual new entity, note that in this
  // cases both attributes exist.
  auto get_attribute_mergeability = [&](AttributeType attribute_type) {
    base::optional_ref<const AttributeInstance> attribute_1 =
        attribute(attribute_type);
    base::optional_ref<const AttributeInstance> attribute_2 =
        newer.attribute(attribute_type);

    // attribute does not exist on either entity.
    if (!attribute_1 && !attribute_2) {
      return 0;
    }

    // Attribute exists on `this` but not in `newer`.
    if (attribute_1 && !attribute_2) {
      return 0;
    }

    // Attribute exists on `newer` but not on `this`.
    if (!attribute_1 && attribute_2) {
      return -1;
    }

    const std::u16string attribute_value_1 =
        AutofillProfileComparator::NormalizeForComparison(attribute_1->value());
    const std::u16string attribute_value_2 =
        AutofillProfileComparator::NormalizeForComparison(attribute_2->value());

    // Attribute exists on `newer` but not on `this`.
    if (attribute_value_1.empty() && !attribute_value_2.empty()) {
      return -1;
    }

    if (!attribute_value_1.empty() && attribute_value_2.empty()) {
      return 0;
    }

    // Returns 1 if the attributes are different, which ultimately means no
    // merge should happen.
    return attribute_value_1 == attribute_value_2 ? 0 : 1;
  };

  bool is_subset = true;
  std::vector<AttributeInstance> mergeable_attributes;
  for (const AttributeType type : type_.attributes()) {
    int attribute_mergeability = get_attribute_mergeability(type);

    is_subset &= (attribute_mergeability == 0);
    if (attribute_mergeability == -1) {
      base::optional_ref<const AttributeInstance> new_attribute =
          newer.attribute(type);
      CHECK(new_attribute);
      mergeable_attributes.emplace_back(*new_attribute);
    } else if (attribute_mergeability == 1) {
      // If an attribute was found in `newer`, which DOES exist in `this` but is
      // different, `newer` is neither a subset, nor mergeable.
      mergeable_attributes.clear();
      break;
    }
  }

  return {std::move(mergeable_attributes), is_subset};
}

}  // namespace autofill
