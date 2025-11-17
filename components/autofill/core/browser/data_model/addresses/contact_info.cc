// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/contact_info.h"

#include <stddef.h>

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/i18n/char_iterator.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_utils.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
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
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {
namespace {

constexpr std::u16string_view kSpace = u" ";

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

std::u16string GetNameForComparison(
    const NameInfo& name_info,
    const AddressCountryCode& common_country_code,
    const FieldType name_type) {
  switch (name_type) {
    case ALTERNATIVE_FULL_NAME:
      return name_info.GetValueForComparisonForType(name_type,
                                                    common_country_code);
    case NAME_FULL:
      // Using GetValue() directly to prevent normalization that would remove
      // diacritics. Normalization happens in
      // `AutofillProfileComparator::Compare()`.
      return name_info.GetRawInfo(name_type);
    default:
      NOTREACHED();
  }
}

// Generate the set of full/initial variants for `name_part`, where
// `name_part` is the user's first or middle name. For example, given "jean
// francois" (the normalized for comparison form of "Jean-François") this
// function returns the set:
//
//   { "", "f", "francois,
//     "j", "j f", "j francois",
//     "jean", "jean f", "jean francois", "jf" }
//
// Note: Expects that `name` is already normalized for comparison.
std::set<std::u16string> GetNamePartVariants(std::u16string_view name_part) {
  static constexpr size_t kMaxSupportedSubNames = 8;

  std::vector<std::u16string_view> sub_names = base::SplitStringPiece(
      name_part, kSpace, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Limit the number of sub-names we support (to constrain memory usage);
  if (sub_names.size() > kMaxSupportedSubNames) {
    return {std::u16string(name_part)};
  }

  // Start with the empty string as a variant.
  std::set<std::u16string> variants = {{}};

  // For each sub-name, add a variant of all the already existing variants that
  // appends this sub-name and one that appends the initial of this sub-name.
  // Duplicates will be discarded when they're added to the variants set.
  for (const auto& sub_name : sub_names) {
    if (sub_name.empty()) {
      continue;
    }
    std::vector<std::u16string> new_variants;
    for (const std::u16string& variant : variants) {
      new_variants.push_back(base::CollapseWhitespace(
          base::JoinString({variant, sub_name}, kSpace), true));
      new_variants.push_back(base::CollapseWhitespace(
          base::JoinString({variant, sub_name.substr(0, 1)}, kSpace), true));
    }
    variants.insert(new_variants.begin(), new_variants.end());
  }

  // As a common case, also add the variant that just concatenates all of the
  // initials.
  std::u16string initials;
  for (const auto& sub_name : sub_names) {
    if (sub_name.empty()) {
      continue;
    }
    initials.push_back(sub_name[0]);
  }
  variants.insert(initials);

  // And, we're done.
  return variants;
}

// Returns true if `full_name_2` is a variant of `full_name_1`.
//
// This function generates all variations of `full_name_1` and returns true if
// one of these variants is equal to `full_name_2`. For example, this function
// will return true if `full_name_2` is "john q public" and `full_name_1` is
// "john quincy public" because `full_name_2` can be derived from
// `full_name_1` by using the middle initial. Note that the reverse is not
// true, "john quincy public" is not a name variant of "john q public".
//
// Note: Expects that `full_name` is already normalized for comparison.
bool IsNormalizedNameVariantOf(std::u16string_view full_name_1,
                               std::u16string_view full_name_2) {
  data_util::NameParts name_1_parts = data_util::SplitName(full_name_1);

  // Build the variants of full_name_1`s given, middle and family names.
  //
  // TODO(rogerm): Figure out whether or not we should break apart a compound
  // family name into variants (crbug.com/619051)
  const std::set<std::u16string> given_name_variants =
      GetNamePartVariants(name_1_parts.given);
  const std::set<std::u16string> middle_name_variants =
      GetNamePartVariants(name_1_parts.middle);
  const std::set<std::u16string> family_name_variants = {name_1_parts.family,
                                                         u""};

  // Iterate over all full name variants of profile 1 and see if any of them
  // match the full name from profile 2.
  for (const std::u16string& given_name : given_name_variants) {
    for (const std::u16string& middle_name : middle_name_variants) {
      for (const std::u16string& family_name : family_name_variants) {
        std::u16string candidate = base::CollapseWhitespace(
            base::JoinString({given_name, middle_name, family_name}, kSpace),
            true);
        if (candidate == full_name_2) {
          return true;
        }
      }
    }
  }

  // Also check if the name is just composed of the user's initials. For
  // example, "thomas jefferson miller" could be composed as "tj miller".
  if (!name_1_parts.given.empty() && !name_1_parts.middle.empty()) {
    std::u16string initials;
    initials.push_back(name_1_parts.given[0]);
    initials.push_back(name_1_parts.middle[0]);
    std::u16string candidate = base::CollapseWhitespace(
        base::JoinString({initials, name_1_parts.family}, kSpace), true);
    if (candidate == full_name_2) {
      return true;
    }
  }

  // There was no match found.
  return false;
}

// TODO(crbug.com/359768803): Make this a private method of `NameInfo` and
// remove the country code arguments.
bool AreNameComponentsMergeable(const NameInfo& name_1,
                                const AddressCountryCode country_code_1,
                                const NameInfo& name_2,
                                const AddressCountryCode country_code_2,
                                const FieldType name_type) {
  DCHECK(name_type == NAME_FULL || name_type == ALTERNATIVE_FULL_NAME);
  const AddressCountryCode common_country_code =
      AddressComponent::GetCommonCountry(country_code_1, country_code_2);
  const std::u16string comparison_name_1 =
      GetNameForComparison(name_1, common_country_code, name_type);
  const std::u16string comparison_name_2 =
      GetNameForComparison(name_2, common_country_code, name_type);

  if (normalization::HasOnlySkippableCharacters(comparison_name_1) ||
      normalization::HasOnlySkippableCharacters(comparison_name_2) ||
      AutofillProfileComparator::Compare(
          comparison_name_1, comparison_name_2,
          normalization::WhitespaceSpec::kDiscard, name_type, country_code_1,
          country_code_2)) {
    return true;
  }

  // If the two names are just a permutation of each other, they are mergeable
  // for structured names.
  if (AreStringTokenEquivalent(comparison_name_1, comparison_name_2)) {
    return true;
  }

  std::u16string canon_full_name_1 = NormalizeForComparison(
      comparison_name_1, normalization::WhitespaceSpec::kRetain,
      country_code_1);
  std::u16string canon_full_name_2 = NormalizeForComparison(
      comparison_name_2, normalization::WhitespaceSpec::kRetain,
      country_code_2);

  // Is it reasonable to merge the names from `p1` and `p2`?
  bool result =
      IsNormalizedNameVariantOf(canon_full_name_1, canon_full_name_2) ||
      IsNormalizedNameVariantOf(canon_full_name_2, canon_full_name_1);
  return result;
}

// TODO(crbug.com/359768803): Make this a private method of `NameInfo` and
// remove the country code arguments.
void MergeNameComponents(const NameInfo& new_name_info,
                         const AddressCountryCode new_country_code,
                         const NameInfo& old_name_info,
                         const AddressCountryCode old_country_code,
                         const FieldType name_type,
                         AddressComponent& name_component) {
  DCHECK(name_type == NAME_FULL || name_type == ALTERNATIVE_FULL_NAME);

  const AddressCountryCode common_country_code =
      AddressComponent::GetCommonCountry(new_country_code, old_country_code);
  const std::u16string name_new = NormalizeForComparison(
      GetNameForComparison(new_name_info, common_country_code, name_type),
      normalization::WhitespaceSpec::kRetain, new_country_code);
  const std::u16string name_old = NormalizeForComparison(
      GetNameForComparison(old_name_info, common_country_code, name_type),
      normalization::WhitespaceSpec::kRetain, old_country_code);

  // At this state it is already determined that the two names are mergeable.
  // This can mean of of the following things:
  // * One name is empty. In this scenario the non-empty name is used.
  // * The names are token equivalent: In this scenario a merge of the tree
  // structure should be possible.
  // * One name is a variant of the other. In this scenario, use the non-variant
  // name.
  // First, set info to the original profile.
  name_component.CopyFrom(*old_name_info.GetRootForType(name_type));
  // If the name of the `new_profile` is empty, just keep the state of
  // `old_profile`.
  if (normalization::HasOnlySkippableCharacters(name_new)) {
    return;
  }
  // Vice versa set name to the one of `new_profile` if `old_profile` has an
  // empty name
  if (normalization::HasOnlySkippableCharacters(name_old)) {
    name_component.CopyFrom(*new_name_info.GetRootForType(name_type));
    return;
  }
  // Try to apply a direct merging.
  if (name_component.MergeWithComponent(
          *new_name_info.GetRootForType(name_type))) {
    return;
  }
  // If the name in `old_profile` is a variant of `new_profile` use the one in
  // `new_profile`.
  if (IsNormalizedNameVariantOf(name_new, name_old)) {
    name_component.CopyFrom(*new_name_info.GetRootForType(name_type));
  } else {
    name_component.CopyFrom(*old_name_info.GetRootForType(name_type));
  }
}

}  // namespace

NameInfo::NameInfo(bool alternative_names_supported)
    : name_(std::make_unique<NameFull>()) {
  if (alternative_names_supported) {
    alternative_name_ = std::make_unique<AlternativeFullName>();
  }
}

NameInfo::NameInfo(const NameInfo& info)
    : NameInfo(info.IsAlternativeNameSupported()) {
  *this = info;
}

NameInfo::NameInfo(std::unique_ptr<NameFull> name,
                   std::unique_ptr<AlternativeFullName> alternative_name)
    : name_(std::move(name)), alternative_name_(std::move(alternative_name)) {}

NameInfo& NameInfo::operator=(const NameInfo& info) {
  if (this == &info)
    return *this;

  name_->CopyFrom(*info.name_);
  if (info.IsAlternativeNameSupported()) {
    alternative_name_ = std::make_unique<AlternativeFullName>();
    alternative_name_->CopyFrom(*info.alternative_name_);
  } else if (alternative_name_) {
    alternative_name_.reset();
  }

  return *this;
}

NameInfo::~NameInfo() = default;

// static
bool NameInfo::MergeNames(const NameInfo& new_name_info,
                          const AddressCountryCode new_country_code,
                          const NameInfo& old_name_info,
                          const AddressCountryCode old_country_code,
                          NameInfo& result_name_info) {
  DCHECK(AreNamesMergeable(new_name_info, new_country_code, old_name_info,
                           old_country_code));
  DCHECK(AreAlternativeNamesMergeable(new_name_info, new_country_code,
                                      old_name_info, old_country_code));

  std::unique_ptr<NameFull> name_full = std::make_unique<NameFull>();
  // If `new_name_info` does not support alternative names, the
  // `alternative_full_name` will be `nullptr`.
  std::unique_ptr<AlternativeFullName> alternative_full_name;

  // TODO(crbug.com/375383124): Update `MergeNames` to provide meaningful
  // return values.
  MergeNameComponents(new_name_info, new_country_code, old_name_info,
                      old_country_code, NAME_FULL, *name_full);
  if (new_name_info.IsAlternativeNameSupported() &&
      base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    alternative_full_name = std::make_unique<AlternativeFullName>();
    MergeNameComponents(new_name_info, new_country_code, old_name_info,
                        old_country_code, ALTERNATIVE_FULL_NAME,
                        *alternative_full_name);
  }
  result_name_info =
      NameInfo(std::move(name_full), std::move(alternative_full_name));
  return true;
}

// static
bool NameInfo::AreNamesMergeable(const NameInfo& name_info_1,
                                 const AddressCountryCode country_code_1,
                                 const NameInfo& name_info_2,
                                 const AddressCountryCode country_code_2) {
  return AreNameComponentsMergeable(name_info_1, country_code_1, name_info_2,
                                    country_code_2, NAME_FULL);
}

// static
bool NameInfo::AreAlternativeNamesMergeable(
    const NameInfo& name_info_1,
    const AddressCountryCode country_code_1,
    const NameInfo& name_info_2,
    const AddressCountryCode country_code_2) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    return true;
  }

  if (!name_info_1.IsAlternativeNameSupported() &&
      !name_info_2.IsAlternativeNameSupported()) {
    return true;
  }
  if (name_info_1.IsAlternativeNameSupported() ^
      name_info_2.IsAlternativeNameSupported()) {
    return false;
  }

  return AreNameComponentsMergeable(name_info_1, country_code_1, name_info_2,
                                    country_code_2, ALTERNATIVE_FULL_NAME);
}

