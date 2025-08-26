// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/address.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/addresses/contact_info.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using i18n_model_definition::kLegacyHierarchyCountryCode;
using ::testing::IsEmpty;

const char kLocale[] = "en-US";

class AutofillProfileComparatorTest : public testing::Test {
 public:
  // Expose the protected methods of AutofillProfileComparator for testing.
  class AutofillProfileComparator : public autofill::AutofillProfileComparator {
   public:
    typedef autofill::AutofillProfileComparator Super;
    using Super::CompareTokens;
    using Super::HaveMergeableAddresses;
    using Super::HaveMergeableCompanyNames;
    using Super::HaveMergeableEmailAddresses;
    using Super::HaveMergeablePhoneNumbers;
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

  AutofillProfile CreateProfileWithEmail(const char* email) {
    AutofillProfile profile(kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile, "", "", "", email, "", "", "", "", "", "",
                         "", "");
    return profile;
  }

  AutofillProfile CreateProfileWithCompanyName(const char* company_name,
                                               const char* country_code = "") {
    AutofillProfile profile(kLegacyHierarchyCountryCode);
    test::SetProfileInfo(&profile, "", "", "", "", company_name, "", "", "", "",
                         "", country_code, "");
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
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      std::u16string(), std::u16string(),
      normalization::WhitespaceSpec::kRetain));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      std::u16string(), std::u16string(),
      normalization::WhitespaceSpec::kDiscard));

  // Checks that leading punctuation and white space are ignored.
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u".,  -().", u"", normalization::WhitespaceSpec::kRetain));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u".,  -().", u"", normalization::WhitespaceSpec::kDiscard));

  // Checks that trailing punctuation and white space are ignored.
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"a ., ", u"a", normalization::WhitespaceSpec::kRetain));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"a ., ", u"a", normalization::WhitespaceSpec::kDiscard));

  // Checks that embedded punctuation and white space is collapsed to a single
  // white space with WhitespaceSpec::kRetain and is ignored with
  // WhitespaceSpec::kDiscard.
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"a() -  a", u"a a", normalization::WhitespaceSpec::kRetain));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"a() -  a", u"aa", normalization::WhitespaceSpec::kDiscard));

  // Checks that characters such as 'œ' respect the status quo established by
  // NormalizeForComparison.
  EXPECT_TRUE(AutofillProfileComparator::Compare(u"œil", u"oeil"));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"Straße", u"Strasse", normalization::WhitespaceSpec::kDiscard));

  // Checks that a substring of the string is not considered equal.
  EXPECT_FALSE(AutofillProfileComparator::Compare(u"A", u"Anna"));

  EXPECT_FALSE(AutofillProfileComparator::Compare(u"Anna", u"A"));

  // Checks that Compare behaves like NormalizeForComparison. Also, checks that
  // diacritics are removed.
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"Timothé", u"timothe", normalization::WhitespaceSpec::kRetain));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u" sven-åke ", u"sven ake", normalization::WhitespaceSpec::kRetain));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"Ç 㸐", u"c 㸐", normalization::WhitespaceSpec::kRetain));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"902103214", u"90210-3214", normalization::WhitespaceSpec::kDiscard));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"Timothé-Noël Étienne Périer", u"timothe noel etienne perier",
      normalization::WhitespaceSpec::kRetain));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"1600 Amphitheatre, Pkwy.", u"1600 amphitheatre pkwy",
      normalization::WhitespaceSpec::kRetain));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"Mid\x2013Island\x2003 Plaza", u"mid island plaza",
      normalization::WhitespaceSpec::kRetain));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"1600 amphitheatre pkwy \n App. 2", u"1600 amphitheatre pkwy app 2",
      normalization::WhitespaceSpec::kRetain));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"まéÖä정", u"まeoa정", normalization::WhitespaceSpec::kRetain));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"유재석", u"유 재석", normalization::WhitespaceSpec::kDiscard));
  EXPECT_TRUE(AutofillProfileComparator::Compare(
      u"ビルゲイツ", u"ヒル・ケイツ", normalization::WhitespaceSpec::kDiscard));
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

  AutofillProfile name_email_profile{AccountInfo{}};

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

  EXPECT_TRUE(comparator_.HaveMergeableAddresses(name_email_profile, p1));
  EXPECT_TRUE(comparator_.HaveMergeableAddresses(p1, name_email_profile));
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

