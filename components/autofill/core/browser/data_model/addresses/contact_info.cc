// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/contact_info.h"

#include <stddef.h>

#include <memory>
#include <ostream>
#include <string>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_l10n_util.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {
namespace {

// Finalizes the structure of `component` and returns the result of the
// finalization. If the `component` could not be completed, it is possible
// that it contains an invalid structure (e.g. first name
// is not matching the full name). In this case, the function wipes the invalid
// structure and tries to complete the structure again.
bool FinalizeNameAddressComponent(AddressComponent* component) {
  CHECK(component->GetStorageType() == NAME_FULL ||
        component->GetStorageType() == ALTERNATIVE_FULL_NAME);
  // Alternative names are not migrated because they were only recently
  // introduced.
  if (component->GetStorageType() == NAME_FULL) {
    component->MigrateLegacyStructure();
  }

  bool result = component->CompleteFullTree();
  if (!result) {
    if (component->GetVerificationStatus() ==
            VerificationStatus::kUserVerified &&
        component->WipeInvalidStructure()) {
      result = component->CompleteFullTree();
    }
  }
  return result;
}

}  // namespace

NameInfo::NameInfo()
    : name_(std::make_unique<NameFull>()),
      alternative_name_(std::make_unique<AlternativeFullName>()) {}

NameInfo::NameInfo(const NameInfo& info) : NameInfo() {
  *this = info;
}

NameInfo::NameInfo(std::unique_ptr<NameFull> name,
                   std::unique_ptr<AlternativeFullName> alternative_name)
    : name_(std::move(name)), alternative_name_(std::move(alternative_name)) {}

NameInfo& NameInfo::operator=(const NameInfo& info) {
  if (this == &info)
    return *this;

  name_->CopyFrom(*info.name_);
  alternative_name_->CopyFrom(*info.alternative_name_);

  return *this;
}

NameInfo::~NameInfo() = default;

bool NameInfo::MergeStructuredName(const NameInfo& newer) {
  if (name_->MergeWithComponent(newer.GetStructuredName())) {
    if (base::FeatureList::IsEnabled(
            features::kAutofillSupportPhoneticNameForJP)) {
      return alternative_name_->MergeWithComponent(
          newer.GetStructuredAlternativeName());
    }
    return true;
  }
  return false;
}

void NameInfo::MergeStructuredNameValidationStatuses(const NameInfo& newer) {
  name_->MergeVerificationStatuses(newer.GetStructuredName());
  if (base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    alternative_name_->MergeVerificationStatuses(
        newer.GetStructuredAlternativeName());
  }
}

bool NameInfo::IsStructuredNameMergeable(const NameInfo& newer) const {
  if (base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    return name_->IsMergeableWithComponent(newer.GetStructuredName()) &&
           alternative_name_->IsMergeableWithComponent(
               newer.GetStructuredAlternativeName());
  }
  return name_->IsMergeableWithComponent(newer.GetStructuredName());
}

bool NameInfo::FinalizeAfterImport() {
  bool result = FinalizeNameAddressComponent(name_.get());

  if (base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    result &= FinalizeNameAddressComponent(alternative_name_.get());
  }
  return result;
}

bool NameInfo::operator==(const NameInfo& other) const {
  if (this == &other)
    return true;
  if (base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    return name_->SameAs(*other.name_) &&
           alternative_name_->SameAs(*other.alternative_name_);
  }
  return name_->SameAs(*other.name_);
}

std::u16string NameInfo::GetRawInfo(FieldType type) const {
  DCHECK_EQ(FieldTypeGroup::kName, GroupTypeOfFieldType(type));
  return GetRootForType(type)->GetValueForType(type);
}

void NameInfo::SetRawInfoWithVerificationStatus(FieldType type,
                                                const std::u16string& value,
                                                VerificationStatus status) {
  DCHECK_EQ(FieldTypeGroup::kName, GroupTypeOfFieldType(type));
  GetRootForType(type)->SetValueForType(type, value, status);
}

FieldTypeSet NameInfo::GetSupportedTypes() const {
  FieldTypeSet supported_types = name_->GetSupportedTypes();
  if (base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    supported_types.insert_all(alternative_name_->GetSupportedTypes());
  }
  return supported_types;
}

std::u16string NameInfo::GetInfo(const AutofillType& type,
                                 const std::string& app_locale) const {
  return GetRawInfo(type.GetStorableType());
}

bool NameInfo::SetInfoWithVerificationStatus(const AutofillType& type,
                                             const std::u16string& value,
                                             const std::string& app_locale,
                                             VerificationStatus status) {
  if (type.GetStorableType() == NAME_FULL ||
      (type.GetStorableType() == ALTERNATIVE_FULL_NAME &&
       base::FeatureList::IsEnabled(
           features::kAutofillSupportPhoneticNameForJP))) {
    // If the set string is token equivalent to the old one, the value can
    // just be updated, otherwise create a new name record and complete it in
    // the end.
    // TODO(crbug.com/40266145): Move this logic to the data model.
    AreStringTokenEquivalent(value,
                             GetRootForType(type.GetStorableType())
                                 ->GetValueForType(type.GetStorableType()))
        ? GetRootForType(type.GetStorableType())
              ->SetValueForType(type.GetStorableType(), value, status)
        : GetRootForType(type.GetStorableType())
              ->SetValueForType(type.GetStorableType(), value, status,
                                /*invalidate_child_nodes=*/true);
    return true;
  }
  SetRawInfoWithVerificationStatus(type.GetStorableType(), value, status);
  return true;
}

