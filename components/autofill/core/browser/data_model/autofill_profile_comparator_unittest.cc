// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/contact_info.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using i18n_model_definition::kLegacyHierarchyCountryCode;

const char kLocale[] = "en-US";

class AutofillProfileComparatorTest : public testing::Test {
 public:
  // Expose the protected methods of AutofillProfileComparator for testing.
  class AutofillProfileComparator : public autofill::AutofillProfileComparator {
   public:
    typedef autofill::AutofillProfileComparator Super;
    using Super::CompareTokens;
    using Super::GetNamePartVariants;
    using Super::HaveMergeableAddresses;
    using Super::HaveMergeableCompanyNames;
    using Super::HaveMergeableEmailAddresses;
    using Super::HaveMergeableNames;
    using Super::HaveMergeablePhoneNumbers;
    using Super::IsNameVariantOf;
    using Super::Super;
    using Super::UniqueTokens;

    using Super::DIFFERENT_TOKENS;
    using Super::S1_CONTAINS_S2;
    using Super::S2_CONTAINS_S1;
    using Super::SAME_TOKENS;
  };

  AutofillProfileComparatorTest() = default;
  AutofillProfileComparatorTest(const AutofillProfileComparatorTest&) = delete;
  AutofillProfileComparatorTest& operator=(
      const AutofillProfileComparatorTest&) = delete;

  NameInfo CreateNameInfo(const char16_t* first,
                          const char16_t* middle,
                          const char16_t* last,
                          const char16_t* full) {
    NameInfo name;
    name.SetRawInfoWithVerificationStatus(NAME_FIRST, first,
                                          VerificationStatus::kObserved);
    name.SetRawInfoWithVerificationStatus(NAME_MIDDLE, middle,
                                          VerificationStatus::kObserved);
    name.SetRawInfoWithVerificationStatus(NAME_LAST, last,
                                          VerificationStatus::kObserved);
    name.SetRawInfoWithVerificationStatus(NAME_FULL, full,
                                          VerificationStatus::kObserved);
    return name;
  }

  AutofillProfile CreateProfileWithName(const char* first,
                                        const char* middle,
                                        const char* last,
                                        bool finalize = true) {
    AutofillProfile profile(kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile, first, middle, last, "", "", "", "", "", "",
                         "", "", "");
    if (finalize)
      profile.FinalizeAfterImport();
    return profile;
  }

  AutofillProfile CreateProfileWithName(const NameInfo& name,
                                        bool finalize = true) {
    AutofillProfile profile(kLegacyHierarchyCountryCode);
    profile.SetRawInfoWithVerificationStatus(
        NAME_FULL, name.GetRawInfo(NAME_FULL),
        name.GetVerificationStatus(NAME_FULL));
    profile.SetRawInfoWithVerificationStatus(
        NAME_FIRST, name.GetRawInfo(NAME_FIRST),
        name.GetVerificationStatus(NAME_MIDDLE));
    profile.SetRawInfoWithVerificationStatus(
        NAME_MIDDLE, name.GetRawInfo(NAME_MIDDLE),
        name.GetVerificationStatus(NAME_MIDDLE));
    profile.SetRawInfoWithVerificationStatus(
        NAME_LAST, name.GetRawInfo(NAME_LAST),
        name.GetVerificationStatus(NAME_LAST));
    if (finalize)
      profile.FinalizeAfterImport();

    return profile;
  }

  AutofillProfile CreateProfileWithEmail(const char* email) {
    AutofillProfile profile(kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile, "", "", "", email, "", "", "", "", "", "",
                         "", "");
    return profile;
  }

  AutofillProfile CreateProfileWithCompanyName(const char* company_name) {
    AutofillProfile profile(kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile, "", "", "", "", company_name, "", "", "", "",
                         "", "", "");
    return profile;
  }

  AutofillProfile CreateProfileWithPhoneNumber(const char* phone_number) {
    AutofillProfile profile(kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile, "", "", "", "", "", "", "", "", "", "", "",
                         phone_number);
    return profile;
  }

  AutofillProfile CreateProfileWithAddress(const char* line1,
                                           const char* line2,
                                           const char* city,
                                           const char* state,
                                           const char* zip,
                                           const char* country) {
    AutofillProfile profile(AddressCountryCode{country});
    test::SetProfileInfo(&profile, "", "", "", "", "", line1, line2, city,
                         state, zip, country, "");
    return profile;
  }

  AutofillProfile CopyAndModify(
      const AutofillProfile& profile,
      const std::vector<std::pair<FieldType, const char16_t*>>& updates) {
    AutofillProfile new_profile = profile;
    for (const auto& [field_type, value] : updates) {
      new_profile.SetRawInfo(field_type, value);
    }
    new_profile.FinalizeAfterImport();
    return new_profile;
  }

  void MergeNamesAndExpect(const AutofillProfile& a,
                           const AutofillProfile& b,
                           const NameInfo& expected) {
    NameInfo actual;
    ASSERT_TRUE(comparator_.MergeNames(a, b, actual));

    // Is the "processed" data correct?
    EXPECT_EQ(expected.GetInfo(NAME_FULL, kLocale),
              actual.GetInfo(NAME_FULL, kLocale));
    EXPECT_EQ(expected.GetInfo(NAME_FIRST, kLocale),
              actual.GetInfo(NAME_FIRST, kLocale));
    EXPECT_EQ(expected.GetInfo(NAME_MIDDLE, kLocale),
              actual.GetInfo(NAME_MIDDLE, kLocale));
    EXPECT_EQ(expected.GetInfo(NAME_LAST, kLocale),
              actual.GetInfo(NAME_LAST, kLocale));

    // Is the raw data correct?
    EXPECT_EQ(expected.GetRawInfo(NAME_FULL), actual.GetRawInfo(NAME_FULL));
    EXPECT_EQ(expected.GetRawInfo(NAME_FIRST), actual.GetRawInfo(NAME_FIRST));
    EXPECT_EQ(expected.GetRawInfo(NAME_MIDDLE), actual.GetRawInfo(NAME_MIDDLE));
    EXPECT_EQ(expected.GetRawInfo(NAME_LAST), actual.GetRawInfo(NAME_LAST));
  }

  void MergeEmailAddressesAndExpect(const AutofillProfile& a,
                                    const AutofillProfile& b,
                                    const EmailInfo& expected) {
    EmailInfo actual;
    ASSERT_TRUE(comparator_.MergeEmailAddresses(a, b, actual));
    EXPECT_EQ(expected.GetRawInfo(EMAIL_ADDRESS),
              actual.GetRawInfo(EMAIL_ADDRESS));
  }

  void MergeCompanyNamesAndExpect(const AutofillProfile& a,
                                  const AutofillProfile& b,
                                  const CompanyInfo& expected) {
    CompanyInfo actual;
    ASSERT_TRUE(comparator_.MergeCompanyNames(a, b, actual));
    EXPECT_EQ(expected.GetRawInfo(COMPANY_NAME),
              actual.GetRawInfo(COMPANY_NAME));
  }

  void MergePhoneNumbersAndExpect(const AutofillProfile& a,
                                  const AutofillProfile& b,
                                  const std::u16string& expected_str) {
    AutofillProfile dummy(kLegacyHierarchyCountryCode);

    // Merge the phone numbers.
    PhoneNumber actual(&dummy);
    ASSERT_TRUE(comparator_.MergePhoneNumbers(a, b, actual));

    // Construct the expected value.
    PhoneNumber expected(&dummy);
    expected.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, expected_str);

