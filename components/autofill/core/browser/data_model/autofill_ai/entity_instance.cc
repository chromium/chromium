// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

#include <algorithm>
#include <ranges>
#include <variant>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_utils.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/contact_info.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

namespace {

std::u16string NormalizeEntityValue(const AttributeInstance& attribute) {
  return normalization::NormalizeForComparison(
      attribute.GetRawInfo(attribute.type().field_type()));
}

std::u16string Format(std::u16string s,
                      base::optional_ref<const std::u16string> format_string) {
  if (!format_string) {
    return s;
  }

  // We parse the leading minus here rather than using `base::StringToInt()` to
  // avoid mixing signed and unsigned integers as this easily leads to
  // undefined behavior.
  std::u16string_view format = *format_string;
  bool suffix = false;
  if (format_string->starts_with(u"-")) {
    format = format.substr(1);
    suffix = true;
  }
  uint32_t offset = 0;
  if (!base::StringToUint(format, &offset)) {
    return s;
  }

  if (suffix) {
    offset = s.size() - offset;
    s = std::move(s).substr(offset < s.size() ? offset : 0);
  } else if (offset > 0) {
    s = std::move(s).substr(0, offset);
  }
  return s;
}

}  // namespace

AttributeInstance::AttributeInstance(AttributeType type)
    : type_(type), info_([&]() -> InfoStructure {
        switch (type.data_type()) {
          case AttributeType::DataType::kName:
            return NameInfo(/*alternative_names_supported=*/false);
          case AttributeType::DataType::kCountry:
            return CountryInfo();
          case AttributeType::DataType::kDate:
            return DateInfo();
          case AttributeType::DataType::kState:
            return StateInfo();
          case AttributeType::DataType::kString:
            return u"";
        }
      }()) {}

AttributeInstance::AttributeInstance(const AttributeInstance&) = default;
AttributeInstance& AttributeInstance::operator=(const AttributeInstance&) =
    default;
AttributeInstance::AttributeInstance(AttributeInstance&&) = default;
AttributeInstance& AttributeInstance::operator=(AttributeInstance&&) = default;
AttributeInstance::~AttributeInstance() = default;

std::u16string AttributeInstance::GetInfo(
    FieldType field_type,
    const std::string& app_locale,
    base::optional_ref<const std::u16string> format_string) const {
  field_type = GetNormalizedFieldType(field_type);
  return std::visit(
      absl::Overload{[&](const CountryInfo& country) {
                       return country.GetCountryName(app_locale);
                     },
                     [&](const DateInfo& date) {
                       // TODO(crbug.com/396325496): Consider falling back
                       // to a locale-specific format by relying on
                       // `app_locale`.
                       return date.GetDate(format_string ? *format_string
                                                         : u"YYYY-MM-DD");
                     },
                     [&](const NameInfo&) { return GetRawInfo(field_type); },
                     [&](const StateInfo&) { return GetRawInfo(field_type); },
                     [&](const std::u16string&) {
                       return Format(GetRawInfo(field_type), format_string);
                     }},
      info_);
}

std::u16string AttributeInstance::GetRawInfo(FieldType field_type) const {
  field_type = GetNormalizedFieldType(field_type);
  return std::visit(
      absl::Overload{
          [&](const CountryInfo& country) {
            return base::UTF8ToUTF16(country.GetCountryCode());
          },
          [&](const DateInfo& date) { return date.GetDate(u"YYYY-MM-DD"); },
          [&](const NameInfo& name) {
            if (!name.GetSupportedTypes().contains(field_type)) {
              return std::u16string();
            }
            return name.GetRawInfo(field_type);
          },
          [&](const StateInfo& state) { return state.value(); },
          [&](const std::u16string& value) { return value; }},
      info_);
}

VerificationStatus AttributeInstance::GetVerificationStatus(
    FieldType field_type) const {
  field_type = GetNormalizedFieldType(field_type);
  return std::visit(
      absl::Overload{
          [&](const CountryInfo&) { return VerificationStatus::kNoStatus; },
          [&](const DateInfo&) { return VerificationStatus::kNoStatus; },
          [&](const NameInfo& name) {
            if (!name.GetSupportedTypes().contains(field_type)) {
              return VerificationStatus::kNoStatus;
            }
            return name.GetVerificationStatus(field_type);
          },
          [&](const StateInfo&) { return VerificationStatus::kNoStatus; },
          [&](const std::u16string&) { return VerificationStatus::kNoStatus; }},
      info_);
}