VerificationStatus NameInfo::GetVerificationStatus(FieldType type) const {
  return GetRootForType(type)->GetVerificationStatusForType(type);
}

AddressComponent* NameInfo::GetRootForType(FieldType field_type) {
  return const_cast<AddressComponent*>(
      const_cast<const NameInfo*>(this)->GetRootForType(field_type));
}

const AddressComponent* NameInfo::GetRootForType(FieldType field_type) const {
  DCHECK_EQ(FieldTypeGroup::kName, GroupTypeOfFieldType(field_type));
  if (IsAlternativeNameType(field_type)) {
    return alternative_name_.get();
  }
  return name_.get();
}

EmailInfo::EmailInfo() = default;

EmailInfo::EmailInfo(const EmailInfo& info) = default;

EmailInfo& EmailInfo::operator=(const EmailInfo& info) = default;

EmailInfo::~EmailInfo() = default;

bool EmailInfo::operator==(const EmailInfo& other) const {
  return this == &other || email_ == other.email_;
}

FieldTypeSet EmailInfo::GetSupportedTypes() const {
  static constexpr FieldTypeSet supported_types{EMAIL_ADDRESS};
  return supported_types;
}

std::u16string EmailInfo::GetInfo(const AutofillType& type,
                                  const std::string& app_locale) const {
  return GetRawInfo(type.GetStorableType());
}

std::u16string EmailInfo::GetRawInfo(FieldType type) const {
  if (type == EMAIL_ADDRESS || type == EMAIL_OR_LOYALTY_MEMBERSHIP_ID) {
    return email_;
  }

  return std::u16string();
}

void EmailInfo::SetRawInfoWithVerificationStatus(FieldType type,
                                                 const std::u16string& value,
                                                 VerificationStatus status) {
  DCHECK_EQ(EMAIL_ADDRESS, type);
  email_ = value;
}

bool EmailInfo::SetInfoWithVerificationStatus(const AutofillType& type,
                                              const std::u16string& value,
                                              const std::string& app_locale,
                                              const VerificationStatus status) {
  SetRawInfoWithVerificationStatus(type.GetStorableType(), value, status);
  return true;
}

VerificationStatus EmailInfo::GetVerificationStatus(FieldType type) const {
  return VerificationStatus::kNoStatus;
}

CompanyInfo::CompanyInfo() = default;

CompanyInfo::CompanyInfo(const CompanyInfo& info) = default;

CompanyInfo::~CompanyInfo() = default;

bool CompanyInfo::operator==(const CompanyInfo& other) const {
  return this == &other ||
         GetRawInfo(COMPANY_NAME) == other.GetRawInfo(COMPANY_NAME);
}

FieldTypeSet CompanyInfo::GetSupportedTypes() const {
  static constexpr FieldTypeSet supported_types{COMPANY_NAME};
  return supported_types;
}

void CompanyInfo::GetMatchingTypes(const std::u16string& text,
                                   const std::string& app_locale,
                                   FieldTypeSet* matching_types) const {
  if (IsValid()) {
    FormGroup::GetMatchingTypes(text, app_locale, matching_types);
  } else if (text.empty()) {
    matching_types->insert(EMPTY_TYPE);
  }
}

std::u16string CompanyInfo::GetInfo(const AutofillType& type,
                                    const std::string& app_locale) const {
  return GetRawInfo(type.GetStorableType());
}

std::u16string CompanyInfo::GetRawInfo(FieldType type) const {
  return company_name_;
}

void CompanyInfo::SetRawInfoWithVerificationStatus(FieldType type,
                                                   const std::u16string& value,
                                                   VerificationStatus status) {
  DCHECK_EQ(COMPANY_NAME, type);
  company_name_ = value;
}

bool CompanyInfo::SetInfoWithVerificationStatus(
    const AutofillType& type,
    const std::u16string& value,
    const std::string& app_locale,
    const VerificationStatus status) {
  SetRawInfoWithVerificationStatus(type.GetStorableType(), value, status);
  return true;
}

VerificationStatus CompanyInfo::GetVerificationStatus(FieldType type) const {
  return VerificationStatus::kNoStatus;
}

bool CompanyInfo::IsValid() const {
  static constexpr char16_t kBirthyearRe[] = u"^(19|20)\\d{2}$";
  static constexpr char16_t kSocialTitleRe[] =
      u"^(Ms\\.?|Mrs\\.?|Mr\\.?|Miss|Mistress|Mister|"
      u"Frau|Herr|"
      u"Mlle|Mme|M\\.|"
      u"Dr\\.?|Prof\\.?)$";
  return !MatchesRegex<kBirthyearRe>(company_name_) &&
         !MatchesRegex<kSocialTitleRe>(company_name_);
}

}  // namespace autofill