bool NameInfo::MergeStructuredName(const NameInfo& newer) {
  if (name_->MergeWithComponent(*newer.name_)) {
    if (IsAlternativeNameSupported() && newer.IsAlternativeNameSupported() &&
        base::FeatureList::IsEnabled(
            features::kAutofillSupportPhoneticNameForJP)) {
      return alternative_name_->MergeWithComponent(*newer.alternative_name_);
    }
    return true;
  }
  return false;
}

void NameInfo::MergeStructuredNameValidationStatuses(const NameInfo& newer) {
  name_->MergeVerificationStatuses(*newer.name_);
  if (IsAlternativeNameSupported() && newer.IsAlternativeNameSupported() &&
      base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    alternative_name_->MergeVerificationStatuses(*newer.alternative_name_);
  }
}

bool NameInfo::IsNameVariantOf(std::u16string_view value,
                               std::string_view app_locale) const {
  return IsNormalizedNameVariantOf(
      normalization::NormalizeForComparison(GetInfo(NAME_FULL, app_locale)),
      normalization::NormalizeForComparison(value));
}

std::optional<FieldType> NameInfo::GetStorableTypeOf(FieldType type) const {
  return GetRootForType(type)->GetStorableTypeOf(type);
}

bool NameInfo::HasNameEligibleForPhoneticNameMigration() const {
  if (!IsAlternativeNameSupported()) {
    return false;
  }
  // A full name is eligible to be migrated into a phonetic name if it contains
  // only Katakana or Hiragana characters (Japanese phonetic symbols) and
  // whitespaces or separators, but no Kanji (regular Japanese characters),
  // Latin characters, etc.
  bool phonetic_characters_found = false;
  UErrorCode error = U_ZERO_ERROR;

  std::string full_name = base::UTF16ToUTF8(name_->GetValue());
  re2::RE2::GlobalReplace(&full_name, autofill::kCjkNameSeparatorsRe, "");
  const std::u16string processed_full_name = base::UTF8ToUTF16(full_name);

  for (base::i18n::UTF16CharIterator iter(processed_full_name); !iter.end();
       iter.Advance()) {
    UScriptCode character = uscript_getScript(iter.get(), &error);
    if (U_FAILURE(error)) {
      DLOG(ERROR) << "uscript_getScript failed, error code: "
                  << u_errorName(error);
      return false;
    }

    // Whitespaces, dashes and hyphens (e.g. katakana dot) are separators that
    // can be ignored.
    if (u_isUWhiteSpace(character) ||
        u_hasBinaryProperty(character, UCHAR_DASH) ||
        u_hasBinaryProperty(character, UCHAR_HYPHEN)) {
      continue;
    }

    if (character == USCRIPT_KATAKANA_OR_HIRAGANA ||
        character == USCRIPT_KATAKANA || character == USCRIPT_HIRAGANA) {
      phonetic_characters_found = true;
      continue;
    }

    // If the character is none of the above (e.g. Kanji, Latin alphabet etc.),
    // the string does not meet either condition.
    return false;
  }

  return phonetic_characters_found;
}