    // Validate that we get what we expect.
    EXPECT_EQ(expected.GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
              actual.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
    EXPECT_EQ(expected.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale),
              actual.GetInfo(PHONE_HOME_WHOLE_NUMBER, kLocale));
    EXPECT_EQ(expected.GetInfo(PHONE_HOME_COUNTRY_CODE, kLocale),
              actual.GetInfo(PHONE_HOME_COUNTRY_CODE, kLocale));
    EXPECT_EQ(expected.GetInfo(PHONE_HOME_CITY_AND_NUMBER, kLocale),
              actual.GetInfo(PHONE_HOME_CITY_AND_NUMBER, kLocale));
    EXPECT_EQ(expected.GetInfo(PHONE_HOME_CITY_CODE, kLocale),
              actual.GetInfo(PHONE_HOME_CITY_CODE, kLocale));
    EXPECT_EQ(expected.GetInfo(PHONE_HOME_NUMBER, kLocale),
              actual.GetInfo(PHONE_HOME_NUMBER, kLocale));
  }

  void MergeAddressesAndExpect(const AutofillProfile& a,
                               const AutofillProfile& b,
                               const Address& expected,
                               bool check_structured_address_tokens = false) {
    Address actual(kLegacyHierarchyCountryCode);
    ASSERT_TRUE(comparator_.MergeAddresses(a, b, actual));

    EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_LINE1, kLocale),
              actual.GetInfo(ADDRESS_HOME_LINE1, kLocale));
    EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_LINE2, kLocale),
              actual.GetInfo(ADDRESS_HOME_LINE2, kLocale));
    EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_LINE3, kLocale),
              actual.GetInfo(ADDRESS_HOME_LINE3, kLocale));
    EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_STREET_ADDRESS, kLocale),
              actual.GetInfo(ADDRESS_HOME_STREET_ADDRESS, kLocale));
    EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, kLocale),
              actual.GetInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, kLocale));
    EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_SORTING_CODE, kLocale),
              actual.GetInfo(ADDRESS_HOME_SORTING_CODE, kLocale));
    EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_CITY, kLocale),
              actual.GetInfo(ADDRESS_HOME_CITY, kLocale));
    EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_STATE, kLocale),
              actual.GetInfo(ADDRESS_HOME_STATE, kLocale));
    EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_ZIP, kLocale),
              actual.GetInfo(ADDRESS_HOME_ZIP, kLocale));
    EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_COUNTRY, kLocale),
              actual.GetInfo(ADDRESS_HOME_COUNTRY, kLocale));

    if (check_structured_address_tokens) {
      EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_STREET_NAME, kLocale),
                actual.GetInfo(ADDRESS_HOME_STREET_NAME, kLocale));
      EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_HOUSE_NUMBER, kLocale),
                actual.GetInfo(ADDRESS_HOME_HOUSE_NUMBER, kLocale));
      EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_SUBPREMISE, kLocale),
                actual.GetInfo(ADDRESS_HOME_SUBPREMISE, kLocale));
      EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_LANDMARK, kLocale),
                actual.GetInfo(ADDRESS_HOME_LANDMARK, kLocale));
      EXPECT_EQ(expected.GetInfo(ADDRESS_HOME_BETWEEN_STREETS, kLocale),
                actual.GetInfo(ADDRESS_HOME_BETWEEN_STREETS, kLocale));
    }
  }

  AutofillProfileComparator comparator_{kLocale};
};

TEST_F(AutofillProfileComparatorTest, UniqueTokens) {
  std::u16string kInput = u"a b a a b";
  std::vector<std::u16string> tokens = {u"a", u"b"};
  EXPECT_EQ(std::set<std::u16string_view>(tokens.begin(), tokens.end()),
            comparator_.UniqueTokens(kInput));
}

TEST_F(AutofillProfileComparatorTest, CompareTokens) {
  std::u16string kEmptyStr = u"";
  std::u16string kHello = u"hello";
  std::u16string kHelloThere = u"hello there";
  std::u16string kHelloThereAlice = u"hello there alice";
  std::u16string kHelloThereBob = u"hello there bob";

  EXPECT_EQ(AutofillProfileComparator::SAME_TOKENS,
            comparator_.CompareTokens(kHelloThereBob, kHelloThereBob));
  EXPECT_EQ(AutofillProfileComparator::S2_CONTAINS_S1,
            comparator_.CompareTokens(kEmptyStr, kHello));
  EXPECT_EQ(AutofillProfileComparator::S1_CONTAINS_S2,
            comparator_.CompareTokens(kHello, kEmptyStr));
  EXPECT_EQ(AutofillProfileComparator::S1_CONTAINS_S2,
            comparator_.CompareTokens(kHelloThere, kHello));
  EXPECT_EQ(AutofillProfileComparator::S2_CONTAINS_S1,
            comparator_.CompareTokens(kHello, kHelloThere));
  EXPECT_EQ(AutofillProfileComparator::DIFFERENT_TOKENS,
            comparator_.CompareTokens(kHelloThereAlice, kHelloThereBob));
  EXPECT_EQ(AutofillProfileComparator::DIFFERENT_TOKENS,
            comparator_.CompareTokens(kHelloThereBob, kHelloThereAlice));
}

TEST_F(AutofillProfileComparatorTest, Compare) {
  // Checks the empty case.
  EXPECT_TRUE(
      comparator_.Compare(std::u16string(), std::u16string(),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(std::u16string(), std::u16string(),
                          AutofillProfileComparator::DISCARD_WHITESPACE));

  // Checks that leading punctuation and white space are ignored.
  EXPECT_TRUE(comparator_.Compare(
      u".,  -().", u"", AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(comparator_.Compare(
      u".,  -().", u"", AutofillProfileComparator::DISCARD_WHITESPACE));

  // Checks that trailing punctuation and white space are ignored.
  EXPECT_TRUE(comparator_.Compare(
      u"a ., ", u"a", AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(comparator_.Compare(
      u"a ., ", u"a", AutofillProfileComparator::DISCARD_WHITESPACE));

  // Checks that embedded punctuation and white space is collapsed to a single
  // white space with RETAIN_WHITESPACE and is ignored with DISCARD_WHITESPACE.
  EXPECT_TRUE(comparator_.Compare(
      u"a() -  a", u"a a", AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(comparator_.Compare(
      u"a() -  a", u"aa", AutofillProfileComparator::DISCARD_WHITESPACE));

  // Checks that characters such as 'œ' respect the status quo established by
  // NormalizeForComparison.
  EXPECT_TRUE(comparator_.Compare(u"œil", u"oeil"));
  EXPECT_TRUE(comparator_.Compare(
      u"Straße", u"Strasse", AutofillProfileComparator::DISCARD_WHITESPACE));

  // Checks that a substring of the string is not considered equal.
  EXPECT_FALSE(comparator_.Compare(u"A", u"Anna"));

  EXPECT_FALSE(comparator_.Compare(u"Anna", u"A"));

  // Checks that Compare behaves like NormalizeForComparison. Also, checks that
  // diacritics are removed.
  EXPECT_TRUE(comparator_.Compare(
      u"Timothé", u"timothe", AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(u" sven-åke ", u"sven ake",
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(comparator_.Compare(
      u"Ç 㸐", u"c 㸐", AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(u"902103214", u"90210-3214",
                          AutofillProfileComparator::DISCARD_WHITESPACE));
  EXPECT_TRUE(comparator_.Compare(
      u"Timothé-Noël Étienne Périer", u"timothe noel etienne perier",
      AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(comparator_.Compare(
      u"1600 Amphitheatre, Pkwy.", u"1600 amphitheatre pkwy",
      AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(u"Mid\x2013Island\x2003 Plaza", u"mid island plaza",
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(comparator_.Compare(
      u"1600 amphitheatre pkwy \n App. 2", u"1600 amphitheatre pkwy app 2",
      AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(comparator_.Compare(
      u"まéÖä정", u"まeoa정", AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(comparator_.Compare(
      u"유재석", u"유 재석", AutofillProfileComparator::DISCARD_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(u"ビルゲイツ", u"ヒル・ケイツ",
                          AutofillProfileComparator::DISCARD_WHITESPACE));
}

TEST_F(AutofillProfileComparatorTest, NormalizeForComparison) {
  EXPECT_EQ(u"timothe",
            AutofillProfileComparator::NormalizeForComparison(u"Timothé"));
  EXPECT_EQ(u"sven ake",
            AutofillProfileComparator::NormalizeForComparison(u" sven-åke "));
  EXPECT_EQ(u"c 㸐",
            AutofillProfileComparator::NormalizeForComparison(u"Ç 㸐"));
  EXPECT_EQ(u"902103214",
            AutofillProfileComparator::NormalizeForComparison(
                u"90210-3214", AutofillProfileComparator::DISCARD_WHITESPACE));
  EXPECT_EQ(u"timothe noel etienne perier",
            AutofillProfileComparator::NormalizeForComparison(
                u"Timothé-Noël Étienne Périer"));
  EXPECT_EQ(u"strasse",
            AutofillProfileComparator::NormalizeForComparison(u"Straße"));
  // NOP.
  EXPECT_EQ(std::u16string(), AutofillProfileComparator::NormalizeForComparison(
                                  std::u16string()));

  // Simple punctuation removed.
  EXPECT_EQ(u"1600 amphitheatre pkwy",
            AutofillProfileComparator::NormalizeForComparison(
                u"1600 Amphitheatre, Pkwy."));

  // Unicode punctuation (hyphen and space), multiple spaces collapsed.
  EXPECT_EQ(u"mid island plaza",
            AutofillProfileComparator::NormalizeForComparison(
                u"Mid\x2013Island\x2003 Plaza"));

  // Newline character removed.
  EXPECT_EQ(u"1600 amphitheatre pkwy app 2",
            AutofillProfileComparator::NormalizeForComparison(
                u"1600 amphitheatre pkwy \n App. 2"));

  // Diacritics removed.
  EXPECT_EQ(u"まeoa정",
            AutofillProfileComparator::NormalizeForComparison(u"まéÖä정"));

  // Spaces removed.
  EXPECT_EQ(u"유재석",
            AutofillProfileComparator::NormalizeForComparison(
                u"유 재석", AutofillProfileComparator::DISCARD_WHITESPACE));

  // Punctuation removed, Japanese kana normalized.
  EXPECT_EQ(u"ヒルケイツ", AutofillProfileComparator::NormalizeForComparison(
                               u"ビル・ゲイツ",
                               AutofillProfileComparator::DISCARD_WHITESPACE));
}

TEST_F(AutofillProfileComparatorTest, GetNamePartVariants) {
  std::set<std::u16string> expected_variants = {
      u"timothe noel", u"timothe n", u"timothe", u"t noel", u"t n", u"t",
      u"noel",         u"n",         u"",        u"tn",
  };

  EXPECT_EQ(expected_variants,
            comparator_.GetNamePartVariants(u"timothe noel"));
}

TEST_F(AutofillProfileComparatorTest, IsNameVariantOf) {
  const std::u16string kNormalizedFullName = u"timothe noel etienne perier";

  EXPECT_TRUE(
      comparator_.IsNameVariantOf(kNormalizedFullName, kNormalizedFullName));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          u"t noel etienne perier"));
  EXPECT_TRUE(
      comparator_.IsNameVariantOf(kNormalizedFullName, u"timothe perier"));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName, u"t perier"));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName, u"noel perier"));
  EXPECT_TRUE(
      comparator_.IsNameVariantOf(kNormalizedFullName, u"t n etienne perier"));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName, u"tn perier"));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName, u"te perier"));

  EXPECT_FALSE(
      comparator_.IsNameVariantOf(kNormalizedFullName, u"etienne noel perier"));
}

TEST_F(AutofillProfileComparatorTest, HaveMergeableNames) {
  AutofillProfile empty = CreateProfileWithName("", "", "");

  AutofillProfile p1 = CreateProfileWithName("sven-åke", "", "larsson");
  AutofillProfile p2 = CreateProfileWithName("Åke", "", "Larsson");
  AutofillProfile p3 = CreateProfileWithName("A", "", "Larsson");
  AutofillProfile p4 = CreateProfileWithName("sven", "ake", "Larsson");

  AutofillProfile initials = CreateProfileWithName("SA", "", "Larsson");

  AutofillProfile different = CreateProfileWithName("Joe", "", "Larsson");

  // |p1|, |p2|, |p3|, |p4| and |empty| should all be the mergeable with
  // one another. The order of the comparands should not matter.
  EXPECT_TRUE(comparator_.HaveMergeableNames(p1, empty));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p1, p1));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p1, p2));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p1, p3));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p1, p4));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p2, empty));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p2, p1));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p2, p2));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p2, p3));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p2, p4));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p3, empty));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p3, p1));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p3, p2));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p3, p3));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p3, p4));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p4, empty));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p4, p1));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p4, p2));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p4, p3));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p4, p4));
  EXPECT_TRUE(comparator_.HaveMergeableNames(empty, empty));
  EXPECT_TRUE(comparator_.HaveMergeableNames(empty, p1));
  EXPECT_TRUE(comparator_.HaveMergeableNames(empty, p2));
  EXPECT_TRUE(comparator_.HaveMergeableNames(empty, p3));
  EXPECT_TRUE(comparator_.HaveMergeableNames(empty, p4));

  // |initials| is mergeable with |p1| and |p4| but not |p2| or |p3|.
  EXPECT_TRUE(comparator_.HaveMergeableNames(initials, empty));
  EXPECT_TRUE(comparator_.HaveMergeableNames(initials, p1));
  EXPECT_TRUE(comparator_.HaveMergeableNames(initials, p4));
  EXPECT_TRUE(comparator_.HaveMergeableNames(empty, initials));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p1, initials));
  EXPECT_TRUE(comparator_.HaveMergeableNames(p4, initials));
  EXPECT_FALSE(comparator_.HaveMergeableNames(initials, p2));
  EXPECT_FALSE(comparator_.HaveMergeableNames(initials, p3));
  EXPECT_FALSE(comparator_.HaveMergeableNames(p2, initials));
  EXPECT_FALSE(comparator_.HaveMergeableNames(p3, initials));

  // None of the non-empty profiles should match |different|. The order of the
  // comparands should not matter.
  EXPECT_FALSE(comparator_.HaveMergeableNames(p1, different));
  EXPECT_FALSE(comparator_.HaveMergeableNames(p2, different));
  EXPECT_FALSE(comparator_.HaveMergeableNames(p3, different));
  EXPECT_FALSE(comparator_.HaveMergeableNames(p4, different));
  EXPECT_FALSE(comparator_.HaveMergeableNames(initials, different));
  EXPECT_FALSE(comparator_.HaveMergeableNames(different, p1));
  EXPECT_FALSE(comparator_.HaveMergeableNames(different, p2));
  EXPECT_FALSE(comparator_.HaveMergeableNames(different, p3));
  EXPECT_FALSE(comparator_.HaveMergeableNames(different, p4));
  EXPECT_FALSE(comparator_.HaveMergeableNames(different, initials));
}