TEST_F(AutofillProfileComparatorTest, MergeEmailAddresses) {
  static const char kEmailA[] = "testaccount@domain.net";
  static const char16_t kEmailA16[] = u"testaccount@domain.net";
  static const char kEmailB[] = "TestAccount@Domain.Net";
  static const char16_t kEmailB16[] = u"TestAccount@Domain.Net";

  EmailInfo email_a;
  email_a.SetRawInfo(EMAIL_ADDRESS, kEmailA16);
  AutofillProfile profile_a = CreateProfileWithEmail(kEmailA);
  profile_a.usage_history().set_use_date(AutofillClock::Now());

  EmailInfo email_b;
  email_b.SetRawInfo(EMAIL_ADDRESS, kEmailB16);
  AutofillProfile profile_b = CreateProfileWithEmail(kEmailB);
  profile_b.usage_history().set_use_date(profile_a.usage_history().use_date() +
                                         base::Days(1));

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
  profile_a.usage_history().set_use_date(AutofillClock::Now());

  // Company Name B is post_normalization identical to Company Name A. The use
  // date will be used to choose between them.
  CompanyInfo company_b;
  company_b.SetRawInfo(COMPANY_NAME, kCompanyB16);
  AutofillProfile profile_b = CreateProfileWithCompanyName(kCompanyB);
  profile_b.usage_history().set_use_date(profile_a.usage_history().use_date() +
                                         base::Days(1));

  // Company Name C is the most complete. Even though it has the earliest use
  // date, it will be preferred to the other two.
  CompanyInfo company_c;
  company_c.SetRawInfo(COMPANY_NAME, kCompanyC16);
  AutofillProfile profile_c = CreateProfileWithCompanyName(kCompanyC);
  profile_c.usage_history().set_use_date(profile_a.usage_history().use_date() -
                                         base::Days(1));

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

  p2.usage_history().set_use_date(p1.usage_history().use_date() +
                                  base::Minutes(1));
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
      "6543 CH BACON", "APP 3", "Barcelona", "Catalunya", "HHH999", "ES");

  p1.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"StreetName");
  p1.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"HouseNumber");
  p1.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"Subpremise");

  AutofillProfile p2 = p1;
  p2.usage_history().set_use_date(p1.usage_history().use_date() +
                                  base::Minutes(1));
  p2.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"StreetName2");
  p2.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"HouseNumber2");
  p2.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"Subpremise2");

  Address expected(AddressCountryCode("ES"));
  expected.SetRawInfo(ADDRESS_HOME_LINE1, u"6543 CH BACON");
  expected.SetRawInfo(ADDRESS_HOME_LINE2, u"APP 3");
  expected.SetRawInfo(ADDRESS_HOME_CITY, u"Barcelona");
  expected.SetRawInfo(ADDRESS_HOME_STATE, u"Catalunya");
  expected.SetRawInfo(ADDRESS_HOME_ZIP, u"HHH999");
  expected.SetRawInfo(ADDRESS_HOME_STREET_NAME, u"StreetName2");
  expected.SetRawInfo(ADDRESS_HOME_HOUSE_NUMBER, u"HouseNumber2");
  expected.SetRawInfo(ADDRESS_HOME_SUBPREMISE, u"Subpremise2");

  MergeAddressesAndExpect(p1, p2, expected);
  MergeAddressesAndExpect(p2, p1, expected);
}

