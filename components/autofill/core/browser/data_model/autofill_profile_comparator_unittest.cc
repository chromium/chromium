// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/contact_info.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

// Field Type Constants
using autofill::ADDRESS_HOME_CITY;
using autofill::ADDRESS_HOME_COUNTRY;
using autofill::ADDRESS_HOME_DEPENDENT_LOCALITY;
using autofill::ADDRESS_HOME_LINE1;
using autofill::ADDRESS_HOME_LINE2;
using autofill::ADDRESS_HOME_LINE3;
using autofill::ADDRESS_HOME_SORTING_CODE;
using autofill::ADDRESS_HOME_STATE;
using autofill::ADDRESS_HOME_STREET_ADDRESS;
using autofill::ADDRESS_HOME_ZIP;
using autofill::COMPANY_NAME;
using autofill::EMAIL_ADDRESS;
using autofill::NAME_FIRST;
using autofill::NAME_FULL;
using autofill::NAME_LAST;
using autofill::NAME_MIDDLE;
using autofill::PHONE_HOME_CITY_AND_NUMBER;
using autofill::PHONE_HOME_CITY_CODE;
using autofill::PHONE_HOME_COUNTRY_CODE;
using autofill::PHONE_HOME_EXTENSION;
using autofill::PHONE_HOME_NUMBER;
using autofill::PHONE_HOME_WHOLE_NUMBER;

// Classes, Functions, and other Symbols
using autofill::Address;
using autofill::AutofillClock;
using autofill::AutofillProfile;
using autofill::AutofillType;
using autofill::CompanyInfo;
using autofill::EmailInfo;
using autofill::NameInfo;
using autofill::PhoneNumber;
using autofill::ServerFieldType;
using base::UTF8ToUTF16;

