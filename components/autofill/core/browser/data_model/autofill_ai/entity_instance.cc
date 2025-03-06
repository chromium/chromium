// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

#include <algorithm>
#include <ranges>

#include "base/functional/overloaded.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/pass_key.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/contact_info.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"

namespace autofill {

AttributeInstance::AttributeInstance(AttributeType type) : type_(type) {
  switch (type.name()) {
    case AttributeTypeName::kPassportName:
    case AttributeTypeName::kDriversLicenseName:
      info_ = NameInfo();
      break;
    case AttributeTypeName::kPassportCountry:
      info_ = CountryInfo();
      break;
    case AttributeTypeName::kPassportExpiryDate:
    case AttributeTypeName::kPassportIssueDate:
    case AttributeTypeName::kDriversLicenseExpirationDate:
    case AttributeTypeName::kDriversLicenseIssueDate:
      info_ = DateInfo();
      break;
    case AttributeTypeName::kPassportNumber:
    case AttributeTypeName::kVehicleOwner:
    case AttributeTypeName::kVehicleLicensePlate:
    case AttributeTypeName::kVehicleVin:
    case AttributeTypeName::kVehicleMake:
    case AttributeTypeName::kVehicleModel:
    case AttributeTypeName::kDriversLicenseRegion:
    case AttributeTypeName::kDriversLicenseNumber:
      info_ = u"";
      break;
  }
}

AttributeInstance::AttributeInstance(const AttributeInstance&) = default;
AttributeInstance& AttributeInstance::operator=(const AttributeInstance&) =
    default;
AttributeInstance::AttributeInstance(AttributeInstance&&) = default;
AttributeInstance& AttributeInstance::operator=(AttributeInstance&&) = default;
AttributeInstance::~AttributeInstance() = default;

std::u16string AttributeInstance::GetInfo(
    FieldType type,
    const std::string& app_locale,
    base::optional_ref<const std::u16string> format_string) const {
  type = GetNormalizedType(type);
  if (type == UNKNOWN_TYPE) {
    return u"";
  }
  return absl::visit(
      base::Overloaded{[&](const CountryInfo& country) {
                         CHECK_EQ(type, ADDRESS_HOME_COUNTRY);
                         return country.GetCountryName(app_locale);
                       },
                       [&](const DateInfo& date) {
                         // TODO(crbug.com/396325496): Consider falling back
                         // to a locale-specific format by relying on
                         // `app_locale`.
                         return date.GetDate(format_string ? *format_string
                                                           : u"YYYY-MM-DD");
                       },
                       [&](const NameInfo& name) {
                         return GetRawInfo(/*pass_key=*/{}, type);
                       },
                       [&](const std::u16string& value) {
                         return GetRawInfo(/*pass_key=*/{}, type);
                       }},
      info_);
}

std::u16string AttributeInstance::GetRawInfo(GetRawInfoPassKey,
                                             FieldType type) const {
  type = GetNormalizedType(type);
  if (type == UNKNOWN_TYPE) {
    return u"";
  }
  return absl::visit(
      base::Overloaded{
          [&](const CountryInfo& country) {
            CHECK_EQ(type, ADDRESS_HOME_COUNTRY);
            return base::UTF8ToUTF16(country.GetCountryCode());
          },
          [&](const DateInfo& date) {
            CHECK(IsDateFieldType(type));
            return date.GetDate(u"YYYY-MM-DD");
          },
          [&](const NameInfo& name) { return name.GetRawInfo(type); },
          [&](const std::u16string& value) {
            CHECK_EQ(type, type_.field_type());
            return value;
          }},
      info_);
}

VerificationStatus AttributeInstance::GetVerificationStatus(
    FieldType type) const {
  type = GetNormalizedType(type);
  if (type == UNKNOWN_TYPE) {
    return VerificationStatus::kNoStatus;
  }
  return absl::visit(base::Overloaded{[&](const CountryInfo& country) {
                                        CHECK_EQ(type, ADDRESS_HOME_COUNTRY);
                                        return VerificationStatus::kNoStatus;
                                      },
                                      [&](const DateInfo& date) {
                                        CHECK(IsDateFieldType(type));
                                        return VerificationStatus::kNoStatus;
                                      },
                                      [&](const NameInfo& name) {
                                        return name.GetVerificationStatus(type);
                                      },
                                      [&](const std::u16string& value) {
                                        CHECK_EQ(type, type_.field_type());
                                        return VerificationStatus::kNoStatus;
                                      }},
                     info_);
}

void AttributeInstance::SetInfo(FieldType type,
                                const std::u16string& value,
                                const std::string& app_locale,
                                std::u16string_view format_string,
                                VerificationStatus status) {
  type = GetNormalizedType(type);
  if (type == UNKNOWN_TYPE) {
    return;
  }
  absl::visit(base::Overloaded{
                  [&](CountryInfo& country) {
                    CHECK_EQ(type, ADDRESS_HOME_COUNTRY);
                    // We assume that the given `value` is either a valid
                    // country code or a valid country name localized to the
                    // provided `app_locale`.
                    if (!country.SetCountryFromCountryCode(value) &&
                        !country.SetCountryFromCountryName(value, app_locale)) {
                      // In case `value` turns out to be neither of the two
                      // options mentioned above, we reset the country value to
                      // indicate failure.
                      country = CountryInfo();
                    }
                  },
                  [&](DateInfo& date) {
                    CHECK(IsDateFieldType(type));
                    date.SetDate(value, format_string);
                  },
                  [&](NameInfo& name) {
                    name.SetInfoWithVerificationStatus(type, value, app_locale,
                                                       status);
                  },
                  [&](std::u16string& old_value) {
                    SetRawInfo(type, value, status);
                  }},
              info_);
}

void AttributeInstance::SetRawInfo(FieldType type,
                                   const std::u16string& value,
                                   VerificationStatus status) {
  type = GetNormalizedType(type);
  if (type == UNKNOWN_TYPE) {
    return;
  }
  absl::visit(base::Overloaded{
                  [&](CountryInfo& country) {
                    CHECK_EQ(type, ADDRESS_HOME_COUNTRY);
                    if (!country.SetCountryFromCountryCode(value)) {
                      // In case `value` isn't a valid country
                      // code, we reset the country value to
                      // indicate failure.
                      country = CountryInfo();
                    }
                  },
                  [&](DateInfo& date) {
                    CHECK(IsDateFieldType(type));
                    date.SetDate(value, u"YYYY-MM-DD");
                  },
                  [&](NameInfo& name) {
                    name.SetRawInfoWithVerificationStatus(type, value, status);
                  },
                  [&](std::u16string& old_value) {
                    CHECK_EQ(type, type_.field_type());
                    old_value = value;
                  }},
              info_);
}

FieldTypeSet AttributeInstance::GetSupportedTypes() const {
  return absl::visit(
      base::Overloaded{
          [&](const CountryInfo&) {
            return FieldTypeSet{ADDRESS_HOME_COUNTRY};
          },
          [&](const DateInfo& date) {
            return FieldTypeSet{type_.field_type()};
          },
          [&](const NameInfo& name) { return name.GetSupportedTypes(); },
          [&](const std::u16string&) {
            return FieldTypeSet{type_.field_type()};
          }},
      info_);
}

FieldTypeSet AttributeInstance::GetDatabaseStoredTypes() const {
  return absl::visit(
      base::Overloaded{
          [&](const CountryInfo&) {
            return FieldTypeSet{ADDRESS_HOME_COUNTRY};
          },
          [&](const DateInfo&) { return FieldTypeSet{type_.field_type()}; },
          [&](const NameInfo&) { return NameInfo::kDatabaseStoredTypes; },
          [&](const std::u16string&) {
            return FieldTypeSet{type_.field_type()};
          }},
      info_);
}

FieldType AttributeInstance::GetNormalizedType(FieldType info_type) const {
  if (GetSupportedTypes().contains(info_type)) {
    return info_type;
  }
  if (info_type == type_.field_type()) {
    // In some cases, a field might have `AutofillField::Type()` being the one
    // corresponding to a structured attribute (e.g., PASSPORT_NAME_TAG). This
    // should not usually happen but for now can, only in case a field couldn't
    // be classified by Autofill's logic but was classified by the ML model. In
    // that case, we assume the type is the top-level type of the attribute.
    return absl::visit(
        base::Overloaded{
            [&](const CountryInfo&) { return ADDRESS_HOME_COUNTRY; },
            [&](const DateInfo& date) { return type().field_type(); },
            [&](const NameInfo&) { return NAME_FULL; },
            [&](const std::u16string&) { return type().field_type(); }},
        info_);
  }
  // In case the field classification is totally unrelated to the
  // attribute type classification, we return UKNOWN_TYPE to inform callers of
  // that.
  return UNKNOWN_TYPE;
}

void AttributeInstance::FinalizeInfo() {
  absl::visit(
      base::Overloaded{[&](const CountryInfo& country) {},
                       [&](const DateInfo& date) {},
                       [&](NameInfo& name) { name.FinalizeAfterImport(); },
                       [&](const std::u16string&) {}},
      info_);
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
  os << a.type() << ": " << '"'
     << a.GetInfo(a.type().field_type(), /*app_locale=*/"en-US",
                  /*format_string=*/std::nullopt)
     << '"';
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

  auto normalized_value = [](const AttributeInstance& attribute) {
    return AutofillProfileComparator::NormalizeForComparison(
        attribute.GetRawInfo(/*pass_key=*/{}, attribute.type().field_type()));
  };

  auto get_attribute_mergeability = [&](AttributeType attribute_type) {
    base::optional_ref<const AttributeInstance> attribute_1 =
        attribute(attribute_type);
    base::optional_ref<const AttributeInstance> attribute_2 =
        newer.attribute(attribute_type);

    auto is_attribute_empty =
        [&](base::optional_ref<const AttributeInstance> attribute_instance) {
          return !attribute_instance ||
                 normalized_value(*attribute_instance).empty();
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

    const std::u16string attribute_value_1 = normalized_value(*attribute_1);
    const std::u16string attribute_value_2 = normalized_value(*attribute_2);
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
                   normalized_value(*attribute_1) ==
                       normalized_value(*attribute_2);
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