TEST_F(AutofillProfileComparatorTest, HaveMergeableEmailAddresses) {
  AutofillProfile empty = CreateProfileWithEmail("");
  AutofillProfile p1 = CreateProfileWithEmail("FOO@BAR.COM");
  AutofillProfile p2 = CreateProfileWithEmail("foo@bar.com");
  AutofillProfile different = CreateProfileWithEmail("not@the-same.com");

  EXPECT_TRUE(comparator_.HaveMergeableEmailAddresses(p1, p2));
  EXPECT_TRUE(comparator_.HaveMergeableEmailAddresses(p2, p1));
  EXPECT_TRUE(comparator_.HaveMergeableEmailAddresses(p1, empty));
  EXPECT_TRUE(comparator_.HaveMergeableEmailAddresses(empty, p2));

  EXPECT_FALSE(comparator_.HaveMergeableEmailAddresses(p1, different));
  EXPECT_FALSE(comparator_.HaveMergeableEmailAddresses(different, p1));
}

TEST_F(AutofillProfileComparatorTest, HaveMergeableCompanyNames) {
  AutofillProfile empty = CreateProfileWithCompanyName("");
  AutofillProfile p1 = CreateProfileWithCompanyName("Nestlé S.A.");
  AutofillProfile p2 = CreateProfileWithCompanyName("Nestle");
  AutofillProfile different = CreateProfileWithCompanyName("Other Corp");

  EXPECT_TRUE(comparator_.HaveMergeableCompanyNames(p1, p2));
  EXPECT_TRUE(comparator_.HaveMergeableCompanyNames(p2, p1));
  EXPECT_TRUE(comparator_.HaveMergeableCompanyNames(p1, empty));
  EXPECT_TRUE(comparator_.HaveMergeableCompanyNames(empty, p2));

  EXPECT_FALSE(comparator_.HaveMergeableCompanyNames(p1, different));
  EXPECT_FALSE(comparator_.HaveMergeableCompanyNames(different, p1));
}

TEST_F(AutofillProfileComparatorTest, HaveMergeablePhoneNumbers) {
  AutofillProfile empty = CreateProfileWithPhoneNumber("");
  AutofillProfile p1 = CreateProfileWithPhoneNumber("+1 (800) 670-8700");
  AutofillProfile p2 = CreateProfileWithPhoneNumber("6708700");
  AutofillProfile different = CreateProfileWithPhoneNumber("1-800-321-4567");

  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p1, p1));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p1, p2));

  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p2, p1));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p2, p2));

  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p1, empty));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(empty, p2));

  EXPECT_FALSE(comparator_.HaveMergeablePhoneNumbers(p1, different));
  EXPECT_FALSE(comparator_.HaveMergeablePhoneNumbers(different, p1));
}

TEST_F(AutofillProfileComparatorTest, HaveMergeableAddresses) {
  // TODO(rogerm): Replace some of the tokens to also test the address
  // normalization string replacement rules. For example:
  //    - State/Province abbreviations
  //    - Street/St/Saint/Sainte
  //    - etc...
  AutofillProfile empty = CreateProfileWithAddress("", "", "", "", "", "");

  // Currently, South Africa uses the legacy address model which supports all
  // the field types set below.
  AutofillProfile p1 = CreateProfileWithAddress(
      "1 Some Street", "Unit 3", "Carver", "CA - California", "90210", "ZA");
  p1.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Some String");
  p1.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"64205 Biarritz CEDEX");

  AutofillProfile p2 = CreateProfileWithAddress(
      "Unit 3", "1 Some Street", "Suburb", "california", "90 210-3214", "ZA");
  AutofillProfile p3 = CreateProfileWithAddress("1 Some Street #3", "",
                                                "Carver City", "ca", "", "ZA");

  AutofillProfile differentCountry =
      CopyAndModify(p1, {{ADDRESS_HOME_COUNTRY, u"CA"}});
  AutofillProfile differentZip =
      CopyAndModify(p1, {{ADDRESS_HOME_ZIP, u"32145"}});
  AutofillProfile differentState = CopyAndModify(
      p1, {{ADDRESS_HOME_ZIP, u""}, {ADDRESS_HOME_STATE, u"Florida"}});
  AutofillProfile differentCity = CopyAndModify(
      p1, {{ADDRESS_HOME_ZIP, u""}, {ADDRESS_HOME_CITY, u"Metropolis"}});
  AutofillProfile differentAddress =
      CopyAndModify(p1, {{ADDRESS_HOME_LINE1, u"17 Park Lane"},
                         {ADDRESS_HOME_LINE2, u"Suite 150"}});
  AutofillProfile differentLocality =
      CopyAndModify(p1, {{ADDRESS_HOME_DEPENDENT_LOCALITY, u"Funky Chicken"}});
  AutofillProfile differentSortingCode =
      CopyAndModify(p1, {{ADDRESS_HOME_SORTING_CODE, u"98000 Monaco"}});

  // A profile with no country uses the legacy address and can be merged with
  // other profiles using the same hierarchy.
  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p1, empty));
  EXPECT_TRUE(comparator_.HaveMergeableAddresses(empty, p2));

  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p1, p3));
  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p3, p1));

  // |p2| matches neither |p1| nor |p3| because we can't resolve the mismatched
  // city/suburb names.
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, p2));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p2, p1));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p2, p3));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p3, p2));

  // Changing things about |p1| causes its copies to stop being mergeable.
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, differentCountry));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, differentZip));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, differentState));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, differentCity));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, differentAddress));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, differentLocality));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, differentSortingCode));
}