TEST_F(AutofillProfileComparatorTest, MergeAddressesWithRewrite) {
  AutofillProfile p1 = CreateProfileWithAddress(
      "6543 CH BACON", "APP 3", "MONTRÉAL", "QUÉBEC", "HHH999", "CA");
  AutofillProfile p2 = CreateProfileWithAddress(
      "6543, Bacon Rd", "", "Montreal", "QC", "hhh 999", "CA");
  p2.usage_history().set_use_date(p1.usage_history().use_date() +
                                  base::Minutes(1));

  Address expected(AddressCountryCode("CA"));
  expected.SetRawInfo(ADDRESS_HOME_LINE1, u"6543 CH BACON");
  expected.SetRawInfo(ADDRESS_HOME_LINE2, u"APP 3");
  expected.SetRawInfo(ADDRESS_HOME_CITY, u"Montreal");
  expected.SetRawInfo(ADDRESS_HOME_STATE, u"QC");
  expected.SetRawInfo(ADDRESS_HOME_ZIP, u"hhh 999");

  MergeAddressesAndExpect(p1, p2, expected);
  MergeAddressesAndExpect(p2, p1, expected);
}

// The intention of this test is to verify that "Straße" and "Str."" ar
// considered equivalent, which requires a normalization of ß to ss followed by
// a rewrite of "strasse" to "str".
TEST_F(AutofillProfileComparatorTest, MergeAddressesWithRewriteDE) {
  AutofillProfile p1 = CreateProfileWithAddress("Erika-Mann-Straße 33", "APP 3",
                                                "München", "", "80636", "DE");

  AutofillProfile p2 = CreateProfileWithAddress("Erika-Mann-Str. 33", "",
                                                "Munchen", "", "80636", "DE");

  Address expected(AddressCountryCode("DE"));
  // The longer string wins.
  expected.SetRawInfo(ADDRESS_HOME_LINE1, u"Erika-Mann-Straße 33");
  // Extra information.
  expected.SetRawInfo(ADDRESS_HOME_LINE2, u"APP 3");
  // The most recent string wins.
  expected.SetRawInfo(ADDRESS_HOME_CITY, u"Munchen");
  expected.SetRawInfo(ADDRESS_HOME_STATE, u"");
  expected.SetRawInfo(ADDRESS_HOME_ZIP, u"80636");

  MergeAddressesAndExpect(p1, p2, expected);
  MergeAddressesAndExpect(p2, p1, expected);
}

TEST_F(AutofillProfileComparatorTest, MergeAddressesWithGermanTransliteration) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableGermanTransliteration};

  AutofillProfile p1 = CreateProfileWithAddress("Hänsel-Str 33", "", "München",
                                                "", "80636", "DE");
  AutofillProfile p2 = CreateProfileWithAddress("Haensel-Str 33", "", "Munchen",
                                                "", "80636", "DE");

  MergeAddressesAndExpect(p1, p2, p2.GetAddress());
  MergeAddressesAndExpect(p2, p1, p2.GetAddress());
}

TEST_F(AutofillProfileComparatorTest, MergeAddressesOneIsAccountNameEmail) {
  const AutofillProfile address_profile = CreateProfileWithAddress(
      "Hänsel-Str 33", "", "München", "", "80636", "DE");

  AccountInfo info;
  info.full_name = "test name";
  info.email = "testaccount@domain.net";
  const AutofillProfile account_name_email_profile{info};

  MergeAddressesAndExpect(address_profile, account_name_email_profile,
                          address_profile.GetAddress());
  MergeAddressesAndExpect(account_name_email_profile, address_profile,
                          address_profile.GetAddress());
}