namespace {

const char kLocale[] = "en-US";

class AutofillProfileComparatorTest : public ::testing::Test {
 public:
  // Expose the protected methods of autofill::AutofillProfileComparator for
  // testing.
  class AutofillProfileComparator
      : public ::autofill::AutofillProfileComparator {
   public:
    typedef ::autofill::AutofillProfileComparator Super;
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

  AutofillProfileComparatorTest() {
    autofill::CountryNames::SetLocaleString(kLocale);
  }

  NameInfo CreateNameInfo(const char* first,
                          const char* middle,
                          const char* last,
                          const char* full) {
    NameInfo name;
    name.SetRawInfo(NAME_FIRST, base::UTF8ToUTF16(first));
    name.SetRawInfo(NAME_MIDDLE, base::UTF8ToUTF16(middle));
    name.SetRawInfo(NAME_LAST, base::UTF8ToUTF16(last));
    name.SetRawInfo(NAME_FULL, base::UTF8ToUTF16(full));
    return name;
  }

  AutofillProfile CreateProfileWithName(const char* first,
                                        const char* middle,
                                        const char* last) {
    AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
    autofill::test::SetProfileInfo(&profile, first, middle, last, "", "", "",
                                   "", "", "", "", "", "");
    return profile;
  }

  AutofillProfile CreateProfileWithName(const NameInfo& name) {
    AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
    profile.SetRawInfo(NAME_FULL, name.GetRawInfo(NAME_FULL));
    profile.SetRawInfo(NAME_FIRST, name.GetRawInfo(NAME_FIRST));
    profile.SetRawInfo(NAME_MIDDLE, name.GetRawInfo(NAME_MIDDLE));
    profile.SetRawInfo(NAME_LAST, name.GetRawInfo(NAME_LAST));
    return profile;
  }

  AutofillProfile CreateProfileWithEmail(const char* email) {
    AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
    autofill::test::SetProfileInfo(&profile, "", "", "", email, "", "", "", "",
                                   "", "", "", "");
    return profile;
  }

  AutofillProfile CreateProfileWithCompanyName(const char* company_name) {
    AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
    autofill::test::SetProfileInfo(&profile, "", "", "", "", company_name, "",
                                   "", "", "", "", "", "");
    return profile;
  }

  AutofillProfile CreateProfileWithPhoneNumber(const char* phone_number) {
    AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
    autofill::test::SetProfileInfo(&profile, "", "", "", "", "", "", "", "", "",
                                   "", "", phone_number);
    return profile;
  }

  AutofillProfile CreateProfileWithAddress(const char* line1,
                                           const char* line2,
                                           const char* city,
                                           const char* state,
                                           const char* zip,
                                           const char* country) {
    AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
    autofill::test::SetProfileInfo(&profile, "", "", "", "", "", line1, line2,
                                   city, state, zip, country, "");
    return profile;
  }

  AutofillProfile CopyAndModify(
      const AutofillProfile& profile,
      const std::vector<std::pair<ServerFieldType, const char*>>& updates) {
    AutofillProfile new_profile = profile;
    for (const auto& update : updates) {
      new_profile.SetRawInfo(update.first, UTF8ToUTF16(update.second));
    }
    return new_profile;
  }

  void MergeNamesAndExpect(const AutofillProfile& a,
                           const AutofillProfile& b,
                           const NameInfo& expected) {
    NameInfo actual;
    ASSERT_TRUE(comparator_.MergeNames(a, b, &actual));

    // Is the "processed" data correct?
    EXPECT_EQ(expected.GetInfo(AutofillType(NAME_FULL), kLocale),
              actual.GetInfo(AutofillType(NAME_FULL), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(NAME_FIRST), kLocale),
              actual.GetInfo(AutofillType(NAME_FIRST), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(NAME_MIDDLE), kLocale),
              actual.GetInfo(AutofillType(NAME_MIDDLE), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(NAME_LAST), kLocale),
              actual.GetInfo(AutofillType(NAME_LAST), kLocale));

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
    ASSERT_TRUE(comparator_.MergeEmailAddresses(a, b, &actual));
    EXPECT_EQ(expected.GetRawInfo(EMAIL_ADDRESS),
              actual.GetRawInfo(EMAIL_ADDRESS));
  }

  void MergeCompanyNamesAndExpect(const AutofillProfile& a,
                                  const AutofillProfile& b,
                                  const CompanyInfo& expected) {
    CompanyInfo actual;
    ASSERT_TRUE(comparator_.MergeCompanyNames(a, b, &actual));
    EXPECT_EQ(expected.GetRawInfo(COMPANY_NAME),
              actual.GetRawInfo(COMPANY_NAME));
  }

  void MergePhoneNumbersAndExpect(const AutofillProfile& a,
                                  const AutofillProfile& b,
                                  const std::string& expected_str) {
    AutofillProfile dummy;

    // Merge the phone numbers.
    PhoneNumber actual(&dummy);
    ASSERT_TRUE(comparator_.MergePhoneNumbers(a, b, &actual));

    // Construct the expected value.
    PhoneNumber expected(&dummy);
    expected.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, UTF8ToUTF16(expected_str));

    // Validate that we get what we expect.
    EXPECT_EQ(expected.GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
              actual.GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
    EXPECT_EQ(expected.GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), kLocale),
              actual.GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(PHONE_HOME_COUNTRY_CODE), kLocale),
              actual.GetInfo(AutofillType(PHONE_HOME_COUNTRY_CODE), kLocale));
    EXPECT_EQ(
        expected.GetInfo(AutofillType(PHONE_HOME_CITY_AND_NUMBER), kLocale),
        actual.GetInfo(AutofillType(PHONE_HOME_CITY_AND_NUMBER), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(PHONE_HOME_CITY_CODE), kLocale),
              actual.GetInfo(AutofillType(PHONE_HOME_CITY_CODE), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(PHONE_HOME_NUMBER), kLocale),
              actual.GetInfo(AutofillType(PHONE_HOME_NUMBER), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(PHONE_HOME_EXTENSION), kLocale),
              actual.GetInfo(AutofillType(PHONE_HOME_EXTENSION), kLocale));
  }

  void MergeAddressesAndExpect(const AutofillProfile& a,
                               const AutofillProfile& b,
                               const Address& expected) {
    Address actual;
    ASSERT_TRUE(comparator_.MergeAddresses(a, b, &actual));

    EXPECT_EQ(expected.GetInfo(AutofillType(ADDRESS_HOME_LINE1), kLocale),
              actual.GetInfo(AutofillType(ADDRESS_HOME_LINE1), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(ADDRESS_HOME_LINE2), kLocale),
              actual.GetInfo(AutofillType(ADDRESS_HOME_LINE2), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(ADDRESS_HOME_LINE3), kLocale),
              actual.GetInfo(AutofillType(ADDRESS_HOME_LINE3), kLocale));
    EXPECT_EQ(
        expected.GetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS), kLocale),
        actual.GetInfo(AutofillType(ADDRESS_HOME_STREET_ADDRESS), kLocale));
    EXPECT_EQ(
        expected.GetInfo(AutofillType(ADDRESS_HOME_DEPENDENT_LOCALITY),
                         kLocale),
        actual.GetInfo(AutofillType(ADDRESS_HOME_DEPENDENT_LOCALITY), kLocale));
    EXPECT_EQ(
        expected.GetInfo(AutofillType(ADDRESS_HOME_SORTING_CODE), kLocale),
        actual.GetInfo(AutofillType(ADDRESS_HOME_SORTING_CODE), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(ADDRESS_HOME_CITY), kLocale),
              actual.GetInfo(AutofillType(ADDRESS_HOME_CITY), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(ADDRESS_HOME_STATE), kLocale),
              actual.GetInfo(AutofillType(ADDRESS_HOME_STATE), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(ADDRESS_HOME_ZIP), kLocale),
              actual.GetInfo(AutofillType(ADDRESS_HOME_ZIP), kLocale));
    EXPECT_EQ(expected.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), kLocale),
              actual.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), kLocale));
  }

  AutofillProfileComparator comparator_{kLocale};

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillProfileComparatorTest);
};

}  // namespace

TEST_F(AutofillProfileComparatorTest, UniqueTokens) {
  base::string16 kInput = UTF8ToUTF16("a b a a b");
  std::vector<base::string16> tokens = {UTF8ToUTF16("a"), UTF8ToUTF16("b")};
  EXPECT_EQ(std::set<base::StringPiece16>(tokens.begin(), tokens.end()),
            comparator_.UniqueTokens(kInput));
}