TEST_F(AutofillProfileComparatorTest, AreMergeable) {
  AutofillProfile p(AddressCountryCode("US"));
  test::SetProfileInfo(&p, "Marion", "Mitchell", "Morrison", "marion@me.xyz",
                       "Fox", "123 Zoo St.", "Unit 5", "Hollywood", "CA",
                       "91601", "US", "+1 (234) 567-8910", /*finalize=*/false);

  AutofillProfile mergeable =
      CopyAndModify(p, {{NAME_FIRST, u"MÁRÍÕÑ"},
                        {NAME_MIDDLE, u"M."},
                        {EMAIL_ADDRESS, u"MARION@ME.XYZ"},
                        {COMPANY_NAME, u"Fox Industries Inc."},
                        {ADDRESS_HOME_LINE1, u"123 zoo st. w., #5"},
                        {ADDRESS_HOME_LINE1, u""},
                        {ADDRESS_HOME_STATE, u"california"},
                        {PHONE_HOME_WHOLE_NUMBER, u"5678910"}});
  AutofillProfile not_mergeable_by_name = CopyAndModify(
      p, {{NAME_FIRST, u"Steven"}, {NAME_FULL, u""}, {NAME_LAST_SECOND, u""}});
  AutofillProfile not_mergeable_by_email_address =
      CopyAndModify(p, {{EMAIL_ADDRESS, u"marion.morrision@me.xyz"}});
  AutofillProfile not_mergeable_by_company_name =
      CopyAndModify(p, {{COMPANY_NAME, u"Hound Corp"}});
  AutofillProfile not_mergeable_by_address =
      CopyAndModify(p, {{ADDRESS_HOME_LINE2, u"Unit 7"}});
  AutofillProfile not_mergeable_by_phone_number =
      CopyAndModify(p, {{PHONE_HOME_WHOLE_NUMBER, u"555-1234"}});

  // Finalize the initial profile.
  // Note, all other profiles are already finalized.
  p.FinalizeAfterImport();

  EXPECT_TRUE(comparator_.AreMergeable(p, p));
  EXPECT_TRUE(comparator_.AreMergeable(p, mergeable));
  EXPECT_FALSE(comparator_.AreMergeable(p, not_mergeable_by_name));
  EXPECT_FALSE(comparator_.AreMergeable(p, not_mergeable_by_email_address));
  EXPECT_FALSE(comparator_.AreMergeable(p, not_mergeable_by_company_name));
  EXPECT_FALSE(comparator_.AreMergeable(p, not_mergeable_by_address));
  EXPECT_FALSE(comparator_.AreMergeable(p, not_mergeable_by_phone_number));
}

TEST_F(AutofillProfileComparatorTest, MergeNames_WithPermutation) {
  // The first name has an observed structure.
  NameInfo name1;
  name1.SetRawInfoWithVerificationStatus(NAME_FIRST, u"Thomas",
                                         VerificationStatus::kObserved);
  name1.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"A.",
                                         VerificationStatus::kObserved);
  name1.SetRawInfoWithVerificationStatus(NAME_LAST, u"Anderson",
                                         VerificationStatus::kObserved);
  AutofillProfile profile1 = CreateProfileWithName(name1);
  profile1.FinalizeAfterImport();

  EXPECT_EQ(profile1.GetRawInfo(NAME_FULL), u"Thomas A. Anderson");
  EXPECT_EQ(profile1.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kFormatted);

  // The second name has an observed full name that uses a custom formatting.
  NameInfo name2;
  name2.SetRawInfoWithVerificationStatus(NAME_FULL, u"Anderson, Thomas A.",
                                         VerificationStatus::kObserved);
  AutofillProfile profile2 = CreateProfileWithName(name2);
  profile2.FinalizeAfterImport();

  NameInfo merged_name;
  comparator_.MergeNames(profile1, profile2, merged_name);

  // The merged name should maintain the structure but use the observation of
  // the custom-formatted full name.
  EXPECT_EQ(merged_name.GetRawInfo(NAME_FULL), u"Anderson, Thomas A.");
  EXPECT_EQ(merged_name.GetVerificationStatus(NAME_FULL),
            VerificationStatus::kObserved);
  EXPECT_EQ(merged_name.GetRawInfo(NAME_FIRST), u"Thomas");
  EXPECT_EQ(merged_name.GetVerificationStatus(NAME_FIRST),
            VerificationStatus::kObserved);
  EXPECT_EQ(merged_name.GetRawInfo(NAME_MIDDLE), u"A.");
  EXPECT_EQ(merged_name.GetVerificationStatus(NAME_MIDDLE),
            VerificationStatus::kObserved);
  EXPECT_EQ(merged_name.GetRawInfo(NAME_LAST), u"Anderson");
  EXPECT_EQ(merged_name.GetVerificationStatus(NAME_LAST),
            VerificationStatus::kObserved);
}

TEST_F(AutofillProfileComparatorTest, MergeNames) {
  NameInfo name1;
  name1.SetRawInfo(NAME_FULL, u"John Quincy Public");
  name1.SetRawInfo(NAME_FIRST, u"John");
  name1.SetRawInfo(NAME_MIDDLE, u"Quincy");
  name1.SetRawInfo(NAME_LAST, u"Public");
  name1.FinalizeAfterImport();

  NameInfo name2;
  name2.SetRawInfo(NAME_FULL, u"John Q. Public");
  name2.SetRawInfo(NAME_FIRST, u"John");
  name2.SetRawInfo(NAME_MIDDLE, u"Q.");
  name2.SetRawInfo(NAME_LAST, u"Public");
  name2.FinalizeAfterImport();

  NameInfo name3;
  name3.SetRawInfo(NAME_FULL, u"J Public");
  name3.SetRawInfo(NAME_FIRST, u"J");
  name3.SetRawInfo(NAME_MIDDLE, u"");
  name3.SetRawInfo(NAME_LAST, u"Public");
  name3.FinalizeAfterImport();

  NameInfo name4;
  name4.SetRawInfo(NAME_FULL, u"John Quincy Public");
  name4.FinalizeAfterImport();

  NameInfo name5;
  name5.SetRawInfo(NAME_FIRST, u"John");
  name5.SetRawInfo(NAME_LAST, u"Public");
  name5.FinalizeAfterImport();

  NameInfo synthesized;
  synthesized.SetRawInfo(NAME_FULL, u"John Public");
  synthesized.SetRawInfo(NAME_FIRST, u"John");
  synthesized.SetRawInfo(NAME_MIDDLE, u"");
  synthesized.SetRawInfo(NAME_LAST, u"Public");
  synthesized.FinalizeAfterImport();

  AutofillProfile p1 = CreateProfileWithName(name1);
  AutofillProfile p2 = CreateProfileWithName(name2);
  AutofillProfile p3 = CreateProfileWithName(name3);
  AutofillProfile p4 = CreateProfileWithName(name4);
  AutofillProfile p5 = CreateProfileWithName(name5);

  MergeNamesAndExpect(p1, p1, name1);
  MergeNamesAndExpect(p1, p2, name1);
  MergeNamesAndExpect(p1, p3, name1);
  MergeNamesAndExpect(p1, p4, name1);
  MergeNamesAndExpect(p1, p5, name1);

  MergeNamesAndExpect(p2, p1, name1);
  MergeNamesAndExpect(p2, p2, name2);
  MergeNamesAndExpect(p2, p3, name2);
  MergeNamesAndExpect(p2, p4, name1);
  MergeNamesAndExpect(p2, p5, name2);

  MergeNamesAndExpect(p3, p1, name1);
  MergeNamesAndExpect(p3, p2, name2);
  MergeNamesAndExpect(p3, p3, name3);
  MergeNamesAndExpect(p3, p4, name1);
  MergeNamesAndExpect(p3, p5, synthesized);

  // P4 can be teased apart and reconstituted as name1.
  MergeNamesAndExpect(p4, p1, name1);
  MergeNamesAndExpect(p4, p2, name1);
  MergeNamesAndExpect(p4, p3, name1);
  MergeNamesAndExpect(p4, p4, name1);
  MergeNamesAndExpect(p4, p5, name1);

  // P5 expands the first name if it's not complete.
  MergeNamesAndExpect(p5, p1, name1);
  MergeNamesAndExpect(p5, p2, name2);
  MergeNamesAndExpect(p5, p3, synthesized);
  MergeNamesAndExpect(p5, p4, name1);
  MergeNamesAndExpect(p5, p5, synthesized);  // We flesh out missing data.
}