TEST_F(AutofillProfileComparatorTest,
       MergeAddressesDependentLocalityAndSortingCode) {
  AutofillProfile p1 =
      CreateProfileWithAddress("6543 CH BACON", "APP 3", "Rio de Janeiro",
                               "Rio de Janeiro", "HHH999", "BR");
  p1.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Some String");
  p1.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"64205 Biarritz CEDEX");
  AutofillProfile p2 = p1;
  p2.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Some Other String");
  p2.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"64205 Biarritz");
  p2.usage_history().set_use_date(p1.usage_history().use_date() +
                                  base::Minutes(1));

  Address expected(AddressCountryCode("BR"));
  expected.SetRawInfo(ADDRESS_HOME_LINE1, u"6543 CH BACON");
  expected.SetRawInfo(ADDRESS_HOME_LINE2, u"APP 3");
  expected.SetRawInfo(ADDRESS_HOME_CITY, u"Rio de Janeiro");
  expected.SetRawInfo(ADDRESS_HOME_STATE, u"Rio de Janeiro");
  expected.SetRawInfo(ADDRESS_HOME_ZIP, u"HHH999");
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

// Tests that JP profiles that differ only in the character set used for
// alternative name are considered equal.
TEST_F(AutofillProfileComparatorTest,
       ProfilesHaveTheSameSettingsVisibleValuesJP) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillSupportPhoneticNameForJP};
  AutofillProfile existing_profile(AddressCountryCode("JP"));
  test::SetProfileInfo(&existing_profile, "firstName", "middleName", "lastName",
                       "mail@mail.com", "company", "line1", "line2", "city",
                       "state", "zip", "JP", "phone");
  existing_profile.SetRawInfo(ALTERNATIVE_GIVEN_NAME, u"あおい");
  existing_profile.SetRawInfo(ALTERNATIVE_FAMILY_NAME, u"やまもと");
  existing_profile.FinalizeAfterImport();

  AutofillProfile new_profile = existing_profile;
  new_profile.SetRawInfo(ALTERNATIVE_GIVEN_NAME, u"アオイ");
  new_profile.SetRawInfo(ALTERNATIVE_FAMILY_NAME, u"ヤマモト");
  new_profile.FinalizeAfterImport();

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