void NameInfo::MigrateRegularNameToPhoneticName() {
  DCHECK(HasNameEligibleForPhoneticNameMigration());
  alternative_name_->SetValueForType(ALTERNATIVE_FULL_NAME,
                                     name_->GetValueForType(NAME_FULL),
                                     VerificationStatus::kNoStatus);
  alternative_name_->SetValueForType(ALTERNATIVE_FAMILY_NAME,
                                     name_->GetValueForType(NAME_LAST),
                                     VerificationStatus::kNoStatus);
  alternative_name_->SetValueForType(ALTERNATIVE_GIVEN_NAME,
                                     name_->GetValueForType(NAME_FIRST),
                                     VerificationStatus::kNoStatus);

  name_->UnsetAddressComponentAndItsSubcomponents();
}

std::u16string NameInfo::GetValueForComparisonForType(
    FieldType field_type,
    const AddressCountryCode& common_country_code) const {
  return GetRootForType(field_type)
      ->GetValueForComparisonForType(field_type, common_country_code);
}

bool NameInfo::IsStructuredNameMergeable(const NameInfo& newer) const {
  // It should never happen in practice as this method is used to override the
  // `AutofillProile` owning `this` with data coming from sync. Since their
  // guids have to match, the country of both profiles (and thus the support for
  // alternative names), should be the same.
  if (!HaveSimilarAlternativeNameSupport(newer)) {
    return false;
  }

  if (IsAlternativeNameSupported() && newer.IsAlternativeNameSupported() &&
      base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    return name_->IsMergeableWithComponent(*newer.name_) &&
           alternative_name_->IsMergeableWithComponent(
               *newer.alternative_name_);
  }
  return name_->IsMergeableWithComponent(*newer.name_);
}