TEST_F(AutofillProfileComparatorTest, MergeCJKNames) {
  // Korean names that are all mergeable, but constructed differently.
  NameInfo name1 = CreateNameInfo(u"호", u"", u"이영", u"이영 호");
  NameInfo name2 = CreateNameInfo(u"이영호", u"", u"", u"이영호");
  NameInfo name3 = CreateNameInfo(u"영호", u"", u"이", u"이영호");
  NameInfo name4 = CreateNameInfo(u"영호", u"", u"이", u"");
  NameInfo name5 = CreateNameInfo(u"영호", u"", u"이", u"이 영호");

  // Mergeable foreign name in Japanese with a 'KATAKANA MIDDLE DOT'.
  NameInfo name6 = CreateNameInfo(u"", u"", u"", u"ゲイツ・ビル");
  NameInfo name7 = CreateNameInfo(u"ビル", u"", u"ゲイツ", u"");

  // Set the use dates for the profiles, because |MergeCJKNames()| tries to use
  // the most recent profile if there is a conflict. The ordering is
  // p1 > p2 > p3 > p4 > p5, with p1 being the most recent.
  AutofillProfile p1 = CreateProfileWithName(name1);
  p1.set_use_date(AutofillClock::Now());
  AutofillProfile p2 = CreateProfileWithName(name2);
  p2.set_use_date(AutofillClock::Now() - base::Hours(1));
  AutofillProfile p3 = CreateProfileWithName(name3);
  p3.set_use_date(AutofillClock::Now() - base::Hours(2));
  AutofillProfile p4 = CreateProfileWithName(name4);
  p4.set_use_date(AutofillClock::Now() - base::Hours(3));
  AutofillProfile p5 = CreateProfileWithName(name5);
  p5.set_use_date(AutofillClock::Now() - base::Hours(4));

  AutofillProfile p6 = CreateProfileWithName(name6);
  AutofillProfile p7 = CreateProfileWithName(name7);

  // Because |p1| is the most recent, it always wins over others.
  MergeNamesAndExpect(p1, p2, CreateNameInfo(u"호", u"", u"이영", u"이영 호"));
  MergeNamesAndExpect(p1, p3, CreateNameInfo(u"호", u"", u"이영", u"이영 호"));
  MergeNamesAndExpect(p1, p4, CreateNameInfo(u"호", u"", u"이영", u"이영 호"));
  MergeNamesAndExpect(p1, p5, CreateNameInfo(u"호", u"", u"이영", u"이영 호"));

  // |p3| is more recent than |p4| and |p5|.
  MergeNamesAndExpect(p3, p4, CreateNameInfo(u"영호", u"", u"이", u"이영호"));
  MergeNamesAndExpect(p3, p5, CreateNameInfo(u"영호", u"", u"이", u"이영호"));

  // |p4| is more recent than |p5|. However, it does not have an explicit
  // full name, so use the one from |p5|.
  MergeNamesAndExpect(p4, p5, CreateNameInfo(u"영호", u"", u"이", u"이 영호"));

  // There is no conflict between |p6| and |p7|, so use the parts from both.
  MergeNamesAndExpect(p6, p7,
                      CreateNameInfo(u"ビル", u"", u"ゲイツ", u"ゲイツ・ビル"));
}

TEST_F(AutofillProfileComparatorTest, MergeEmailAddresses) {
  static const char kEmailA[] = "testaccount@domain.net";
  static const char16_t kEmailA16[] = u"testaccount@domain.net";
  static const char kEmailB[] = "TestAccount@Domain.Net";
  static const char16_t kEmailB16[] = u"TestAccount@Domain.Net";

  EmailInfo email_a;
  email_a.SetRawInfo(EMAIL_ADDRESS, kEmailA16);
  AutofillProfile profile_a = CreateProfileWithEmail(kEmailA);
  profile_a.set_use_date(AutofillClock::Now());

  EmailInfo email_b;
  email_b.SetRawInfo(EMAIL_ADDRESS, kEmailB16);
  AutofillProfile profile_b = CreateProfileWithEmail(kEmailB);
  profile_b.set_use_date(profile_a.use_date() + base::Days(1));

  MergeEmailAddressesAndExpect(profile_a, profile_a, email_a);
  MergeEmailAddressesAndExpect(profile_b, profile_b, email_b);
  MergeEmailAddressesAndExpect(profile_a, profile_b, email_b);
  MergeEmailAddressesAndExpect(profile_b, profile_a, email_b);
}

TEST_F(AutofillProfileComparatorTest, MergeCompanyNames) {
  static const char kCompanyA[] = "Some Company";
  static const char16_t kCompanyA16[] = u"Some Company";
  static const char kCompanyB[] = "SÔMÈ ÇÖMPÁÑÝ";
  static const char16_t kCompanyB16[] = u"SÔMÈ ÇÖMPÁÑÝ";
  static const char kCompanyC[] = "SÔMÈ ÇÖMPÁÑÝ A.G.";
  static const char16_t kCompanyC16[] = u"SÔMÈ ÇÖMPÁÑÝ A.G.";

  CompanyInfo company_a;
  company_a.SetRawInfo(COMPANY_NAME, kCompanyA16);
  AutofillProfile profile_a = CreateProfileWithCompanyName(kCompanyA);
  profile_a.set_use_date(AutofillClock::Now());

  // Company Name B is post_normalization identical to Company Name A. The use
  // date will be used to choose between them.
  CompanyInfo company_b;
  company_b.SetRawInfo(COMPANY_NAME, kCompanyB16);
  AutofillProfile profile_b = CreateProfileWithCompanyName(kCompanyB);
  profile_b.set_use_date(profile_a.use_date() + base::Days(1));

  // Company Name C is the most complete. Even though it has the earliest use
  // date, it will be preferred to the other two.
  CompanyInfo company_c;
  company_c.SetRawInfo(COMPANY_NAME, kCompanyC16);
  AutofillProfile profile_c = CreateProfileWithCompanyName(kCompanyC);
  profile_c.set_use_date(profile_a.use_date() - base::Days(1));

  MergeCompanyNamesAndExpect(profile_a, profile_a, company_a);
  MergeCompanyNamesAndExpect(profile_a, profile_b, company_b);
  MergeCompanyNamesAndExpect(profile_a, profile_c, company_c);

  MergeCompanyNamesAndExpect(profile_b, profile_a, company_b);
  MergeCompanyNamesAndExpect(profile_b, profile_b, company_b);
  MergeCompanyNamesAndExpect(profile_b, profile_c, company_c);

  MergeCompanyNamesAndExpect(profile_c, profile_a, company_c);
  MergeCompanyNamesAndExpect(profile_c, profile_b, company_c);
  MergeCompanyNamesAndExpect(profile_c, profile_c, company_c);
}

TEST_F(AutofillProfileComparatorTest, MergePhoneNumbers_NA) {
  AutofillProfile profile_a = CreateProfileWithPhoneNumber("5550199");
  AutofillProfile profile_b = CreateProfileWithPhoneNumber("555.0199");
  AutofillProfile profile_c = CreateProfileWithPhoneNumber("8005550199");

  // Profile A
  MergePhoneNumbersAndExpect(profile_a, profile_a, u"5550199");
  MergePhoneNumbersAndExpect(profile_a, profile_b, u"555-0199");
  MergePhoneNumbersAndExpect(profile_a, profile_c, u"(800) 555-0199");

  // Profile B
  MergePhoneNumbersAndExpect(profile_b, profile_a, u"555-0199");
  MergePhoneNumbersAndExpect(profile_b, profile_b, u"555.0199");
  MergePhoneNumbersAndExpect(profile_b, profile_c, u"(800) 555-0199");

  // Profile D
  MergePhoneNumbersAndExpect(profile_c, profile_a, u"(800) 555-0199");
  MergePhoneNumbersAndExpect(profile_c, profile_b, u"(800) 555-0199");
  MergePhoneNumbersAndExpect(profile_c, profile_c, u"8005550199");
}