TEST_F(AutofillProfileComparatorTest, CompareTokens) {
  base::string16 kEmptyStr = UTF8ToUTF16("");
  base::string16 kHello = UTF8ToUTF16("hello");
  base::string16 kHelloThere = UTF8ToUTF16("hello there");
  base::string16 kHelloThereAlice = UTF8ToUTF16("hello there alice");
  base::string16 kHelloThereBob = UTF8ToUTF16("hello there bob");

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
      comparator_.Compare(base::string16(), base::string16(),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(base::string16(), base::string16(),
                          AutofillProfileComparator::DISCARD_WHITESPACE));

  // Checks that leading punctuation and white space are ignored.
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16(".,  -()."), UTF8ToUTF16(""),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16(".,  -()."), UTF8ToUTF16(""),
                          AutofillProfileComparator::DISCARD_WHITESPACE));

  // Checks that trailing punctuation and white space are ignored.
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16("a ., "), UTF8ToUTF16("a"),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16("a ., "), UTF8ToUTF16("a"),
                          AutofillProfileComparator::DISCARD_WHITESPACE));

  // Checks that embedded punctuation and white space is collapsed to a single
  // white space with RETAIN_WHITESPACE and is ignored with DISCARD_WHITESPACE.
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16("a() -  a"), UTF8ToUTF16("a a"),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16("a() -  a"), UTF8ToUTF16("aa"),
                          AutofillProfileComparator::DISCARD_WHITESPACE));

  // Checks that characters such as 'œ' respect the status quo established by
  // NormalizeForComparison.
  EXPECT_FALSE(comparator_.Compare(UTF8ToUTF16("œil"), UTF8ToUTF16("oeil")));

  // Checks that a substring of the string is not considered equal.
  EXPECT_FALSE(comparator_.Compare(UTF8ToUTF16("A"), UTF8ToUTF16("Anna")));

  EXPECT_FALSE(comparator_.Compare(UTF8ToUTF16("Anna"), UTF8ToUTF16("A")));

  // Checks that Compare behaves like NormalizeForComparison. Also, checks that
  // diacritics are removed.
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16("Timothé"), UTF8ToUTF16("timothe"),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16(" sven-åke "), UTF8ToUTF16("sven ake"),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16("Ç 㸐"), UTF8ToUTF16("c 㸐"),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16("902103214"), UTF8ToUTF16("90210-3214"),
                          AutofillProfileComparator::DISCARD_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16("Timothé-Noël Étienne Périer"),
                          UTF8ToUTF16("timothe noel etienne perier"),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16("1600 Amphitheatre, Pkwy."),
                          UTF8ToUTF16("1600 amphitheatre pkwy"),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(base::WideToUTF16(L"Mid\x2013Island\x2003 Plaza"),
                          UTF8ToUTF16("mid island plaza"),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16("1600 amphitheatre pkwy \n App. 2"),
                          UTF8ToUTF16("1600 amphitheatre pkwy app 2"),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16("まéÖä정"), UTF8ToUTF16("まeoa정"),
                          AutofillProfileComparator::RETAIN_WHITESPACE));
  EXPECT_TRUE(
      comparator_.Compare(UTF8ToUTF16("유재석"), UTF8ToUTF16("유 재석"),
                          AutofillProfileComparator::DISCARD_WHITESPACE));
  EXPECT_TRUE(comparator_.Compare(
      UTF8ToUTF16("ビルゲイツ"), UTF8ToUTF16("ヒル・ケイツ"),
      AutofillProfileComparator::DISCARD_WHITESPACE));
}

TEST_F(AutofillProfileComparatorTest, NormalizeForComparison) {
  EXPECT_EQ(UTF8ToUTF16("timothe"),
            comparator_.NormalizeForComparison(UTF8ToUTF16("Timothé")));
  EXPECT_EQ(UTF8ToUTF16("sven ake"),
            comparator_.NormalizeForComparison(UTF8ToUTF16(" sven-åke ")));
  EXPECT_EQ(UTF8ToUTF16("c 㸐"),
            comparator_.NormalizeForComparison(UTF8ToUTF16("Ç 㸐")));
  EXPECT_EQ(UTF8ToUTF16("902103214"),
            comparator_.NormalizeForComparison(
                base::UTF8ToUTF16("90210-3214"),
                AutofillProfileComparator::DISCARD_WHITESPACE));
  EXPECT_EQ(UTF8ToUTF16("timothe noel etienne perier"),
            comparator_.NormalizeForComparison(
                UTF8ToUTF16("Timothé-Noël Étienne Périer")));
  // NOP.
  EXPECT_EQ(base::string16(),
            comparator_.NormalizeForComparison(base::string16()));

  // Simple punctuation removed.
  EXPECT_EQ(UTF8ToUTF16("1600 amphitheatre pkwy"),
            comparator_.NormalizeForComparison(
                UTF8ToUTF16("1600 Amphitheatre, Pkwy.")));

  // Unicode punctuation (hyphen and space), multiple spaces collapsed.
  EXPECT_EQ(UTF8ToUTF16("mid island plaza"),
            comparator_.NormalizeForComparison(
                base::WideToUTF16(L"Mid\x2013Island\x2003 Plaza")));

  // Newline character removed.
  EXPECT_EQ(UTF8ToUTF16("1600 amphitheatre pkwy app 2"),
            comparator_.NormalizeForComparison(
                UTF8ToUTF16("1600 amphitheatre pkwy \n App. 2")));

  // Diacritics removed.
  EXPECT_EQ(UTF8ToUTF16("まeoa정"),
            comparator_.NormalizeForComparison(UTF8ToUTF16("まéÖä정")));

  // Spaces removed.
  EXPECT_EQ(UTF8ToUTF16("유재석"),
            comparator_.NormalizeForComparison(
                UTF8ToUTF16("유 재석"),
                AutofillProfileComparator::DISCARD_WHITESPACE));

  // Punctuation removed, Japanese kana normalized.
  EXPECT_EQ(UTF8ToUTF16("ヒルケイツ"),
            comparator_.NormalizeForComparison(
                UTF8ToUTF16("ビル・ゲイツ"),
                AutofillProfileComparator::DISCARD_WHITESPACE));
}

TEST_F(AutofillProfileComparatorTest, GetNamePartVariants) {
  std::set<base::string16> expected_variants = {
      UTF8ToUTF16("timothe noel"),
      UTF8ToUTF16("timothe n"),
      UTF8ToUTF16("timothe"),
      UTF8ToUTF16("t noel"),
      UTF8ToUTF16("t n"),
      UTF8ToUTF16("t"),
      UTF8ToUTF16("noel"),
      UTF8ToUTF16("n"),
      UTF8ToUTF16(""),
      UTF8ToUTF16("tn"),
  };

  EXPECT_EQ(expected_variants,
            comparator_.GetNamePartVariants(UTF8ToUTF16("timothe noel")));
}

