// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_type.h"

#include <string_view>
#include <variant>
#include <vector>

#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/common/autofill_features.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

namespace {

// Returns the single element in `s` if it exists or UNKNOWN_TYPE.
// `s` must not contain more than one element.
[[nodiscard]] constexpr FieldType GetUniqueIfAny(const FieldTypeSet& s) {
  DCHECK_LE(s.size(), 1u) << FieldTypeSetToString(s);
  return !s.empty() ? *s.begin() : UNKNOWN_TYPE;
}

// Used by the constructor to handle NO_SERVER_DATA and UNKNOWN_TYPE:
// - NO_SERVER_DATA is represented as the empty set of FieldTypes.
// - UNKNOWN_TYPE trumps all other FieldTypes.
FieldTypeSet Normalize(FieldTypeSet field_types) {
  field_types.erase(NO_SERVER_DATA);
  if (field_types.contains(UNKNOWN_TYPE)) {
    field_types.clear();
    field_types.insert(UNKNOWN_TYPE);
  }
  return field_types;
}

// The sets below define the AutofillType constraints. Every AutofillType must
// contain at most one of these FieldTypes. This is so that we define getters
// like GetAddressType() which return a unique FieldType. See TestConstraints().
//
// These FieldTypes are not identical to other groupings of FieldTypes:
// - FieldTypeGroups are too granular (e.g., multiple FieldTypeGroups make up
//   the address-related FieldTypes) and overlap (e.g., FieldTypeGroup::kName
//   types count both as address and as Autofill AI FieldTypes).
// - FormTypes are too granular (FormType::kCreditCardForm and
//   FormType::kStandaloneCvcForm both count as CVC fields) and too hierarchical
//   (e.g., they count names exclusively towards addresses) and incomplete
//   (e.g., there is no FormType::kAutofillAi).
// - FillingProducts are too decoupled from FieldTypes (e.g., some
//   FillingProducts have no associated FieldTypes).

constexpr FieldTypeSet kAddressFieldTypes =
    Union(FieldTypesOfGroup(FieldTypeGroup::kName),
          FieldTypesOfGroup(FieldTypeGroup::kEmail),
          FieldTypesOfGroup(FieldTypeGroup::kCompany),
          FieldTypesOfGroup(FieldTypeGroup::kAddress),
          FieldTypesOfGroup(FieldTypeGroup::kPhone));

constexpr FieldTypeSet kCreditCardFieldTypes =
    Union(FieldTypesOfGroup(FieldTypeGroup::kCreditCard),
          FieldTypesOfGroup(FieldTypeGroup::kStandaloneCvcField));

// FedCM currently only supports full names (NAME_FULL) and given names
// (NAME_FIRST), no other name parts:
// https://w3c-fedid.github.io/FedCM/#dictdef-identityprovideraccount
//
// TODO(crbug.com/432645177): We probably need to remove this set and the
// `FieldType GetIdentityCredentialType()` getter if and when we start
// populating AutofillType with _all_ FieldTypes received from the server, which
// will be needed when we migrate the PWM to AutofillType. The reason is that
// the server does and should predict classical Autofill FieldTypes and PWM
// FieldTypes simultaneously (e.g., a field may receive the predictions
// NAME_FULL and PASSWORD), and this set disallows them. This will become even
// more acute if the set below grows further (e.g., to include USERNAME) and the
// overlap with PWM types grows (the Autofill server may predict
// EMAIL_ADDRESS and USERNAME for the same field).
constexpr FieldTypeSet kIdentityCredentialFieldTypes = {
    NAME_FIRST, NAME_FULL, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER, PASSWORD};

// TODO(crbug.com/432645177): Remove `EMAIL_OR_LOYALTY_MEMBERSHIP_ID` and
// represent it as union of the other three types. That means the getter
// `FieldType GetLoyaltyCardType()` must be replaced with
// `FieldTypeSet GetLoyaltyCardTypes()`.
constexpr FieldTypeSet kLoyaltyCardFieldTypes = {
    EMAIL_ADDRESS, LOYALTY_MEMBERSHIP_ID, LOYALTY_MEMBERSHIP_PROGRAM,
    LOYALTY_MEMBERSHIP_PROVIDER, EMAIL_OR_LOYALTY_MEMBERSHIP_ID};

// Password Manager currently does not use AutofillType except for filling
// ONE_TIME_CODE fields. If and when we want to migrate Password Manager to
// AutofillType, we need to be careful about the AutofillType constraints.
// For example, the constraint for Identity Credentials says that an
// AutofillType cannot contain NAME_FULL and PASSWORD simultaneously, but the
// server may well serve such predictions at the moment.
constexpr FieldTypeSet kPasswordManagerFieldTypes =
    Union(FieldTypesOfGroup(FieldTypeGroup::kUsernameField),
          FieldTypesOfGroup(FieldTypeGroup::kPasswordField),
          FieldTypeSet{ONE_TIME_CODE});

}  // namespace