TEST_F(AutofillProfileComparatorTest, MergePhoneNumbers_Intl) {
  AutofillProfile profile_a = CreateProfileWithPhoneNumber("+49492180185611");
  AutofillProfile profile_b =
      CreateProfileWithPhoneNumber("+49 4921 801 856-11");
  AutofillProfile profile_c = CreateProfileWithPhoneNumber("04921 80185611");

  profile_a.SetInfo(ADDRESS_HOME_COUNTRY, u"DE", kLocale);
  profile_b.SetInfo(ADDRESS_HOME_COUNTRY, u"DE", kLocale);
  profile_c.SetInfo(ADDRESS_HOME_COUNTRY, u"DE", kLocale);

  // Profile A
  MergePhoneNumbersAndExpect(profile_a, profile_a, u"+49492180185611");
  MergePhoneNumbersAndExpect(profile_a, profile_b, u"+49 4921 80185611");
  MergePhoneNumbersAndExpect(profile_a, profile_c, u"+49 4921 80185611");

  // Profile B
  MergePhoneNumbersAndExpect(profile_b, profile_a, u"+49 4921 80185611");
  MergePhoneNumbersAndExpect(profile_b, profile_b, u"+49 4921 801 856-11");
  MergePhoneNumbersAndExpect(profile_b, profile_c, u"+49 4921 80185611");

  // Profile D
  MergePhoneNumbersAndExpect(profile_c, profile_a, u"+49 4921 80185611");
  MergePhoneNumbersAndExpect(profile_c, profile_b, u"+49 4921 80185611");
  MergePhoneNumbersAndExpect(profile_c, profile_c, u"04921 80185611");
}

TEST_F(AutofillProfileComparatorTest, MergeAddresses) {
  AutofillProfile p1 = CreateProfileWithAddress(
      "1 Some Street", "Unit 3", "Carver", "CA - California", "90210", "US");
  AutofillProfile p2 = CreateProfileWithAddress(
      "1 Some Street #3", "", "Carver City", "ca", "90210-1234", "US");

  Address expected(kLegacyHierarchyCountryCode);
  expected.SetRawInfo(ADDRESS_HOME_LINE1, u"1 Some Street");
  expected.SetRawInfo(ADDRESS_HOME_LINE2, u"Unit 3");
  expected.SetRawInfo(ADDRESS_HOME_CITY, u"Carver City");
  expected.SetRawInfo(ADDRESS_HOME_STATE, u"ca");
  expected.SetRawInfo(ADDRESS_HOME_ZIP, u"90210-1234");
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");

  expected.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"1");
  expected.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"Some Street");

  MergeAddressesAndExpect(p1, p2, expected,
                          /*check_structured_address_tokens=*/false);
}

TEST_F(AutofillProfileComparatorTest, MergeAddressesMostUniqueTokens) {
  AutofillProfile p1 = CreateProfileWithAddress(
      "1 Some Street", "Unit 3", "Carver", "CA - California", "90210", "US");

  p1.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"Some Street");
  p1.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"");
  p1.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"Unit 3");

  AutofillProfile p2 = CreateProfileWithAddress(
      "1 Some Other Street", "Unit 3", "Carver City", "ca", "90210-1234", "US");

  p2.set_use_date(p1.use_date() + base::Minutes(1));
  p2.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"Some Other Street");
  p2.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"HouseNumber2");
  p2.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"Subpremise2");

  Address expected(kLegacyHierarchyCountryCode);
  expected.SetRawInfo(ADDRESS_HOME_LINE1, u"1 Some Other Street");
  expected.SetRawInfo(ADDRESS_HOME_LINE2, u"Unit 3");
  expected.SetRawInfo(ADDRESS_HOME_CITY, u"Carver City");
  expected.SetRawInfo(ADDRESS_HOME_STATE, u"ca");
  expected.SetRawInfo(ADDRESS_HOME_ZIP, u"90210-1234");
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");

  MergeAddressesAndExpect(p1, p2, expected);
  MergeAddressesAndExpect(p2, p1, expected);
}

TEST_F(AutofillProfileComparatorTest, MergeAddressesWithStructure) {
  AutofillProfile p1 = CreateProfileWithAddress(
      "6543 CH BACON", "APP 3", "MONTRÉAL", "QUÉBEC", "HHH999", "ca");

  p1.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"StreetName");
  p1.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"HouseNumber");
  p1.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"Subpremise");

  AutofillProfile p2 = CreateProfileWithAddress(
      "6543, Bacon Rd", "", "Montreal", "QC", "hhh 999", "CA");
  p2.set_use_date(p1.use_date() + base::Minutes(1));
  p2.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"StreetName2");
  p2.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"HouseNumber2");
  p2.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"Subpremise2");

  Address expected(kLegacyHierarchyCountryCode);
  expected.SetRawInfo(ADDRESS_HOME_LINE1, u"6543 CH BACON");
  expected.SetRawInfo(ADDRESS_HOME_LINE2, u"APP 3");
  expected.SetRawInfo(ADDRESS_HOME_CITY, u"Montreal");
  expected.SetRawInfo(ADDRESS_HOME_STATE, u"QC");
  expected.SetRawInfo(ADDRESS_HOME_ZIP, u"hhh 999");
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, u"CA");

  MergeAddressesAndExpect(p1, p2, expected);
  MergeAddressesAndExpect(p2, p1, expected);
}

TEST_F(AutofillProfileComparatorTest, MergeAddressesWithRewrite) {
  AutofillProfile p1 = CreateProfileWithAddress(
      "6543 CH BACON", "APP 3", "MONTRÉAL", "QUÉBEC", "HHH999", "ca");

  p1.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"StreetName");
  p1.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"HouseNumber");
  p1.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"Subpremise");

  AutofillProfile p2 = CreateProfileWithAddress(
      "6543, Bacon Rd", "", "Montreal", "QC", "hhh 999", "CA");
  p2.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"StreetName2");
  p2.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"HouseNumber2");
  p2.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"Subpremise2");

  p2.set_use_date(p1.use_date() + base::Minutes(1));

  Address expected(kLegacyHierarchyCountryCode);
  expected.SetRawInfo(ADDRESS_HOME_LINE1, u"6543 CH BACON");
  expected.SetRawInfo(ADDRESS_HOME_LINE2, u"APP 3");
  expected.SetRawInfo(ADDRESS_HOME_CITY, u"Montreal");
  expected.SetRawInfo(ADDRESS_HOME_STATE, u"QC");
  expected.SetRawInfo(ADDRESS_HOME_ZIP, u"hhh 999");
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, u"CA");

  MergeAddressesAndExpect(p1, p2, expected);
  MergeAddressesAndExpect(p2, p1, expected);
}

// The intention of this test is to verify that "Straße" and "Str."" ar
// considered equivalent, which requires a normalization of ß to ss followed by
// a rewrite of "strasse" to "str".
TEST_F(AutofillProfileComparatorTest, MergeAddressesWithRewriteDE) {
  AutofillProfile p1 = CreateProfileWithAddress("Erika-Mann-Straße 33", "APP 3",
                                                "München", "", "80636", "DE");

  p1.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"Erika-Mann-Straße");
  p1.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"33");
  p1.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"APP 3");

  AutofillProfile p2 = CreateProfileWithAddress("Erika-Mann-Str. 33", "",
                                                "Munchen", "", "80636", "DE");
  p2.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"Erika-Mann-Str");
  p2.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"33");

  p2.set_use_date(p1.use_date() + base::Minutes(1));

  Address expected(kLegacyHierarchyCountryCode);
  // The longer string wins.
  expected.SetRawInfo(ADDRESS_HOME_LINE1, u"Erika-Mann-Straße 33");
  // Extra information.
  expected.SetRawInfo(ADDRESS_HOME_LINE2, u"APP 3");
  // The most recent string wins.
  expected.SetRawInfo(ADDRESS_HOME_CITY, u"Munchen");
  expected.SetRawInfo(ADDRESS_HOME_STATE, u"");
  expected.SetRawInfo(ADDRESS_HOME_ZIP, u"80636");
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, u"DE");

  MergeAddressesAndExpect(p1, p2, expected);
  MergeAddressesAndExpect(p2, p1, expected);
}

TEST_F(AutofillProfileComparatorTest,
       MergeAddressesDependentLocalityAndSortingCode) {
  AutofillProfile p1 = CreateProfileWithAddress(
      "6543 CH BACON", "APP 3", "MONTRÉAL", "QUÉBEC", "HHH999", "ca");
  p1.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Some String");
  p1.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"64205 Biarritz CEDEX");
  AutofillProfile p2 = CreateProfileWithAddress(
      "6543, Bacon Rd", "", "Montreal", "QC", "hhh 999", "CA");
  p2.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Some Other String");
  p2.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"64205 Biarritz");
  p2.set_use_date(p1.use_date() + base::Minutes(1));

  Address expected(kLegacyHierarchyCountryCode);
  expected.SetRawInfo(ADDRESS_HOME_LINE1, u"6543 CH BACON");
  expected.SetRawInfo(ADDRESS_HOME_LINE2, u"APP 3");
  expected.SetRawInfo(ADDRESS_HOME_CITY, u"Montreal");
  expected.SetRawInfo(ADDRESS_HOME_STATE, u"QC");
  expected.SetRawInfo(ADDRESS_HOME_ZIP, u"hhh 999");
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, u"CA");
  expected.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Some Other String");
  expected.SetRawInfo(ADDRESS_HOME_SORTING_CODE,
                      u"64205 Biarritz");  // Preferred by use date.

  MergeAddressesAndExpect(p1, p2, expected);
  MergeAddressesAndExpect(p2, p1, expected);
}