TEST_F(AutofillProfileComparatorTest, IsNameVariantOf) {
  const base::string16 kNormalizedFullName =
      UTF8ToUTF16("timothe noel etienne perier");

  EXPECT_TRUE(
      comparator_.IsNameVariantOf(kNormalizedFullName, kNormalizedFullName));
  EXPECT_TRUE(comparator_.IsNameVariantOf(
      kNormalizedFullName, UTF8ToUTF16("t noel etienne perier")));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          UTF8ToUTF16("timothe perier")));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          UTF8ToUTF16("t perier")));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          UTF8ToUTF16("noel perier")));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          UTF8ToUTF16("t n etienne perier")));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          UTF8ToUTF16("tn perier")));
  EXPECT_TRUE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                          UTF8ToUTF16("te perier")));

  EXPECT_FALSE(comparator_.IsNameVariantOf(kNormalizedFullName,
                                           UTF8ToUTF16("etienne noel perier")));
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
  AutofillProfile p2 = CreateProfileWithPhoneNumber("800.670.8700x321");
  AutofillProfile p3 = CreateProfileWithPhoneNumber("670-8700 ext321");
  AutofillProfile p4 = CreateProfileWithPhoneNumber("6708700");
  AutofillProfile different = CreateProfileWithPhoneNumber("1-800-321-4567");

  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p1, p1));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p1, p2));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p1, p3));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p1, p4));

  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p2, p1));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p2, p2));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p2, p3));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p2, p4));

  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p3, p1));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p3, p2));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p3, p3));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p3, p4));

  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p4, p1));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p4, p2));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p4, p3));
  EXPECT_TRUE(comparator_.HaveMergeablePhoneNumbers(p4, p4));

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
  AutofillProfile p1 = CreateProfileWithAddress(
      "1 Some Street", "Unit 3", "Carver", "CA - California", "90210", "US");
  p1.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, UTF8ToUTF16("Some String"));
  p1.SetRawInfo(ADDRESS_HOME_SORTING_CODE, UTF8ToUTF16("64205 Biarritz CEDEX"));

  AutofillProfile p2 = CreateProfileWithAddress(
      "Unit 3", "1 Some Street", "Suburb", "california", "90 210-3214", "");
  AutofillProfile p3 = CreateProfileWithAddress("1 Some Street #3", "",
                                                "Carver City", "ca", "", "us");

  AutofillProfile differentCountry =
      CopyAndModify(p1, {{ADDRESS_HOME_COUNTRY, "CA"}});
  AutofillProfile differentZip =
      CopyAndModify(p1, {{ADDRESS_HOME_ZIP, "32145"}});
  AutofillProfile differentState = CopyAndModify(
      p1, {{ADDRESS_HOME_ZIP, ""}, {ADDRESS_HOME_STATE, "Florida"}});
  AutofillProfile differentCity = CopyAndModify(
      p1, {{ADDRESS_HOME_ZIP, ""}, {ADDRESS_HOME_CITY, "Metropolis"}});
  AutofillProfile differentAddress =
      CopyAndModify(p1, {{ADDRESS_HOME_LINE1, "17 Park Lane"},
                         {ADDRESS_HOME_LINE2, "Suite 150"}});
  AutofillProfile differentLocality =
      CopyAndModify(p1, {{ADDRESS_HOME_DEPENDENT_LOCALITY, "Funky Chicken"}});
  AutofillProfile differentSortingCode =
      CopyAndModify(p1, {{ADDRESS_HOME_SORTING_CODE, "98000 Monaco"}});

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
  AutofillProfile p(base::GenerateGUID(), "https://www.example.com/");
  autofill::test::SetProfileInfo(&p, "Marion", "Mitchell", "Morrison",
                                 "marion@me.xyz", "Fox", "123 Zoo St.",
                                 "Unit 5", "Hollywood", "CA", "91601", "US",
                                 "+1 (234) 567-8910");

  AutofillProfile mergeable =
      CopyAndModify(p, {{NAME_FIRST, "MÁRÍÕÑ"},
                        {NAME_MIDDLE, "M."},
                        {EMAIL_ADDRESS, "MARION@ME.XYZ"},
                        {COMPANY_NAME, "Fox Industries Inc."},
                        {ADDRESS_HOME_LINE1, "123 zoo st. w., #5"},
                        {ADDRESS_HOME_LINE1, ""},
                        {ADDRESS_HOME_STATE, "california"},
                        {PHONE_HOME_WHOLE_NUMBER, "5678910 ext. 77"}});
  AutofillProfile not_mergeable_by_name =
      CopyAndModify(p, {{NAME_FIRST, "Steven"}});
  AutofillProfile not_mergeable_by_email_address =
      CopyAndModify(p, {{EMAIL_ADDRESS, "marion.morrision@me.xyz"}});
  AutofillProfile not_mergeable_by_company_name =
      CopyAndModify(p, {{COMPANY_NAME, "Hound Corp"}});
  AutofillProfile not_mergeable_by_address =
      CopyAndModify(p, {{ADDRESS_HOME_LINE2, "Unit 7"}});
  AutofillProfile not_mergeable_by_phone_number =
      CopyAndModify(p, {{PHONE_HOME_WHOLE_NUMBER, "555-1234"}});

  EXPECT_TRUE(comparator_.AreMergeable(p, p));
  EXPECT_TRUE(comparator_.AreMergeable(p, mergeable));
  EXPECT_FALSE(comparator_.AreMergeable(p, not_mergeable_by_name));
  EXPECT_FALSE(comparator_.AreMergeable(p, not_mergeable_by_email_address));
  EXPECT_FALSE(comparator_.AreMergeable(p, not_mergeable_by_company_name));
  EXPECT_FALSE(comparator_.AreMergeable(p, not_mergeable_by_address));
  EXPECT_FALSE(comparator_.AreMergeable(p, not_mergeable_by_phone_number));
}