bool NameInfo::FinalizeAfterImport() {
  bool result = FinalizeNameAddressComponent(name_.get());
  if (IsAlternativeNameSupported() &&
      base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    result &= FinalizeNameAddressComponent(alternative_name_.get());
  }
  return result;
}

bool NameInfo::operator==(const NameInfo& other) const {
  if (this == &other)
    return true;

  // If only one of the profiles supports the alternative name, the two
  // `NameInfo`s are different.
  if (!HaveSimilarAlternativeNameSupport(other) &&
      base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    return false;
  }

  if (IsAlternativeNameSupported() &&
      base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    return name_->SameAs(*other.name_) &&
           alternative_name_->SameAs(*other.alternative_name_);
  }
  return name_->SameAs(*other.name_);
}

std::u16string NameInfo::GetRawInfo(FieldType type) const {
  DCHECK_EQ(FieldTypeGroup::kName, GroupTypeOfFieldType(type));
  if (IsAlternativeNameType(type) && !IsAlternativeNameSupported()) {
    return std::u16string();
  }
  return GetRootForType(type)->GetValueForType(type);
}

void NameInfo::SetRawInfoWithVerificationStatus(FieldType type,
                                                std::u16string_view value,
                                                VerificationStatus status) {
  DCHECK_EQ(FieldTypeGroup::kName, GroupTypeOfFieldType(type));
  if (IsAlternativeNameType(type) && !IsAlternativeNameSupported()) {
    return;
  }
  GetRootForType(type)->SetValueForType(type, value, status);
}