TEST_F(AutofillProfileComparatorTest, MergeLandmarkAndBetweenStreetsAndAdmin2) {
  AutofillProfile empty(kLegacyHierarchyCountryCode);
  AutofillProfile profile2(kLegacyHierarchyCountryCode);
  profile2.SetRawInfo(ADDRESS_HOME_LANDMARK, u"Landmark example");
  profile2.SetRawInfo(ADDRESS_HOME_BETWEEN_STREETS, u"Between streets example");
  profile2.SetRawInfo(ADDRESS_HOME_ADMIN_LEVEL2, u"Admin level 2 example");

  Address expected(kLegacyHierarchyCountryCode);
  expected.SetRawInfo(ADDRESS_HOME_LANDMARK, u"Landmark example");
  expected.SetRawInfo(ADDRESS_HOME_BETWEEN_STREETS, u"Between streets example");
  expected.SetRawInfo(ADDRESS_HOME_ADMIN_LEVEL2, u"Admin level 2 example");

  MergeAddressesAndExpect(empty, profile2, expected);
}

// Checks for various scenarios for determining mergability of profiles w.r.t.
// the state.
TEST_F(AutofillProfileComparatorTest, CheckStatesMergability) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting(
      "DE", "RandomState",
      {{.canonical_name = "RandomState",
        .abbreviations = {"RS"},
        .alternative_names = {"AlternateRandomState"}}});

  AutofillProfile empty = CreateProfileWithAddress("", "", "", "", "", "DE");
  AutofillProfile p1 =
      CreateProfileWithAddress("", "", "", "RandomState", "", "DE");
  AutofillProfile p2 =
      CreateProfileWithAddress("", "", "", "AnotherOne", "", "DE");
  AutofillProfile p3 = CreateProfileWithAddress(
      "", "", "", "RandomState - RS - AlternateRandomState", "", "DE");
  AutofillProfile p4 =
      CreateProfileWithAddress("", "", "", "AlternateRandomState", "", "DE");

  EXPECT_TRUE(comparator_.HaveMergeableAddresses(empty, empty));
  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p1, empty));
  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p1, p1));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p1, p2));
  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p3, p1));
  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p1, p4));
  EXPECT_FALSE(comparator_.HaveMergeableAddresses(p2, p4));
}

// Tests if determining if two profiles have at least one different settings
// visible value works.
TEST_F(AutofillProfileComparatorTest,
       ProfilesHaveDifferentSettingsVisibleValues) {
  AutofillProfile existing_profile(AddressCountryCode("US"));
  test::SetProfileInfo(&existing_profile, "firstName", "middleName", "lastName",
                       "mail@mail.com", "company", "line1", "line2", "city",
                       "state", "zip", "US", "phone");


  // A profile compared with itself cannot have different settings visible
  // values.
  EXPECT_FALSE(
      AutofillProfileComparator::ProfilesHaveDifferentSettingsVisibleValues(
          existing_profile, existing_profile, kLocale));

  // Test for most settings visible types that a change is correctly recognized.
  for (FieldType changed_type :
       {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_CITY,
        ADDRESS_HOME_ZIP, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER}) {
    // Make a fresh copy and test that the function returns false.
    AutofillProfile new_profile = existing_profile;
    EXPECT_FALSE(
        AutofillProfileComparator::ProfilesHaveDifferentSettingsVisibleValues(
            existing_profile, new_profile, kLocale));

    // Change one of the settings visible values and test that the function
    // returns true.
    SCOPED_TRACE(changed_type);
    new_profile.SetRawInfo(
        changed_type, existing_profile.GetRawInfo(changed_type) + u"_edited");
    EXPECT_TRUE(
        AutofillProfileComparator::ProfilesHaveDifferentSettingsVisibleValues(
            existing_profile, new_profile, kLocale));
  }

  AutofillProfile new_profile = existing_profile;
  // Now change the first name which is not visible in the settings to upper
  // case. Note, the value was converted to upper case to maintain the name
  // structure in a correct state.
  new_profile.SetRawInfo(
      NAME_FIRST, base::ToUpperASCII(existing_profile.GetRawInfo(NAME_FIRST)));
  EXPECT_FALSE(
      AutofillProfileComparator::ProfilesHaveDifferentSettingsVisibleValues(
          existing_profile, new_profile, kLocale));
}

TEST_F(AutofillProfileComparatorTest, GetProfileDifference) {
  AutofillProfile existing_profile(AddressCountryCode("US"));
  test::SetProfileInfo(&existing_profile, "firstName", "middleName", "lastName",
                       "mail@mail.com", "company", "line1", "line2", "city",
                       "state", "zip", "US", "phone");

  // Change the zip code of the second profile.
  AutofillProfile second_existing_profile = existing_profile;
  second_existing_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"another_zip");

  // There should be no difference in NAME_FULL type.
  EXPECT_TRUE(
      AutofillProfileComparator::GetProfileDifference(
          existing_profile, second_existing_profile, {NAME_FULL}, kLocale)
          .empty());

  // But there should be difference in ADDRESS_HOME_ZIP type.
  std::vector<ProfileValueDifference> expected_difference = {
      {ADDRESS_HOME_ZIP, u"zip", u"another_zip"}};

  EXPECT_EQ(AutofillProfileComparator::GetProfileDifference(
                existing_profile, second_existing_profile, {ADDRESS_HOME_ZIP},
                kLocale),
            expected_difference);
}

TEST_F(AutofillProfileComparatorTest, GetProfileDifferenceMap) {
  AutofillProfile existing_profile(AddressCountryCode("US"));
  test::SetProfileInfo(&existing_profile, "firstName", "middleName", "lastName",
                       "mail@mail.com", "company", "line1", "line2", "city",
                       "state", "zip", "US", "phone");

  // Change the zip code of the second profile.
  AutofillProfile second_existing_profile = existing_profile;
  second_existing_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"another_zip");

  // There should be no difference in NAME_FULL type.
  EXPECT_TRUE(
      AutofillProfileComparator::GetProfileDifferenceMap(
          existing_profile, second_existing_profile, {NAME_FULL}, kLocale)
          .empty());

  // But there should be difference in ADDRESS_HOME_ZIP type.
  base::flat_map<FieldType, std::pair<std::u16string, std::u16string>>
      expected_difference;
  expected_difference.insert({ADDRESS_HOME_ZIP, {u"zip", u"another_zip"}});

  EXPECT_EQ(AutofillProfileComparator::GetProfileDifferenceMap(
                existing_profile, second_existing_profile, {ADDRESS_HOME_ZIP},
                kLocale),
            expected_difference);
}

TEST_F(AutofillProfileComparatorTest, GetSettingsVisibleProfileDifference) {
  AutofillProfile existing_profile(AddressCountryCode("US"));
  test::SetProfileInfo(&existing_profile, "firstName", "middleName", "lastName",
                       "mail@mail.com", "company", "line1", "line2", "city",
                       "state", "zip", "US", "phone");

  // Make a copy of the existing profile.
  AutofillProfile second_existing_profile = existing_profile;

  // There should be no difference in the profiles.
  EXPECT_TRUE(AutofillProfileComparator::GetSettingsVisibleProfileDifference(
                  existing_profile, second_existing_profile, kLocale)
                  .empty());

  // Change the zip code of the second profile and test the difference.
  second_existing_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"another_zip");
  std::vector<ProfileValueDifference> expected_difference = {
      {ADDRESS_HOME_ZIP, u"zip", u"another_zip"}};
  EXPECT_EQ(AutofillProfileComparator::GetSettingsVisibleProfileDifference(
                existing_profile, second_existing_profile, kLocale),
            expected_difference);

  // Change a second value and check the expectations.
  second_existing_profile.SetRawInfo(ADDRESS_HOME_CITY, u"another_city");
  expected_difference.emplace(
      expected_difference.begin(),
      ProfileValueDifference{ADDRESS_HOME_CITY, u"city", u"another_city"});
  EXPECT_EQ(AutofillProfileComparator::GetSettingsVisibleProfileDifference(
                existing_profile, second_existing_profile, kLocale),
            expected_difference);
}

TEST_F(AutofillProfileComparatorTest, GetSettingsVisibleProfileDifferenceMap) {
  AutofillProfile existing_profile(AddressCountryCode("US"));
  test::SetProfileInfo(&existing_profile, "firstName", "middleName", "lastName",
                       "mail@mail.com", "company", "line1", "line2", "city",
                       "state", "zip", "US", "phone");

  // Make a copy of the existing profile.
  AutofillProfile second_existing_profile = existing_profile;

  // There should be no difference in the profiles.
  EXPECT_TRUE(AutofillProfileComparator::GetSettingsVisibleProfileDifferenceMap(
                  existing_profile, second_existing_profile, kLocale)
                  .empty());

  // Change the zip code of the second profile and test the difference.
  second_existing_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"another_zip");
  base::flat_map<FieldType, std::pair<std::u16string, std::u16string>>
      expected_difference;
  expected_difference.insert({ADDRESS_HOME_ZIP, {u"zip", u"another_zip"}});
  EXPECT_EQ(AutofillProfileComparator::GetSettingsVisibleProfileDifferenceMap(
                existing_profile, second_existing_profile, kLocale),
            expected_difference);

  // Change a second value and check the expectations.
  second_existing_profile.SetRawInfo(ADDRESS_HOME_CITY, u"another_city");
  expected_difference.insert({ADDRESS_HOME_CITY, {u"city", u"another_city"}});
  EXPECT_EQ(AutofillProfileComparator::GetSettingsVisibleProfileDifferenceMap(
                existing_profile, second_existing_profile, kLocale),
            expected_difference);
}

