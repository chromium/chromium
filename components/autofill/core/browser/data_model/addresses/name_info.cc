// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/name_info.h"

#include <stddef.h>

#include <concepts>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_util.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_constants.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_util.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_type_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
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

// The name trees stored in a `NameInfo`.
template <typename T>
concept NameType =
    std::same_as<T, NameFull> || std::same_as<T, AlternativeFullName>;

// Returns the root type of the name tree `T`.
template <NameType T>
consteval FieldType GetFieldType() {
  if constexpr (std::same_as<T, NameFull>) {
    return NAME_FULL;
  } else {
    return ALTERNATIVE_FULL_NAME;
  }
}

template <NameType T>
std::u16string GetNameForComparison(
    const NameInfo& name_info,
    const AddressCountryCode& common_country_code) {
  if constexpr (std::same_as<T, AlternativeFullName>) {
    return name_info.GetValueForComparisonForType(GetFieldType<T>(),
                                                  common_country_code);
  } else {
    // Using GetValue() directly to prevent normalization that would remove
    // diacritics. Normalization happens in
    // `AutofillProfileComparator::Compare()`.
    return name_info.GetRawInfo(GetFieldType<T>());
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
// TODO(crbug.com/479905438) Remove once launched.
std::set<std::u16string> GetNamePartVariantsDeprecated(
    std::u16string_view name_part) {
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
    new_variants.reserve(variants.size() * 2);
    for (const std::u16string& variant : variants) {
      new_variants.push_back(base::CollapseWhitespace(
          base::JoinString({variant, sub_name}, kSpace), true));
      new_variants.push_back(base::CollapseWhitespace(
          base::JoinString({variant, sub_name.substr(0, 1)}, kSpace), true));
    }
    variants.insert_range(std::views::as_rvalue(new_variants));
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

// TODO(crbug.com/479905438) Remove once launched.
bool MatchesCjkVariant(std::u16string_view full_name,
                       const std::set<std::u16string>& given_name_variants,
                       const std::set<std::u16string>& family_name_variants) {
  // CJK names are formatted like this: Family Name + Given Name
  // Note: CJK names typically do not have middle names in this structure.
  for (const std::u16string& family : family_name_variants) {
    for (const std::u16string& given : given_name_variants) {
      if (base::CollapseWhitespace(base::StrCat({family, given}), true) ==
          full_name) {
        return true;
      }
      // Typically CJK names do not have separators, but this case should be
      // supported as well.
      if (base::CollapseWhitespace(base::JoinString({family, given}, kSpace),
                                   true) == full_name) {
        return true;
      }
    }
  }
  return false;
}

// An implementation of `IsNormalizedNameVariantOf` with exponential time
// complexity in the number of given and middle name tokens in `full_name_1`.
//
// `GetNamePartVariantsDeprecated` generates all possible subsequences where
// tokens are either fully included, abbreviated to a one-letter initial, or
// skipped. This results in 3^n subsequences, where n is the number of tokens.
// TODO(crbug.com/479905438) Remove once launched.
bool IsNormalizedNameVariantOfExponential(std::u16string_view full_name_1,
                                          std::u16string_view full_name_2) {
  // This early return is just an optimization, the rest of the logic should
  // handle this case as well.
  if (full_name_1 == full_name_2) {
    return true;
  }

  data_util::NameParts name_1_parts = data_util::SplitName(full_name_1);

  // Build the variants of full_name_1`s given, middle and family names.
  const std::set<std::u16string> given_name_variants =
      GetNamePartVariantsDeprecated(name_1_parts.given);
  const std::set<std::u16string> middle_name_variants =
      GetNamePartVariantsDeprecated(name_1_parts.middle);
  const std::set<std::u16string> family_name_variants = {name_1_parts.family,
                                                         u""};

  if (HasCjkNameCharacteristics(base::UTF16ToUTF8(full_name_1)) &&
      MatchesCjkVariant(full_name_2, given_name_variants,
                        family_name_variants)) {
    return true;
  }
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

// An implementation of `IsNormalizedNameVariantOf` with linear time complexity
// in the number of tokens in `full_name_1` and `full_name_2`.
bool IsNormalizedNameVariantOfLinear(std::u16string_view full_name_1,
                                     std::u16string_view full_name_2) {
  // These early returns are just optimizations, the rest of the logic should
  // handle these cases as well.
  if (full_name_1 == full_name_2 || full_name_2.empty()) {
    return true;
  }

  const bool is_cjk = HasCjkNameCharacteristics(base::UTF16ToUTF8(full_name_1));

  if (!is_cjk && full_name_2.size() > full_name_1.size()) {
    return false;
  }

  data_util::NameParts name_1_parts = data_util::SplitName(full_name_1);

  auto tokenize = [](std::u16string_view str) {
    return base::SplitStringPiece(str, kSpace, base::TRIM_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY);
  };

  // Family name abbreviations are not allowed; the family name must match
  // exactly or be completely omitted. If found, it is stripped first (from the
  // beginning for CJK, or from the end for non-CJK), leaving only given and
  // middle names (if applicable, middle name is not parsed for CJK) for the
  // subsequence check.
  if (is_cjk) {
    std::u16string_view given_name_2 =
        base::RemovePrefix(full_name_2, name_1_parts.family)
            .value_or(full_name_2);
    if (IsAbbreviatedConcatenatedSubsequence(tokenize(name_1_parts.given),
                                             tokenize(given_name_2))) {
      return true;
    }
  }

  std::vector<std::u16string_view> tokens_1 = tokenize(name_1_parts.given);
  std::vector<std::u16string_view> middle_tokens =
      tokenize(name_1_parts.middle);
  tokens_1.insert(tokens_1.end(), middle_tokens.begin(), middle_tokens.end());

  std::vector<std::u16string_view> tokens_2 = tokenize(full_name_2);
  std::vector<std::u16string_view> family_tokens =
      tokenize(name_1_parts.family);

  // Similarly to CJK, remove the family name from the end if found before the
  // abbreviated subsequence check.
  if (std::ranges::ends_with(tokens_2, family_tokens)) {
    tokens_2.resize(tokens_2.size() - family_tokens.size());
  }

  return IsAbbreviatedConcatenatedSubsequence(tokens_1, tokens_2);
}

// Returns true if `full_name_2` is a variant of `full_name_1`.
//
// Consider these names:
// full_name_1 = "john quincy public"
// full_name_2 = "john q public"
//
// In this case, full_name_2 is a variant of full_name_1 because full_name_2
// can be derived from full_name_1 by using the middle initial.
//
// At the same time, full_name_1 is not a variant of full_name_2 because
// we cannot be sure that "q" is an abbreviation of "quincy".
//
// Note: Expects that `full_name` is already normalized for comparison.
bool IsNormalizedNameVariantOf(std::u16string_view full_name_1,
                               std::u16string_view full_name_2) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.IsNormalizedNameVariantOf");
  if (base::FeatureList::IsEnabled(
          features::kAutofillOptimizeIsNormalizedNameVariantOf)) {
    return IsNormalizedNameVariantOfLinear(full_name_1, full_name_2);
  }
  return IsNormalizedNameVariantOfExponential(full_name_1, full_name_2);
}

template <NameType T>
bool AreNameComponentsMergeable(const NameInfo& name_1,
                                const AddressCountryCode& country_code_1,
                                const NameInfo& name_2,
                                const AddressCountryCode& country_code_2) {
  if constexpr (std::same_as<T, AlternativeFullName>) {
    if (!name_1.IsAlternativeNameSupported() &&
        !name_2.IsAlternativeNameSupported()) {
      return true;
    }
    if (name_1.IsAlternativeNameSupported() ^
        name_2.IsAlternativeNameSupported()) {
      return false;
    }
  }

  const AddressCountryCode common_country_code =
      AddressComponent::GetCommonCountry(country_code_1, country_code_2);
  const std::u16string comparison_name_1 =
      GetNameForComparison<T>(name_1, common_country_code);
  const std::u16string comparison_name_2 =
      GetNameForComparison<T>(name_2, common_country_code);

  if (normalization::HasOnlySkippableCharacters(comparison_name_1) ||
      normalization::HasOnlySkippableCharacters(comparison_name_2) ||
      AutofillProfileComparator::Compare(
          comparison_name_1, comparison_name_2,
          normalization::WhitespaceSpec::kDiscard, GetFieldType<T>(),
          country_code_1, country_code_2)) {
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
  return IsNormalizedNameVariantOf(canon_full_name_1, canon_full_name_2) ||
         IsNormalizedNameVariantOf(canon_full_name_2, canon_full_name_1);
}

template <NameType T>
std::unique_ptr<T> MergeNameComponents(
    const NameInfo& new_name_info,
    const AddressCountryCode& new_country_code,
    const NameInfo& old_name_info,
    const AddressCountryCode& old_country_code,
    bool newer_was_more_recently_used) {
  static constexpr FieldType kNameType = GetFieldType<T>();

  // At this stage, it has been already determined that the two names are
  // mergeable. This can mean one of the following things:
  // * One name is empty. In this scenario the non-empty name is used.
  // * The names are token equivalent: In this scenario a merge of the tree
  // structure should be possible.
  // * One name is a variant of the other. In this scenario, use the non-variant
  // name.
  const AddressCountryCode common_country_code =
      AddressComponent::GetCommonCountry(new_country_code, old_country_code);
  const std::u16string name_new = NormalizeForComparison(
      GetNameForComparison<T>(new_name_info, common_country_code),
      normalization::WhitespaceSpec::kRetain, new_country_code);
  const std::u16string name_old = NormalizeForComparison(
      GetNameForComparison<T>(old_name_info, common_country_code),
      normalization::WhitespaceSpec::kRetain, old_country_code);

  std::unique_ptr<T> name_component = std::make_unique<T>();

  // First, set info to the original profile.
  name_component->CopyFrom(*old_name_info.GetRootForType(kNameType));
  // If the name of the `new_profile` is empty, just keep the state of
  // `old_profile`.
  if (normalization::HasOnlySkippableCharacters(name_new)) {
    return name_component;
  }
  // Vice versa set name to the one of `new_profile` if `old_profile` has an
  // empty name
  if (normalization::HasOnlySkippableCharacters(name_old)) {
    name_component->CopyFrom(*new_name_info.GetRootForType(kNameType));
    return name_component;
  }
  // Try to apply a direct merging.
  if (name_component->MergeWithComponent(
          *new_name_info.GetRootForType(kNameType),
          newer_was_more_recently_used)) {
    return name_component;
  }
  // If the name in `old_profile` is a variant of `new_profile` use the one in
  // `new_profile`.
  if (IsNormalizedNameVariantOf(name_new, name_old)) {
    name_component->CopyFrom(*new_name_info.GetRootForType(kNameType));
  } else {
    name_component->CopyFrom(*old_name_info.GetRootForType(kNameType));
  }
  return name_component;
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

NameInfo::NameInfo(NameInfo&& info) noexcept = default;

NameInfo::NameInfo(std::unique_ptr<NameFull> name,
                   std::unique_ptr<AlternativeFullName> alternative_name)
    : name_(std::move(name)), alternative_name_(std::move(alternative_name)) {}

NameInfo& NameInfo::operator=(const NameInfo& info) {
  if (this == &info) {
    return *this;
  }

  name_->CopyFrom(*info.name_);
  if (info.IsAlternativeNameSupported()) {
    alternative_name_ = std::make_unique<AlternativeFullName>();
    alternative_name_->CopyFrom(*info.alternative_name_);
  } else if (alternative_name_) {
    alternative_name_.reset();
  }

  return *this;
}

NameInfo& NameInfo::operator=(NameInfo&& info) noexcept = default;

NameInfo::~NameInfo() = default;

// static
base::expected<NameInfo, NameInfo::MergeFailureReason> NameInfo::MergeNames(
    const NameInfo& new_name_info,
    const AddressCountryCode& new_country_code,
    const NameInfo& old_name_info,
    const AddressCountryCode& old_country_code,
    bool newer_was_more_recently_used) {
  using enum MergeFailureReason;
  const bool name_full_mergeable = AreNameComponentsMergeable<NameFull>(
      new_name_info, new_country_code, old_name_info, old_country_code);
  const bool alternative_name_mergeable =
      AreNameComponentsMergeable<AlternativeFullName>(
          new_name_info, new_country_code, old_name_info, old_country_code);

  if (!name_full_mergeable && !alternative_name_mergeable) {
    return base::unexpected(kBothFailed);
  }

  if (!alternative_name_mergeable) {
    return base::unexpected(kAlternativeNameFailed);
  }

  if (!name_full_mergeable) {
    return base::unexpected(kNameFullFailed);
  }

  return NameInfo(MergeNameComponents<NameFull>(new_name_info, new_country_code,
                                                old_name_info, old_country_code,
                                                newer_was_more_recently_used),
                  new_name_info.IsAlternativeNameSupported()
                      ? MergeNameComponents<AlternativeFullName>(
                            new_name_info, new_country_code, old_name_info,
                            old_country_code, newer_was_more_recently_used)
                      : nullptr);
}

bool NameInfo::MergeStructuredName(const NameInfo& newer,
                                   bool newer_was_more_recently_used) {
  // It should never happen in practice as this method is used to override the
  // `AutofillProfile` owning `this` with data coming from sync. Since their
  // GUIDs have to match, the country of both profiles (and thus the support for
  // alternative names) should be the same.
  if (!HaveSimilarAlternativeNameSupport(newer)) {
    return false;
  }

  // Check the mergeability of the full name. A full name must be present in all
  // valid (not-moved-from) `NameInfo` objects.
  if (!name_->IsMergeableWithComponent(*newer.name_)) {
    return false;
  }

  // The presence of an alternative name is not mandatory; however, if one
  // exists, its mergeability must be checked. In such a case, `NameInfo` is
  // mergeable only if both of its components are mergeable with the respective
  // components of the `newer` object.
  if (IsAlternativeNameSupported() && newer.IsAlternativeNameSupported()) {
    if (!alternative_name_->IsMergeableWithComponent(
            *newer.alternative_name_)) {
      return false;
    }
    // This must return true because `IsMergeableWithComponent` checks for both
    // name components were already performed and had their results been
    // negative this method would have returned earlier.
    return name_->MergeWithComponent(*newer.name_,
                                     newer_was_more_recently_used) &&
           alternative_name_->MergeWithComponent(*newer.alternative_name_,
                                                 newer_was_more_recently_used);
  }

  // The alternative name does not exist, so only merge the full name. This must
  // return true, because the `IsMergeableWithComponent` check for the full name
  // was called earlier and would have returned early had it had a negative
  // result.
  return name_->MergeWithComponent(*newer.name_, newer_was_more_recently_used);
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
  re2::RE2::GlobalReplace(&full_name, kCjkNameSeparatorsRe, "");
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

bool NameInfo::FinalizeAfterImport() {
  bool result = FinalizeNameAddressComponent(name_.get());
  if (IsAlternativeNameSupported()) {
    result &= FinalizeNameAddressComponent(alternative_name_.get());
  }
  return result;
}

bool NameInfo::operator==(const NameInfo& other) const {
  if (this == &other) {
    return true;
  }

  // If only one of the profiles supports the alternative name, the two
  // `NameInfo`s are different.
  if (!HaveSimilarAlternativeNameSupport(other)) {
    return false;
  }

  if (IsAlternativeNameSupported()) {
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
  if (IsAlternativeNameSupported()) {
    supported_types.insert_all(alternative_name_->GetSupportedTypes());
  }
  return supported_types;
}

std::u16string NameInfo::GetInfo(FieldType type,
                                 std::string_view app_locale) const {
  return GetRawInfo(type);
}

bool NameInfo::SetInfoWithVerificationStatus(FieldType type,
                                             std::u16string_view value,
                                             std::string_view app_locale,
                                             VerificationStatus status) {
  if (type == NAME_FULL ||
      (type == ALTERNATIVE_FULL_NAME && IsAlternativeNameSupported())) {
    // If the set string is token equivalent to the old one, the value can
    // just be updated, otherwise create a new name record and complete it in
    // the end.
    // TODO(crbug.com/40266145): Move this logic to the data model.
    AreStringTokenEquivalent(value, GetRootForType(type)->GetValueForType(type))
        ? GetRootForType(type)->SetValueForType(type, value, status)
        : GetRootForType(type)->SetValueForType(
              type, value, status,
              /*invalidate_child_nodes=*/true);
    return true;
  }
  SetRawInfoWithVerificationStatus(type, value, status);
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

  alternative_name_ = std::make_unique<AlternativeFullName>();
}

void NameInfo::DeleteAlternativeNameTree() {
  alternative_name_.reset();
}

bool NameInfo::IsAlternativeNameSupported() const {
  return alternative_name_ != nullptr;
}

bool NameInfo::HaveSimilarAlternativeNameSupport(const NameInfo& other) const {
  return IsAlternativeNameSupported() == other.IsAlternativeNameSupported();
}

}  // namespace autofill