FieldTypeSet NameInfo::GetSupportedTypes() const {
  FieldTypeSet supported_types = name_->GetSupportedTypes();
  if (IsAlternativeNameSupported() &&
      base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    supported_types.insert_all(alternative_name_->GetSupportedTypes());
  }
  return supported_types;
}

std::u16string NameInfo::GetInfo(const AutofillType& type,
                                 std::string_view app_locale) const {
  return GetRawInfo(type.GetAddressType());
}

bool NameInfo::SetInfoWithVerificationStatus(const AutofillType& type,
                                             std::u16string_view value,
                                             std::string_view app_locale,
                                             VerificationStatus status) {
  const FieldType ft = type.GetAddressType();
  if (ft == NAME_FULL ||
      (ft == ALTERNATIVE_FULL_NAME && IsAlternativeNameSupported() &&
       base::FeatureList::IsEnabled(
           features::kAutofillSupportPhoneticNameForJP))) {
    // If the set string is token equivalent to the old one, the value can
    // just be updated, otherwise create a new name record and complete it in
    // the end.
    // TODO(crbug.com/40266145): Move this logic to the data model.
    AreStringTokenEquivalent(value, GetRootForType(ft)->GetValueForType(ft))
        ? GetRootForType(ft)->SetValueForType(ft, value, status)
        : GetRootForType(ft)->SetValueForType(ft, value, status,
                                              /*invalidate_child_nodes=*/true);
    return true;
  }
  SetRawInfoWithVerificationStatus(ft, value, status);
  return true;
}

