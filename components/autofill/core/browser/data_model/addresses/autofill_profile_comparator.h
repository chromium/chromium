// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_PROFILE_COMPARATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_PROFILE_COMPARATOR_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_util.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/company_info.h"
#include "components/autofill/core/browser/data_model/addresses/email_info.h"
#include "components/autofill/core/browser/data_model/addresses/phone_number.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class Address;

struct ProfileValueDifference {
  // The type of the field that is different.
  FieldType type;
  // The original value.
  std::u16string first_value;
  // The new value.
  std::u16string second_value;
  bool operator==(const ProfileValueDifference& right) const = default;
};

// A utility class to assist in the comparison of AutofillProfile data.
class AutofillProfileComparator {
 public:
  explicit AutofillProfileComparator(std::string_view app_locale);

  AutofillProfileComparator(const AutofillProfileComparator&) = delete;
  AutofillProfileComparator& operator=(const AutofillProfileComparator&) =
      delete;

  ~AutofillProfileComparator();

  // Returns true if `text1` matches `text2`. The following normalization
  // techniques are applied to the given texts before comparing.
  //
  // (1) Diacritics are rewritten using country specific rules
  //     (`country_code_1` for `text1`, `country_code_2` for `text2`)
  //     e.g. حَ to ح, ビ to ヒ, and é to e;
  // (2) Leading and trailing whitespace and punctuation are ignored;
  // (3) Characters are converted to lowercase;
  // (4) For alternative name types, katakana is converted to hiragana.
  //
  // If `whitespace_spec` is kDiscard, then punctuation and whitespace
  // are discarded. For example, the postal codes "B15 3TR" and "B153TR" and
  // street addresses "16 Bridge St." and "16 Bridge St" are considered equal.
  //
  // If `whitespace_spec` is kRetain, then the postal codes "B15 3TR"
  // and "B153TR" are not considered equal, but "16 Bridge St." and "16 Bridge
  // St" are because trailing whitespace and punctuation are ignored.
  static bool Compare(
      std::u16string_view text1,
      std::u16string_view text2,
      normalization::WhitespaceSpec whitespace_spec,
      std::optional<FieldType> type = std::nullopt,
      AddressCountryCode country_code_1 = AddressCountryCode(""),
      AddressCountryCode country_code_2 = AddressCountryCode(""));

  // Returns true if two AutofillProfiles `p1` and `p2` have at least one
  // settings-visible value that is different.
  static bool ProfilesHaveDifferentSettingsVisibleValues(
      const AutofillProfile& p1,
      const AutofillProfile& p2,
      const std::string& app_locale);

  // Get the difference in 'types' of two profiles. The difference is determined
  // with respect to the provided `app_locale`.
  static std::vector<ProfileValueDifference> GetProfileDifference(
      const AutofillProfile& first_profile,
      const AutofillProfile& second_profile,
      FieldTypeSet types,
      const std::string& app_locale);

  // Get the difference of two profiles for settings-visible values.
  // The difference is determined with respect to the provided `app_locale`.
  static std::vector<ProfileValueDifference>
  GetSettingsVisibleProfileDifference(const AutofillProfile& first_profile,
                                      const AutofillProfile& second_profile,
                                      const std::string& app_locale);

  // Merges the email addresses in `new_profile` and `old_profile`. Returns an
  // `EmailInfo` on success or `std::nullopt` if the merge fails.
  //
  // Heuristic: If one email address is empty, use the other; otherwise, prefer
  // the most recently used version of the email address.
  std::optional<EmailInfo> MergeEmailAddresses(
      const AutofillProfile& new_profile,
      const AutofillProfile& old_profile) const;

  // Merges the company names in `new_profile` and `old_profile`. Returns a
  // `CompanyInfo` on success or `std::nullopt` if the merge fails.
  //
  // Heuristic: If one is empty, use the other; otherwise, if the tokens in one
  // company name are a superset of those in the other, prefer the former; and,
  // as a tiebreaker, prefer the most recently used version of the company name.
  std::optional<CompanyInfo> MergeCompanyNames(
      const AutofillProfile& new_profile,
      const AutofillProfile& old_profile) const;

  // Merges the phone numbers in `new_profile` and `old_profile`. Returns a
  // `PhoneNumber` on success or `std::nullopt` if the merge fails.
  //
  // Heuristic: Populate the missing parts of each number from the other.
  std::optional<PhoneNumber> MergePhoneNumbers(
      const AutofillProfile& new_profile,
      const AutofillProfile& old_profile) const;

  // Populates `address` with the result of merging the addresses in
  // `new_profile` and `old_profile`. Returns the merge result.
  //
  // Heuristic: Populate the missing parts of each address from the other.
  // Prefer the abbreviated state, the shorter zip code and routing code, the
  // more verbose city, dependent locality, and address.
  //
  // If one of the profiles is `kAccountNameEmail`, sets `address` to the
  // address tree of the other profile.
  //
  // TODO(crbug.com/453945181): Return a newly created `Address` instead of
  // modifying `address`.
  AutofillProfile::ProfileMergeResult MergeAddresses(
      const AutofillProfile& new_profile,
      const AutofillProfile& old_profile,
      Address& address) const;

  // Returns the subset of setting-visible types whose values in `a` and `b` are
  // non-mergeable. This means that `a` and `b` become mergeable, if the values
  // for all types returned by this function and their substructures are
  // cleared. As such, the size of the returned set can be interpreted as a
  // dissimilarity measure of `a` and `b`. If `a` and `b` differ in country,
  // nullopt is returned, since profiles of different countries are generally
  // not considered mergeable due to the differences in the underlying address
  // model.
  std::optional<FieldTypeSet> NonMergeableSettingVisibleTypes(
      const AutofillProfile& a,
      const AutofillProfile& b) const;

  // App locale used when this comparator instance was created.
  const std::string app_locale() const { return app_locale_; }

 protected:
  // The result type returned by CompareTokens.
  enum CompareTokensResult {
    DIFFERENT_TOKENS,
    SAME_TOKENS,
    S1_CONTAINS_S2,
    S2_CONTAINS_S1,
  };

  // Returns the set of unique tokens in `s`. Note that the string data backing
  // `s` is expected to have a lifetime which exceeds the call to UniqueTokens.
  static std::set<std::u16string_view> UniqueTokens(std::u16string_view s);

  // Compares the unique tokens in s1 and s2.
  static CompareTokensResult CompareTokens(std::u16string_view s1,
                                           std::u16string_view s2);

  // Returns the value of `t` from `p1` or `p2` depending on which is non-empty.
  // This method expects that the value is either the same in `p1` and `p2` or
  // empty in one of them.
  // TODO(crbug.com/40264633): Pass a `FieldType` instead of `AutofillType`.
  std::u16string GetNonEmptyOf(const AutofillProfile& p1,
                               const AutofillProfile& p2,
                               AutofillType t) const;

 private:
  const std::string app_locale_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_PROFILE_COMPARATOR_H_