// static
bool AutofillType::TestConstraints(const FieldTypeSet& s) {
  // Each EntityType defines one constraint, so we don't have a constant
  // FieldTypeSet for each of them.
  auto test_entity_constraint = [&s](EntityType entity) {
    FieldTypeSet t;
    for (AttributeType attribute : entity.attributes()) {
      if (base::FeatureList::IsEnabled(features::kAutofillAiNoTagTypes)) {
        t.insert_all(attribute.field_subtypes());
      } else {
        t.insert(attribute.field_type_with_tag_types());
      }
    }
    return Intersection(s, t).size() <= 1;
  };
  return Intersection(s, kAddressFieldTypes).size() <= 1 &&
         std::ranges::all_of(DenseSet<EntityType>::all(),
                             test_entity_constraint) &&
         Intersection(s, kCreditCardFieldTypes).size() <= 1 &&
         Intersection(s, kIdentityCredentialFieldTypes).size() <= 1 &&
         Intersection(s, kLoyaltyCardFieldTypes).size() <= 1 &&
         Intersection(s, kPasswordManagerFieldTypes).size() <= 1;
}

AutofillType::ServerPrediction::ServerPrediction() = default;

AutofillType::ServerPrediction::ServerPrediction(const AutofillField& field) {
  password_requirements = field.password_requirements();
  server_predictions = field.server_predictions();
}

AutofillType::ServerPrediction::ServerPrediction(const ServerPrediction&) =
    default;

AutofillType::ServerPrediction& AutofillType::ServerPrediction::operator=(
    const ServerPrediction&) = default;

AutofillType::ServerPrediction::ServerPrediction(ServerPrediction&&) = default;

AutofillType::ServerPrediction& AutofillType::ServerPrediction::operator=(
    ServerPrediction&&) = default;

AutofillType::ServerPrediction::~ServerPrediction() = default;

FieldType AutofillType::ServerPrediction::server_type() const {
  return server_predictions.empty()
             ? NO_SERVER_DATA
             : ToSafeFieldType(server_predictions[0].type(), NO_SERVER_DATA);
}

bool AutofillType::ServerPrediction::is_override() const {
  return server_predictions.empty() ? false : server_predictions[0].override();
}

AutofillType::AutofillType(FieldTypeSet field_types)
    : types_(Normalize(field_types)) {
  DCHECK(TestConstraints(field_types)) << FieldTypeSetToString(field_types);
  DCHECK(TestConstraints(GetTypes())) << FieldTypeSetToString(GetTypes());
}

AutofillType::AutofillType(FieldType field_type)
    : AutofillType(FieldTypeSet{field_type}) {}

AutofillType::AutofillType(HtmlFieldType field_type) : types_(field_type) {
  DCHECK(TestConstraints(GetTypes())) << FieldTypeSetToString(GetTypes());
}

HtmlFieldType AutofillType::html_type() const {
  const HtmlFieldType* html_type = std::get_if<HtmlFieldType>(&types_);
  return html_type ? *html_type : HtmlFieldType::kUnspecified;
}

FieldType AutofillType::GetStorableType() const {
  FieldTypeSet types = GetTypes();
  return !types.empty() ? *types.begin() : NO_SERVER_DATA;
}

FieldTypeGroup AutofillType::group() const {
  FieldTypeGroupSet groups = GetGroups();
  return !groups.empty() ? *groups.begin() : FieldTypeGroup::kNoGroup;
}

FieldTypeSet AutofillType::GetTypes() const {
  return std::visit(
      absl::Overload{
          [](FieldTypeSet field_types) { return field_types; },
          [](HtmlFieldType html_type) {
            return FieldTypeSet{
                HtmlFieldTypeToBestCorrespondingFieldType(html_type)};
          }},
      types_);
}