VerificationStatus NameInfo::GetVerificationStatus(FieldType type) const {
  if (IsAlternativeNameType(type) && !IsAlternativeNameSupported()) {
    return VerificationStatus::kNoStatus;
  }
  return GetRootForType(type)->GetVerificationStatusForType(type);
}

AddressComponent* NameInfo::GetRootForType(FieldType field_type) {
  return const_cast<AddressComponent*>(
      const_cast<const NameInfo*>(this)->GetRootForType(field_type));
}

void NameInfo::OnCountryChange(const AddressCountryCode& new_country_code) {
  if (new_country_code == AddressCountryCode("JP")) {
    CreateAlternativeNameTree();
  } else {
    DeleteAlternativeNameTree();
  }
}

const AddressComponent* NameInfo::GetRootForType(FieldType field_type) const {
  CHECK_EQ(FieldTypeGroup::kName, GroupTypeOfFieldType(field_type));
  if (IsAlternativeNameType(field_type)) {
    return IsAlternativeNameSupported() ? alternative_name_.get() : nullptr;
  }
  return name_.get();
}

void NameInfo::CreateAlternativeNameTree() {
  if (alternative_name_) {
    return;
  }
  if (!base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    return;
  }

  alternative_name_ = std::make_unique<AlternativeFullName>();
}

void NameInfo::DeleteAlternativeNameTree() {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillSupportPhoneticNameForJP)) {
    return;
  }
  alternative_name_.reset();
}

bool NameInfo::IsAlternativeNameSupported() const {
  return alternative_name_ != nullptr;
}

bool NameInfo::HaveSimilarAlternativeNameSupport(const NameInfo& other) const {
  return IsAlternativeNameSupported() == other.IsAlternativeNameSupported();
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
                                  std::string_view app_locale) const {
  return GetRawInfo(type.GetAddressType());
}

std::u16string EmailInfo::GetRawInfo(FieldType type) const {
  if (type == EMAIL_ADDRESS || type == EMAIL_OR_LOYALTY_MEMBERSHIP_ID) {
    return email_;
  }

  return std::u16string();
}

void EmailInfo::SetRawInfoWithVerificationStatus(FieldType type,
                                                 std::u16string_view value,
                                                 VerificationStatus status) {
  CHECK(type == EMAIL_ADDRESS || type == EMAIL_OR_LOYALTY_MEMBERSHIP_ID,
        base::NotFatalUntil::M145);
  email_ = value;
}

bool EmailInfo::SetInfoWithVerificationStatus(const AutofillType& type,
                                              std::u16string_view value,
                                              std::string_view app_locale,
                                              const VerificationStatus status) {
  SetRawInfoWithVerificationStatus(type.GetAddressType(), value, status);
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

void CompanyInfo::GetMatchingTypes(std::u16string_view text,
                                   std::string_view app_locale,
                                   FieldTypeSet* matching_types) const {
  if (IsValid()) {
    FormGroup::GetMatchingTypes(text, app_locale, matching_types);
  } else if (text.empty()) {
    matching_types->insert(EMPTY_TYPE);
  }
}

std::u16string CompanyInfo::GetInfo(const AutofillType& type,
                                    std::string_view app_locale) const {
  return GetRawInfo(type.GetAddressType());
}

std::u16string CompanyInfo::GetRawInfo(FieldType type) const {
  return company_name_;
}

void CompanyInfo::SetRawInfoWithVerificationStatus(FieldType type,
                                                   std::u16string_view value,
                                                   VerificationStatus status) {
  DCHECK_EQ(COMPANY_NAME, type);
  company_name_ = value;
}

bool CompanyInfo::SetInfoWithVerificationStatus(
    const AutofillType& type,
    std::u16string_view value,
    std::string_view app_locale,
    const VerificationStatus status) {
  SetRawInfoWithVerificationStatus(type.GetAddressType(), value, status);
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