TEST_F(AutofillProfileComparatorTest, MergeNames) {
  NameInfo name1;
  name1.SetRawInfo(NAME_FULL, UTF8ToUTF16("John Quincy Public"));
  name1.SetRawInfo(NAME_FIRST, UTF8ToUTF16("John"));
  name1.SetRawInfo(NAME_MIDDLE, UTF8ToUTF16("Quincy"));
  name1.SetRawInfo(NAME_LAST, UTF8ToUTF16("Public"));

  NameInfo name2;
  name2.SetRawInfo(NAME_FULL, UTF8ToUTF16("John Q. Public"));
  name2.SetRawInfo(NAME_FIRST, UTF8ToUTF16("John"));
  name2.SetRawInfo(NAME_MIDDLE, UTF8ToUTF16("Q."));
  name2.SetRawInfo(NAME_LAST, UTF8ToUTF16("Public"));

  NameInfo name3;
  name3.SetRawInfo(NAME_FULL, UTF8ToUTF16("J Public"));
  name3.SetRawInfo(NAME_FIRST, UTF8ToUTF16("J"));
  name3.SetRawInfo(NAME_MIDDLE, UTF8ToUTF16(""));
  name3.SetRawInfo(NAME_LAST, UTF8ToUTF16("Public"));

  NameInfo name4;
  name4.SetRawInfo(NAME_FULL, UTF8ToUTF16("John Quincy Public"));

  NameInfo name5;
  name5.SetRawInfo(NAME_FIRST, UTF8ToUTF16("John"));
  name5.SetRawInfo(NAME_LAST, UTF8ToUTF16("Public"));

  NameInfo synthesized;
  synthesized.SetRawInfo(NAME_FULL, UTF8ToUTF16("John Public"));
  synthesized.SetRawInfo(NAME_FIRST, UTF8ToUTF16("John"));
  synthesized.SetRawInfo(NAME_MIDDLE, UTF8ToUTF16(""));
  synthesized.SetRawInfo(NAME_LAST, UTF8ToUTF16("Public"));

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
  NameInfo name1 = CreateNameInfo("호", "", "이영", "이영 호");
  NameInfo name2 = CreateNameInfo("이영호", "", "", "이영호");
  NameInfo name3 = CreateNameInfo("영호", "", "이", "이영호");
  NameInfo name4 = CreateNameInfo("영호", "", "이", "");
  NameInfo name5 = CreateNameInfo("영호", "", "이", "이 영호");

  // Mergeable foreign name in Japanese with a 'KATAKANA MIDDLE DOT'.
  NameInfo name6 = CreateNameInfo("", "", "", "ゲイツ・ビル");
  NameInfo name7 = CreateNameInfo("ビル", "", "ゲイツ", "");

  // Set the use dates for the profiles, because |MergeCJKNames()| tries to use
  // the most recent profile if there is a conflict. The ordering is
  // p1 > p2 > p3 > p4 > p5, with p1 being the most recent.
  AutofillProfile p1 = CreateProfileWithName(name1);
  p1.set_use_date(AutofillClock::Now());
  AutofillProfile p2 = CreateProfileWithName(name2);
  p2.set_use_date(AutofillClock::Now() - base::TimeDelta::FromHours(1));
  AutofillProfile p3 = CreateProfileWithName(name3);
  p3.set_use_date(AutofillClock::Now() - base::TimeDelta::FromHours(2));
  AutofillProfile p4 = CreateProfileWithName(name4);
  p4.set_use_date(AutofillClock::Now() - base::TimeDelta::FromHours(3));
  AutofillProfile p5 = CreateProfileWithName(name5);
  p5.set_use_date(AutofillClock::Now() - base::TimeDelta::FromHours(4));

  AutofillProfile p6 = CreateProfileWithName(name6);
  AutofillProfile p7 = CreateProfileWithName(name7);

  // Because |p1| is the most recent, it always wins over others.
  MergeNamesAndExpect(p1, p2, CreateNameInfo("호", "", "이영", "이영 호"));
  MergeNamesAndExpect(p1, p3, CreateNameInfo("호", "", "이영", "이영 호"));
  MergeNamesAndExpect(p1, p4, CreateNameInfo("호", "", "이영", "이영 호"));
  MergeNamesAndExpect(p1, p5, CreateNameInfo("호", "", "이영", "이영 호"));

  // |p2| is more recent than |p3|, |p4|, and |p5|. However, it does not have a
  // surname entry (it was probably parsed with the old logic), so the other
  // profiles are used as the source for given/surname.
  MergeNamesAndExpect(p2, p3, CreateNameInfo("영호", "", "이", "이영호"));
  MergeNamesAndExpect(p2, p4, CreateNameInfo("영호", "", "이", "이영호"));
  MergeNamesAndExpect(p2, p5, CreateNameInfo("영호", "", "이", "이영호"));

  // |p3| is more recent than |p4| and |p5|.
  MergeNamesAndExpect(p3, p4, CreateNameInfo("영호", "", "이", "이영호"));
  MergeNamesAndExpect(p3, p5, CreateNameInfo("영호", "", "이", "이영호"));

  // |p4| is more recent than |p5|. However, it does not have an explicit full
  // name, so use the one from |p5|.
  MergeNamesAndExpect(p4, p5, CreateNameInfo("영호", "", "이", "이 영호"));

  // There is no conflict between |p6| and |p7|, so use the parts from both.
  MergeNamesAndExpect(p6, p7,
                      CreateNameInfo("ビル", "", "ゲイツ", "ゲイツ・ビル"));
}