void AttributeInstance::SetInfo(FieldType field_type,
                                const std::u16string& value,
                                const std::string& app_locale,
                                std::u16string_view format_string,
                                VerificationStatus status) {
  field_type = GetNormalizedFieldType(field_type);
  std::visit(
      absl::Overload{
          [&](CountryInfo& country) {
            // We assume that the given `value` is either a valid country code
            // or a valid country name localized to the provided `app_locale`.
            if (!country.SetCountryFromCountryCode(value) &&
                !country.SetCountryFromCountryName(value, app_locale)) {
              // In case `value` turns out to be neither of the two options
              // mentioned above, we reset the country value to indicate
              // failure.
              country = CountryInfo();
            }
          },
          [&](DateInfo& date) { date.SetDate(value, format_string); },
          [&](NameInfo& name) {
            if (!name.GetSupportedTypes().contains(field_type)) {
              return;
            }
            name.SetInfoWithVerificationStatus(AutofillType(field_type), value,
                                               app_locale, status);
          },
          [&](const StateInfo&) { SetRawInfo(field_type, value, status); },
          [&](std::u16string&) { SetRawInfo(field_type, value, status); }},
      info_);
}

void AttributeInstance::SetRawInfo(FieldType field_type,
                                   const std::u16string& value,
                                   VerificationStatus status) {
  field_type = GetNormalizedFieldType(field_type);
  std::visit(absl::Overload{
                 [&](CountryInfo& country) {
                   if (!country.SetCountryFromCountryCode(value)) {
                     // In case `value` isn't a valid country code, we reset the
                     // country value to indicate failure.
                     country = CountryInfo();
                   }
                 },
                 [&](DateInfo& date) { date.SetDate(value, u"YYYY-MM-DD"); },
                 [&](NameInfo& name) {
                   if (!name.GetSupportedTypes().contains(field_type)) {
                     return;
                   }
                   name.SetRawInfoWithVerificationStatus(field_type, value,
                                                         status);
                 },
                 [&](StateInfo& state) { state = StateInfo(value); },
                 [&](std::u16string& old_value) { old_value = value; }},
             info_);
}

FieldType AttributeInstance::GetNormalizedFieldType(
    FieldType field_type) const {
  return type_.field_subtypes().contains(field_type) ? field_type
                                                     : type_.field_type();
}

void AttributeInstance::FinalizeInfo() {
  std::visit(
      absl::Overload{[&](const CountryInfo&) {}, [&](const DateInfo&) {},
                     [&](NameInfo& name) { name.FinalizeAfterImport(); },
                     [&](const StateInfo&) {}, [&](const std::u16string&) {}},
      info_);
}

EntityInstance::EntityInstance(
    EntityType type,
    base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
        attributes,
    EntityId guid,
    std::string nickname,
    base::Time date_modified,
    size_t use_count,
    base::Time use_date,
    RecordType record_type,
    AreAttributesReadOnly are_attributes_read_only)
    : type_(type),
      attributes_(std::move(attributes)),
      guid_(std::move(guid)),
      nickname_(std::move(nickname)),
      date_modified_(date_modified),
      use_count_(use_count),
      use_date_(use_date),
      record_type_(record_type),
      are_attributes_read_only_(are_attributes_read_only) {
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
  os << a.type() << ": " << '"'
     << a.GetInfo(a.type().field_type(), /*app_locale=*/"en-US",
                  /*format_string=*/std::nullopt)
     << '"';
  return os;
}