TEST_F(AutofillProfileComparatorTest, IsMergeCandidate) {
  AutofillProfile existing_profile(AddressCountryCode("US"));
  test::SetProfileInfo(&existing_profile, "firstName", "middleName", "lastName",
                       "mail@mail.com", "company", "line1", "line2", "the city",
                       "state", "zip", "US", "phone");

  AutofillProfileComparator comparator("en_US");

  // A profile is not a merge candidate to itself.
  EXPECT_FALSE(
      comparator.IsMergeCandidate(existing_profile, existing_profile, "en_US"));

  // A profile that is mergeable but only by changing a value is a merge
  // candidate.
  AutofillProfile mergeable_profile = existing_profile;
  // This is a superset of the existing city name and should result in a merge
  // and change of the stored value.
  mergeable_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_CITY, u"the real City", VerificationStatus::kObserved);
  EXPECT_TRUE(comparator.IsMergeCandidate(existing_profile, mergeable_profile,
                                          "en_US"));

  // A profile that is mergeable but without changing a value is not a merge
  // candidate.
  AutofillProfile updatable_profile = existing_profile;
  // This is a subset of the existing city name and should result in a merge but
  // without changing the stored value.
  mergeable_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_CITY, u"City", VerificationStatus::kObserved);
  EXPECT_FALSE(comparator.IsMergeCandidate(existing_profile, updatable_profile,
                                           "en_US"));

  // A profile that is not mergeable is not a merge candidate.
  AutofillProfile unmergeable_profile = existing_profile;
  // This is a different city name and therefore should not result in a merge.
  mergeable_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_CITY, u"Village", VerificationStatus::kObserved);
  EXPECT_FALSE(comparator.IsMergeCandidate(existing_profile,
                                           unmergeable_profile, "en_US"));
}

// Tests that the profiles are merged when they have common states.
TEST_F(AutofillProfileComparatorTest, MergeProfilesBasedOnState) {
  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting(
      "IN", "UP",
      {{.canonical_name = "Uttar Pradesh",
        .abbreviations = {"UP"},
        .alternative_names = {}}});

  AutofillProfile empty = CreateProfileWithAddress("", "", "", "", "", "DE");
  AutofillProfile p1 = CreateProfileWithAddress("", "", "", "Bayern", "", "DE");
  AutofillProfile p2 =
      CreateProfileWithAddress("", "", "", "Bayern - BY - Bavaria", "", "DE");

  Address expected(kLegacyHierarchyCountryCode);
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, u"DE");
  expected.SetRawInfo(ADDRESS_HOME_STATE, u"Bayern");
  MergeAddressesAndExpect(empty, p1, expected);
  MergeAddressesAndExpect(p1, empty, expected);
  MergeAddressesAndExpect(p1, p2, expected);
  MergeAddressesAndExpect(p2, p1, expected);

  AutofillProfile p3 =
      CreateProfileWithAddress("", "", "", "Pradesh", "", "IN");
  AutofillProfile p4 =
      CreateProfileWithAddress("", "", "", "Uttar Pradesh", "", "IN");
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, u"IN");
  expected.SetRawInfo(ADDRESS_HOME_STATE, u"Uttar Pradesh");
  MergeAddressesAndExpect(p3, p4, expected);
  MergeAddressesAndExpect(p4, p3, expected);
}

// Tests that when `NonMergeableSettingVisibleTypes()` is called with two
// profiles of the same country, differing in `differing_types`, the function
// returns `expected_setting_visible_difference`.
struct NonMergeableSettingVisibleTypesTestCase {
  const FieldTypeSet differing_types;
  const FieldTypeSet expected_setting_visible_difference;
};

class NonMergeableSettingVisibleTypesTest
    : public AutofillProfileComparatorTest,
      public testing::WithParamInterface<
          NonMergeableSettingVisibleTypesTestCase> {};

TEST_P(NonMergeableSettingVisibleTypesTest, DifferingTypes) {
  const NonMergeableSettingVisibleTypesTestCase& test = GetParam();
  // Construct two profiles differing in exactly `test.differing_types`.
  AutofillProfile a(kLegacyHierarchyCountryCode);
  AutofillProfile b(kLegacyHierarchyCountryCode);
  for (FieldType t : test.differing_types) {
    a.SetRawInfo(t, u"a");
    b.SetRawInfo(t, u"b");
  }
  // Initialize all other setting-visible types with the same value.
  for (FieldType t : GetUserVisibleTypes()) {
    // If a type of the same `FieldTypeGroup` was already set, ignore it, to
    // avoid constructing conflicting substructures.
    if (!base::Contains(test.differing_types, GroupTypeOfFieldType(t),
                        &GroupTypeOfFieldType)) {
      a.SetRawInfo(t, u"same");
      b.SetRawInfo(t, u"same");
    }
  }
  ASSERT_TRUE(a.FinalizeAfterImport());
  ASSERT_TRUE(b.FinalizeAfterImport());
  EXPECT_THAT(comparator_.NonMergeableSettingVisibleTypes(a, b),
              testing::Optional(test.expected_setting_visible_difference));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillProfileComparatorTest,
    NonMergeableSettingVisibleTypesTest,
    testing::Values(
        // No differences:
        NonMergeableSettingVisibleTypesTestCase{{}, {}},
        // Differences in a single non-country type:
        NonMergeableSettingVisibleTypesTestCase{{NAME_FULL}, {NAME_FULL}},
        NonMergeableSettingVisibleTypesTestCase{{NAME_FIRST}, {NAME_FULL}},
        NonMergeableSettingVisibleTypesTestCase{{NAME_LAST}, {NAME_FULL}},
        NonMergeableSettingVisibleTypesTestCase{{COMPANY_NAME}, {COMPANY_NAME}},
        NonMergeableSettingVisibleTypesTestCase{{ADDRESS_HOME_STREET_ADDRESS},
                                                {ADDRESS_HOME_STREET_ADDRESS}},
        NonMergeableSettingVisibleTypesTestCase{{ADDRESS_HOME_LINE1},
                                                {ADDRESS_HOME_STREET_ADDRESS}},
        NonMergeableSettingVisibleTypesTestCase{{ADDRESS_HOME_LINE2},
                                                {ADDRESS_HOME_STREET_ADDRESS}},
        NonMergeableSettingVisibleTypesTestCase{{ADDRESS_HOME_CITY},
                                                {ADDRESS_HOME_CITY}},
        NonMergeableSettingVisibleTypesTestCase{{ADDRESS_HOME_ZIP},
                                                {ADDRESS_HOME_ZIP}},
        NonMergeableSettingVisibleTypesTestCase{{ADDRESS_HOME_STATE},
                                                {ADDRESS_HOME_STATE}},
        NonMergeableSettingVisibleTypesTestCase{{PHONE_HOME_WHOLE_NUMBER},
                                                {PHONE_HOME_WHOLE_NUMBER}},
        NonMergeableSettingVisibleTypesTestCase{{EMAIL_ADDRESS},
                                                {EMAIL_ADDRESS}},
        // Differences in multiple types:
        NonMergeableSettingVisibleTypesTestCase{
            {NAME_FIRST, ADDRESS_HOME_LINE1, ADDRESS_HOME_STATE, EMAIL_ADDRESS},
            {NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_STATE,
             EMAIL_ADDRESS}}));

// Test that types with mergeable values are not returned by
// `NonMergeableSettingVisibleTypes()`.
TEST_F(AutofillProfileComparatorTest,
       NonMergeableSettingVisibleTypes_Mergeability) {
  AutofillProfile a = test::GetFullProfile();
  AutofillProfile b = a;
  // Tests that rewriting rules are applied.
  a.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"123 Str.");
  b.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"123 Street");
  EXPECT_THAT(comparator_.NonMergeableSettingVisibleTypes(a, b),
              testing::Optional(testing::IsEmpty()));
  // Tests that empty values are always mergeable.
  a.ClearFields({ADDRESS_HOME_STREET_ADDRESS});
  EXPECT_THAT(comparator_.NonMergeableSettingVisibleTypes(a, b),
              testing::Optional(testing::IsEmpty()));
}

// Tests that `NonMergeableSettingVisibleTypes()` is nullopt for profiles of
// differing countries.
TEST_F(AutofillProfileComparatorTest,
       NonMergeableSettingVisibleTypes_DifferentCountry) {
  EXPECT_EQ(comparator_.NonMergeableSettingVisibleTypes(
                test::GetFullProfile(), test::GetFullCanadianProfile()),
            std::nullopt);
}

}  // namespace

}  // namespace autofill