TEST_F(AutofillProfileComparatorTest, MergeEmailAddresses) {
  static const char kEmailA[] = "testaccount@domain.net";
  static const char kEmailB[] = "TestAccount@Domain.Net";

  EmailInfo email_a;
  email_a.SetRawInfo(EMAIL_ADDRESS, UTF8ToUTF16(kEmailA));
  AutofillProfile profile_a = CreateProfileWithEmail(kEmailA);
  profile_a.set_use_date(AutofillClock::Now());

  EmailInfo email_b;
  email_b.SetRawInfo(EMAIL_ADDRESS, UTF8ToUTF16(kEmailB));
  AutofillProfile profile_b = CreateProfileWithEmail(kEmailB);
  profile_b.set_use_date(profile_a.use_date() + base::TimeDelta::FromDays(1));

  MergeEmailAddressesAndExpect(profile_a, profile_a, email_a);
  MergeEmailAddressesAndExpect(profile_b, profile_b, email_b);
  MergeEmailAddressesAndExpect(profile_a, profile_b, email_b);
  MergeEmailAddressesAndExpect(profile_b, profile_a, email_b);
}

TEST_F(AutofillProfileComparatorTest, MergeCompanyNames) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{autofill::features::
                                kAutofillRejectCompanyBirthyear},
      /*disabled_features=*/{});

  static const char kCompanyA[] = "Some Company";
  static const char kCompanyB[] = "SÔMÈ ÇÖMPÁÑÝ";
  static const char kCompanyC[] = "SÔMÈ ÇÖMPÁÑÝ A.G.";
  static const char kCompanyD[] = "1987";

  CompanyInfo company_a;
  company_a.SetRawInfo(COMPANY_NAME, UTF8ToUTF16(kCompanyA));
  AutofillProfile profile_a = CreateProfileWithCompanyName(kCompanyA);
  profile_a.set_use_date(AutofillClock::Now());

  // Company Name B is post_normalization identical to Company Name A. The use
  // date will be used to choose between them.
  CompanyInfo company_b;
  company_b.SetRawInfo(COMPANY_NAME, UTF8ToUTF16(kCompanyB));
  AutofillProfile profile_b = CreateProfileWithCompanyName(kCompanyB);
  profile_b.set_use_date(profile_a.use_date() + base::TimeDelta::FromDays(1));

  // Company Name C is the most complete. Even though it has the earliest use
  // date, it will be preferred to the other two.
  CompanyInfo company_c;
  company_c.SetRawInfo(COMPANY_NAME, UTF8ToUTF16(kCompanyC));
  AutofillProfile profile_c = CreateProfileWithCompanyName(kCompanyC);
  profile_c.set_use_date(profile_a.use_date() - base::TimeDelta::FromDays(1));

  // Company Name D is in the format of a birthyear, invalid and non-verified.
  CompanyInfo company_d;
  company_d.SetRawInfo(COMPANY_NAME, UTF8ToUTF16(kCompanyD));
  AutofillProfile profile_d = CreateProfileWithCompanyName(kCompanyD);
  profile_a.set_use_date(AutofillClock::Now());

  MergeCompanyNamesAndExpect(profile_a, profile_a, company_a);
  MergeCompanyNamesAndExpect(profile_a, profile_b, company_b);
  MergeCompanyNamesAndExpect(profile_a, profile_c, company_c);
  MergeCompanyNamesAndExpect(profile_a, profile_d, company_a);

  MergeCompanyNamesAndExpect(profile_b, profile_a, company_b);
  MergeCompanyNamesAndExpect(profile_b, profile_b, company_b);
  MergeCompanyNamesAndExpect(profile_b, profile_c, company_c);
  MergeCompanyNamesAndExpect(profile_b, profile_d, company_b);

  MergeCompanyNamesAndExpect(profile_c, profile_a, company_c);
  MergeCompanyNamesAndExpect(profile_c, profile_b, company_c);
  MergeCompanyNamesAndExpect(profile_c, profile_c, company_c);
  MergeCompanyNamesAndExpect(profile_c, profile_d, company_c);

  MergeCompanyNamesAndExpect(profile_d, profile_a, company_a);
  MergeCompanyNamesAndExpect(profile_d, profile_b, company_b);
  MergeCompanyNamesAndExpect(profile_d, profile_c, company_c);
  MergeCompanyNamesAndExpect(profile_d, profile_d, company_d);
}