std::ostream& operator<<(std::ostream& os, const EntityInstance& e) {
  os << "- name: " << '"' << e.type() << '"' << std::endl;
  os << "- nickname: " << '"' << e.nickname() << '"' << std::endl;
  os << "- guid: " << '"' << e.guid() << '"' << std::endl;
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

void EntityInstance::RecordEntityUsed(base::Time date) {
  use_date_ = date;
  ++use_count_;
}

EntityInstance::EntityMergeability EntityInstance::GetEntityMergeability(
    const EntityInstance& newer) const {
  CHECK_EQ(type_, newer.type());

  // If a certain set of mergeable constraints for both entities have the same
  // values, we consider them to be the same entity. This affects how we handle
  // attributes with different values. For entities that are not the same, this
  // will lead to  `newer` being a fresh new entity, otherwise we chose the
  // attribute of `newer` as a mergeable attribute to eventually override the
  // value of `this`.
  const bool is_same_entity = [&]() {
    return std::ranges::any_of(
        type_.merge_constraints(),
        [&](const DenseSet<AttributeType>& constraints) {
          return std::ranges::all_of(constraints, [&](AttributeType type) {
            base::optional_ref<const AttributeInstance> attribute_1 =
                attribute(type);
            base::optional_ref<const AttributeInstance> attribute_2 =
                newer.attribute(type);
            return attribute_1 && attribute_2 &&
                   NormalizeEntityValue(*attribute_1) ==
                       NormalizeEntityValue(*attribute_2);
          });
        });
  }();

  const bool is_subset = [&]() {
    return std::ranges::all_of(type_.attributes(), [&](AttributeType type) {
      base::optional_ref<const AttributeInstance> attribute_1 = attribute(type);
      base::optional_ref<const AttributeInstance> attribute_2 =
          newer.attribute(type);
      return !attribute_2 ||
             (attribute_1 && NormalizeEntityValue(*attribute_1) ==
                                 NormalizeEntityValue(*attribute_2));
    });
  }();

  if (!is_same_entity) {
    return {{}, is_subset};
  }

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
        [&](base::optional_ref<const AttributeInstance> attribute_instance) {
          return !attribute_instance ||
                 NormalizeEntityValue(*attribute_instance).empty();
        };
    const bool is_attribute_1_empty = is_attribute_empty(attribute_1);
    const bool is_attribute_2_empty = is_attribute_empty(attribute_2);

    // Attribute does not exist on either entity.
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

    const std::u16string attribute_value_1 = NormalizeEntityValue(*attribute_1);
    const std::u16string attribute_value_2 = NormalizeEntityValue(*attribute_2);
    return attribute_value_1 == attribute_value_2
               ? AttributeMergeabilityResult::
                     kNewAndOldEntitiesHaveSameAttribute
               : AttributeMergeabilityResult::
                     kNewAndOldEntitiesHaveDifferentAttribute;
  };

  std::vector<AttributeInstance> mergeable_attributes;
  for (const AttributeType type : type_.attributes()) {
    AttributeMergeabilityResult attribute_mergeability =
        get_attribute_mergeability(type);

    if (attribute_mergeability ==
        AttributeMergeabilityResult::kNewEntityHasNewAttribute) {
      base::optional_ref<const AttributeInstance> new_attribute =
          newer.attribute(type);
      CHECK(new_attribute);
      mergeable_attributes.emplace_back(*new_attribute);
    } else if (attribute_mergeability ==
               AttributeMergeabilityResult::
                   kNewAndOldEntitiesHaveDifferentAttribute) {
      // Since the entities are already matching on some merge constraints,
      // always chooses the `newer` entity type as the new attribute in the ones
      // that differ.
      base::optional_ref<const AttributeInstance> new_attribute =
          newer.attribute(type);
      CHECK(new_attribute);
      mergeable_attributes.emplace_back(*new_attribute);
    }
  }

  return {std::move(mergeable_attributes), is_subset};
}

bool EntityInstance::IsSubsetOf(const EntityInstance& other) const {
  if (type_ != other.type_) {
    return false;
  }
  for (AttributeType type : type_.attributes()) {
    base::optional_ref<const AttributeInstance> this_attribute =
        attribute(type);
    base::optional_ref<const AttributeInstance> other_attribute =
        other.attribute(type);

    const std::u16string this_attribute_value =
        this_attribute ? NormalizeEntityValue(*this_attribute) : u"";
    const std::u16string other_attribute_value =
        other_attribute ? NormalizeEntityValue(*other_attribute) : u"";

    // Both entities do not have a value for a certain attribute, move forward
    // to check other attributes.
    if (this_attribute_value.empty() && other_attribute_value.empty()) {
      continue;
    }

    // `other` has a value for a certain attribute, while `this` does not, this
    // means `this` could be a subset of `other`, move forward to check other
    // attributes.
    if (this_attribute_value.empty() && !other_attribute_value.empty()) {
      continue;
    }

    // If the other entity does not have a certain attribute set, but `this`
    // does. `This` is not a subset of `other`.
    if (other_attribute_value.empty() && !this_attribute_value.empty()) {
      return false;
    }

    // Both `this` and `other` have different values stored for a certain
    // attribute, `this` is not a subset of `other`.
    if (this_attribute_value != other_attribute_value) {
      return false;
    }
  }
  return true;
}

EntityInstance::FrecencyOrder::FrecencyOrder(base::Time now) : now_(now) {}

bool EntityInstance::FrecencyOrder::operator()(
    const EntityInstance& lhs,
    const EntityInstance& rhs) const {
  // At days_since_last_use = 0, use_count = 0, the score is -1.
  // As days_since_last_use increases, the score becomes more negative.
  // As use_count increases, the score approaches 0.
  auto get_ranking_score = [&](const EntityInstance& entity) {
    int days_since_last_use = std::max(0, (now_ - entity.use_date()).InDays());
    // The numerator punishes old usages, since as days_since_last_use
    // grows, the score becomes smaller (note the negative sign). The
    // denominator softens this penalty by making it smaller the more often a
    // user has used an entity.
    return -log(static_cast<double>(days_since_last_use) + 2) /
           log(entity.use_count() + 2);
  };

  const double lhs_score = get_ranking_score(lhs);
  const double rhs_score = get_ranking_score(rhs);

  const double kEpsilon = 0.00001;
  if (std::fabs(lhs_score - rhs_score) > kEpsilon) {
    return lhs_score > rhs_score;
  }
  return lhs.use_date() > rhs.use_date();
}

}  // namespace autofill