DenseSet<FieldTypeGroup> AutofillType::GetGroups() const {
  FieldTypeGroupSet groups = std::visit(
      absl::Overload{
          [](const FieldTypeSet& field_types) {
            return FieldTypeGroupSet(field_types, &GroupTypeOfFieldType);
          },
          [](HtmlFieldType html_type) {
            return FieldTypeGroupSet{GroupTypeOfHtmlFieldType(html_type)};
          }},
      types_);
  groups.erase(FieldTypeGroup::kNoGroup);
  return groups;
}

DenseSet<FormType> AutofillType::GetFormTypes() const {
  DenseSet<FormType> form_types =
      DenseSet<FormType>(GetGroups(), &FieldTypeGroupToFormType);
  form_types.erase(FormType::kUnknownFormType);
  return form_types;
}

FieldType AutofillType::GetAddressType() const {
  return GetUniqueIfAny(Intersection(GetTypes(), kAddressFieldTypes));
}

FieldType AutofillType::GetAutofillAiType(EntityType entity) const {
  FieldTypeSet field_types = {};
  if (base::FeatureList::IsEnabled(features::kAutofillAiNoTagTypes)) {
    for (AttributeType attribute : entity.attributes()) {
      field_types.insert_all(attribute.field_subtypes());
    }
  } else {
    for (AttributeType attribute : entity.attributes()) {
      field_types.insert(attribute.field_type_with_tag_types());
    }
  }
  return GetUniqueIfAny(Intersection(GetTypes(), field_types));
}

FieldType AutofillType::GetCreditCardType() const {
  return GetUniqueIfAny(Intersection(GetTypes(), kCreditCardFieldTypes));
}

FieldType AutofillType::GetIdentityCredentialType() const {
  return GetUniqueIfAny(
      Intersection(GetTypes(), kIdentityCredentialFieldTypes));
}

FieldType AutofillType::GetLoyaltyCardType() const {
  return GetUniqueIfAny(Intersection(GetTypes(), kLoyaltyCardFieldTypes));
}

FieldType AutofillType::GetPasswordManagerType() const {
  return GetUniqueIfAny(Intersection(GetTypes(), kPasswordManagerFieldTypes));
}

FieldTypeSet AutofillType::GetAutofillAiTypes() const {
  if (base::FeatureList::IsEnabled(features::kAutofillAiNoTagTypes)) {
    static FieldTypeSet kFieldTypesWithoutTagTypes = [] {
      FieldTypeSet field_types;
      for (EntityType entity : DenseSet<EntityType>::all()) {
        for (AttributeType attribute : entity.attributes()) {
          field_types.insert_all(attribute.field_subtypes());
        }
      }
      return field_types;
    }();
    return Intersection(GetTypes(), kFieldTypesWithoutTagTypes);
  } else {
    // Some entities (e.g. National Id Card) use NAME_FULL instead of a tag
    // type.
    static constexpr FieldTypeSet kFieldTypes =
        Union(FieldTypesOfGroup(FieldTypeGroup::kAutofillAi),
              FieldTypeSet{NAME_FULL});
    return Intersection(GetTypes(), kFieldTypes);
  }

  // TODO(crbug.com/422563282): Remove when cleaning up kAutofillAiNoTagTypes,
  // do the following:
  // - Exclude `*_TAG` types in ToSafeFieldType().
  // - Remove the above code of this function.
  // - Activate the below code of this function.

  // static constexpr FieldTypeSet kFieldTypes =
  //     Union(FieldTypesOfGroup(FieldTypeGroup::kName),
  //           FieldTypesOfGroup(FieldTypeGroup::kAutofillAi));
  // return Intersection(GetTypes(), kFieldTypes);
}

FieldTypeSet AutofillType::GetStaticAutofillAiTypes() const {
  static constexpr FieldTypeSet kFieldTypes =
      FieldTypesOfGroup(FieldTypeGroup::kAutofillAi);
  return Intersection(GetTypes(), kFieldTypes);
}

std::string AutofillType::ToString() const {
  return std::visit(
      absl::Overload{
          [](const FieldTypeSet& field_types) {
            return !field_types.empty()
                       ? FieldTypeSetToString(field_types)
                       : FieldTypeSetToString({NO_SERVER_DATA});
          },
          [](HtmlFieldType html_type) { return FieldTypeToString(html_type); }},
      types_);
}

}  // namespace autofill