TEST_F(AutofillProfileComparatorTest, MergePhoneNumbers_NA) {
  static const char kPhoneA[] = "5550199";
  static const char kPhoneB[] = "555.0199";
  static const char kPhoneC[] = "555-0199 ext321";
  static const char kPhoneD[] = "8005550199";
  static const char kPhoneE[] = "800-555-0199 #321";
  static const char kPhoneF[] = "1-800-555-0199 #321";
  static const char kPhoneG[] = "+1 (800) 555.0199;ext=321";
  static const char kMergedShortNumber[] = "5550199";
  static const char kMergedShortNumberExt[] = "5550199 ext. 321";
  static const char kMergedFullNumber[] = "+1 800-555-0199";
  static const char kMergedFullNumberExt[] = "+1 800-555-0199 ext. 321";

  AutofillProfile profile_a = CreateProfileWithPhoneNumber(kPhoneA);
  AutofillProfile profile_b = CreateProfileWithPhoneNumber(kPhoneB);
  AutofillProfile profile_c = CreateProfileWithPhoneNumber(kPhoneC);
  AutofillProfile profile_d = CreateProfileWithPhoneNumber(kPhoneD);
  AutofillProfile profile_e = CreateProfileWithPhoneNumber(kPhoneE);
  AutofillProfile profile_f = CreateProfileWithPhoneNumber(kPhoneF);
  AutofillProfile profile_g = CreateProfileWithPhoneNumber(kPhoneG);

  // Profile A
  MergePhoneNumbersAndExpect(profile_a, profile_a, kPhoneA);
  MergePhoneNumbersAndExpect(profile_a, profile_b, kMergedShortNumber);
  MergePhoneNumbersAndExpect(profile_a, profile_c, kMergedShortNumberExt);
  MergePhoneNumbersAndExpect(profile_a, profile_d, kMergedFullNumber);
  MergePhoneNumbersAndExpect(profile_a, profile_e, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_a, profile_f, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_a, profile_g, kMergedFullNumberExt);

  // Profile B
  MergePhoneNumbersAndExpect(profile_b, profile_a, kMergedShortNumber);
  MergePhoneNumbersAndExpect(profile_b, profile_b, kPhoneB);
  MergePhoneNumbersAndExpect(profile_b, profile_c, kMergedShortNumberExt);
  MergePhoneNumbersAndExpect(profile_b, profile_d, kMergedFullNumber);
  MergePhoneNumbersAndExpect(profile_b, profile_e, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_b, profile_f, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_b, profile_g, kMergedFullNumberExt);

  // Profile C
  MergePhoneNumbersAndExpect(profile_c, profile_a, kMergedShortNumberExt);
  MergePhoneNumbersAndExpect(profile_c, profile_b, kMergedShortNumberExt);
  MergePhoneNumbersAndExpect(profile_c, profile_c, kPhoneC);
  MergePhoneNumbersAndExpect(profile_c, profile_d, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_c, profile_e, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_c, profile_f, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_c, profile_g, kMergedFullNumberExt);

  // Profile D
  MergePhoneNumbersAndExpect(profile_d, profile_a, kMergedFullNumber);
  MergePhoneNumbersAndExpect(profile_d, profile_b, kMergedFullNumber);
  MergePhoneNumbersAndExpect(profile_d, profile_c, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_d, profile_d, kPhoneD);
  MergePhoneNumbersAndExpect(profile_d, profile_e, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_d, profile_f, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_d, profile_g, kMergedFullNumberExt);

  // Profile E
  MergePhoneNumbersAndExpect(profile_e, profile_a, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_e, profile_b, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_e, profile_c, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_e, profile_d, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_e, profile_e, kPhoneE);
  MergePhoneNumbersAndExpect(profile_e, profile_f, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_e, profile_g, kMergedFullNumberExt);

  // Profile F
  MergePhoneNumbersAndExpect(profile_f, profile_a, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_f, profile_b, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_f, profile_c, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_f, profile_d, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_f, profile_e, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_f, profile_f, kPhoneF);
  MergePhoneNumbersAndExpect(profile_f, profile_g, kMergedFullNumberExt);

  // Profile G
  MergePhoneNumbersAndExpect(profile_g, profile_a, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_g, profile_b, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_g, profile_c, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_g, profile_d, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_g, profile_e, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_g, profile_f, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_g, profile_g, kPhoneG);
}

TEST_F(AutofillProfileComparatorTest, MergePhoneNumbers_Intl) {
  const base::string16 kGermany = UTF8ToUTF16("DE");
  const AutofillType kCountry(ADDRESS_HOME_COUNTRY);

  static const char kPhoneA[] = "+49492180185611";
  static const char kPhoneB[] = "+49 4921 801 856-11";
  static const char kPhoneC[] = "+49 4921 8018 5611;ext=22";
  static const char kPhoneD[] = "04921 80185611";  // National Format.
  static const char kMergedFullNumber[] = "+49 4921 80185611";
  static const char kMergedFullNumberExt[] = "+49 4921 80185611 ext. 22";

  AutofillProfile profile_a = CreateProfileWithPhoneNumber(kPhoneA);
  AutofillProfile profile_b = CreateProfileWithPhoneNumber(kPhoneB);
  AutofillProfile profile_c = CreateProfileWithPhoneNumber(kPhoneC);
  AutofillProfile profile_d = CreateProfileWithPhoneNumber(kPhoneD);

  profile_a.SetInfo(kCountry, kGermany, kLocale);
  profile_b.SetInfo(kCountry, kGermany, kLocale);
  profile_c.SetInfo(kCountry, kGermany, kLocale);
  profile_d.SetInfo(kCountry, kGermany, kLocale);

  // Profile A
  MergePhoneNumbersAndExpect(profile_a, profile_a, kPhoneA);
  MergePhoneNumbersAndExpect(profile_a, profile_b, kMergedFullNumber);
  MergePhoneNumbersAndExpect(profile_a, profile_c, kMergedFullNumberExt);

  // Profile B
  MergePhoneNumbersAndExpect(profile_b, profile_a, kMergedFullNumber);
  MergePhoneNumbersAndExpect(profile_b, profile_b, kPhoneB);
  MergePhoneNumbersAndExpect(profile_b, profile_c, kMergedFullNumberExt);

  // Profile C
  MergePhoneNumbersAndExpect(profile_c, profile_a, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_c, profile_b, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_c, profile_c, kPhoneC);

  // Profile D
  MergePhoneNumbersAndExpect(profile_d, profile_a, kMergedFullNumber);
  MergePhoneNumbersAndExpect(profile_d, profile_b, kMergedFullNumber);
  MergePhoneNumbersAndExpect(profile_d, profile_c, kMergedFullNumberExt);
  MergePhoneNumbersAndExpect(profile_d, profile_d, kPhoneD);
}