TEST_F(AutofillProfileComparatorTest, GetDifferentCountriesProfileDifference) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillSupportPhoneticNameForJP};
  AutofillProfile existing_profile(AddressCountryCode("US"));
  test::SetProfileInfo(&existing_profile, "firstName", "middleName", "lastName",
                       "mail@mail.com", "company", "line1", "line2", "city",
                       "state", "zip", "US", "phone");

  AutofillProfile second_existing_profile(AddressCountryCode("JP"));
  test::SetProfileInfo(&second_existing_profile, "firstName", "middleName",
                       "lastName", "mail@mail.com", "company", "line1", "line2",
                       "city", "state", "zip", "JP", "phone");
  second_existing_profile.SetRawInfo(ALTERNATIVE_GIVEN_NAME,
                                     u"alternativeGivenName");
  second_existing_profile.SetRawInfo(ALTERNATIVE_FAMILY_NAME,
                                     u"alternativeFamilyName");
  second_existing_profile.FinalizeAfterImport();

  // There should be no difference in NAME_FULL type.
  EXPECT_THAT(
      AutofillProfileComparator::GetProfileDifference(
          existing_profile, second_existing_profile, {NAME_FULL}, kLocale),
      IsEmpty());

  // But there should be a difference in the ADDRESS_HOME_ZIP type.
  std::vector<ProfileValueDifference> expected_difference = {
      {ALTERNATIVE_FULL_NAME, u"",
       u"alternativeGivenName alternativeFamilyName"}};

  EXPECT_EQ(AutofillProfileComparator::GetProfileDifference(
                existing_profile, second_existing_profile,
                {ALTERNATIVE_FULL_NAME}, kLocale),
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
          std::tuple<AddressCountryCode,
                     NonMergeableSettingVisibleTypesTestCase>> {};

TEST_P(NonMergeableSettingVisibleTypesTest, DifferingTypesLegacy) {
  AddressCountryCode country_code = std::get<0>(GetParam());
  const NonMergeableSettingVisibleTypesTestCase& type_differences =
      std::get<1>(GetParam());

  // Construct two profiles differing in exactly `test.differing_types`.
  AutofillProfile a(country_code);
  AutofillProfile b(country_code);
  // If AutofillProfile::GetUserVisibleTypes() does not contain a type of
  // `differing_types`, the test is not applicable. This can happen for types
  // that are not setting-visible in a certain country.
  if (std::ranges::any_of(type_differences.differing_types, [&](FieldType t) {
        return !a.GetUserVisibleTypes().contains(t);
      })) {
    return;
  }
  for (FieldType t : type_differences.differing_types) {
    a.SetRawInfo(t, u"a");
    b.SetRawInfo(t, u"b");
  }
  // Initialize all other setting-visible types with the same value.
  for (FieldType t : a.GetUserVisibleTypes()) {
    // If a type of the same `FieldTypeGroup` was already set, ignore it, to
    // avoid constructing conflicting substructures.
    if (!base::Contains(type_differences.differing_types,
                        GroupTypeOfFieldType(t), &GroupTypeOfFieldType)) {
      a.SetRawInfo(t, u"same");
      b.SetRawInfo(t, u"same");
    }
  }
  ASSERT_TRUE(a.FinalizeAfterImport());
  ASSERT_TRUE(b.FinalizeAfterImport());
  EXPECT_THAT(
      comparator_.NonMergeableSettingVisibleTypes(a, b),
      testing::Optional(type_differences.expected_setting_visible_difference));
}

INSTANTIATE_TEST_SUITE_P(
    AutofillProfileComparatorTest,
    NonMergeableSettingVisibleTypesTest,
    testing::Combine(
        testing::Values(AddressCountryCode("JP"), kLegacyHierarchyCountryCode),
        testing::Values(
            // No differences:
            NonMergeableSettingVisibleTypesTestCase{{}, {}},
            // Differences in a single non-country type:
            NonMergeableSettingVisibleTypesTestCase{{NAME_FULL}, {NAME_FULL}},
            NonMergeableSettingVisibleTypesTestCase{{NAME_FIRST}, {NAME_FULL}},
            NonMergeableSettingVisibleTypesTestCase{{NAME_LAST}, {NAME_FULL}},
            NonMergeableSettingVisibleTypesTestCase{{ALTERNATIVE_FULL_NAME},
                                                    {ALTERNATIVE_FULL_NAME}},
            NonMergeableSettingVisibleTypesTestCase{{ALTERNATIVE_GIVEN_NAME},
                                                    {ALTERNATIVE_FULL_NAME}},
            NonMergeableSettingVisibleTypesTestCase{{ALTERNATIVE_FAMILY_NAME},
                                                    {ALTERNATIVE_FULL_NAME}},
            NonMergeableSettingVisibleTypesTestCase{{COMPANY_NAME},
                                                    {COMPANY_NAME}},
            NonMergeableSettingVisibleTypesTestCase{
                {ADDRESS_HOME_STREET_ADDRESS},
                {ADDRESS_HOME_STREET_ADDRESS}},
            NonMergeableSettingVisibleTypesTestCase{
                {ADDRESS_HOME_LINE1},
                {ADDRESS_HOME_STREET_ADDRESS}},
            NonMergeableSettingVisibleTypesTestCase{
                {ADDRESS_HOME_LINE2},
                {ADDRESS_HOME_STREET_ADDRESS}},
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
                {NAME_FIRST, ALTERNATIVE_GIVEN_NAME, ADDRESS_HOME_LINE1,
                 ADDRESS_HOME_STATE, EMAIL_ADDRESS},
                {NAME_FULL, ALTERNATIVE_FULL_NAME, ADDRESS_HOME_STREET_ADDRESS,
                 ADDRESS_HOME_STATE, EMAIL_ADDRESS}})));

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