TEST_F(AutofillProfileComparatorTest, MergeAddresses) {
  AutofillProfile p1 = CreateProfileWithAddress(
      "1 Some Street", "Unit 3", "Carver", "CA - California", "90210", "US");
  AutofillProfile p2 = CreateProfileWithAddress(
      "1 Some Street #3", "", "Carver City", "ca", "90210-1234", "us");

  Address expected;
  expected.SetRawInfo(ADDRESS_HOME_LINE1, UTF8ToUTF16("1 Some Street"));
  expected.SetRawInfo(ADDRESS_HOME_LINE2, UTF8ToUTF16("Unit 3"));
  expected.SetRawInfo(ADDRESS_HOME_CITY, UTF8ToUTF16("Carver City"));
  expected.SetRawInfo(ADDRESS_HOME_STATE, UTF8ToUTF16("ca"));
  expected.SetRawInfo(ADDRESS_HOME_ZIP, UTF8ToUTF16("90210-1234"));
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, UTF8ToUTF16("US"));

  MergeAddressesAndExpect(p1, p2, expected);
}

TEST_F(AutofillProfileComparatorTest, MergeAddressesMostUniqueTokens) {
  AutofillProfile p1 = CreateProfileWithAddress(
      "1 Some Street", "Unit 3", "Carver", "CA - California", "90210", "US");
  AutofillProfile p2 = CreateProfileWithAddress(
      "1 Some Other Street", "Unit 3", "Carver City", "ca", "90210-1234", "us");

  Address expected;
  expected.SetRawInfo(ADDRESS_HOME_LINE1, UTF8ToUTF16("1 Some Other Street"));
  expected.SetRawInfo(ADDRESS_HOME_LINE2, UTF8ToUTF16("Unit 3"));
  expected.SetRawInfo(ADDRESS_HOME_CITY, UTF8ToUTF16("Carver City"));
  expected.SetRawInfo(ADDRESS_HOME_STATE, UTF8ToUTF16("ca"));
  expected.SetRawInfo(ADDRESS_HOME_ZIP, UTF8ToUTF16("90210-1234"));
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, UTF8ToUTF16("US"));

  MergeAddressesAndExpect(p1, p2, expected);
  MergeAddressesAndExpect(p2, p1, expected);
}

TEST_F(AutofillProfileComparatorTest, MergeAddressesWithRewrite) {
  AutofillProfile p1 = CreateProfileWithAddress(
      "6543 CH BACON", "APP 3", "MONTRÉAL", "QUÉBEC", "HHH999", "ca");
  AutofillProfile p2 = CreateProfileWithAddress(
      "6543, Bacon Rd", "", "Montreal", "QC", "hhh 999", "CA");
  p2.set_use_date(p1.use_date() + base::TimeDelta::FromMinutes(1));

  Address expected;
  expected.SetRawInfo(ADDRESS_HOME_LINE1, UTF8ToUTF16("6543 CH BACON"));
  expected.SetRawInfo(ADDRESS_HOME_LINE2, UTF8ToUTF16("APP 3"));
  expected.SetRawInfo(ADDRESS_HOME_CITY, UTF8ToUTF16("Montreal"));
  expected.SetRawInfo(ADDRESS_HOME_STATE, UTF8ToUTF16("QC"));
  expected.SetRawInfo(ADDRESS_HOME_ZIP, UTF8ToUTF16("hhh 999"));
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, UTF8ToUTF16("CA"));

  MergeAddressesAndExpect(p1, p2, expected);
  MergeAddressesAndExpect(p2, p1, expected);
}

TEST_F(AutofillProfileComparatorTest,
       MergeAddressesDependendLocalityAndSortingCode) {
  AutofillProfile p1 = CreateProfileWithAddress(
      "6543 CH BACON", "APP 3", "MONTRÉAL", "QUÉBEC", "HHH999", "ca");
  p1.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, UTF8ToUTF16("Some String"));
  p1.SetRawInfo(ADDRESS_HOME_SORTING_CODE, UTF8ToUTF16("64205 Biarritz CEDEX"));
  AutofillProfile p2 = CreateProfileWithAddress(
      "6543, Bacon Rd", "", "Montreal", "QC", "hhh 999", "CA");
  p2.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                UTF8ToUTF16("Some Other String"));
  p2.SetRawInfo(ADDRESS_HOME_SORTING_CODE, UTF8ToUTF16("64205 Biarritz"));
  p2.set_use_date(p1.use_date() + base::TimeDelta::FromMinutes(1));

  Address expected;
  expected.SetRawInfo(ADDRESS_HOME_LINE1, UTF8ToUTF16("6543 CH BACON"));
  expected.SetRawInfo(ADDRESS_HOME_LINE2, UTF8ToUTF16("APP 3"));
  expected.SetRawInfo(ADDRESS_HOME_CITY, UTF8ToUTF16("Montreal"));
  expected.SetRawInfo(ADDRESS_HOME_STATE, UTF8ToUTF16("QC"));
  expected.SetRawInfo(ADDRESS_HOME_ZIP, UTF8ToUTF16("hhh 999"));
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, UTF8ToUTF16("CA"));
  expected.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                      UTF8ToUTF16("Some Other String"));
  expected.SetRawInfo(ADDRESS_HOME_SORTING_CODE,
                      UTF8ToUTF16("64205 Biarritz"));  // Preferred by use date.

  MergeAddressesAndExpect(p1, p2, expected);
  MergeAddressesAndExpect(p2, p1, expected);
}
