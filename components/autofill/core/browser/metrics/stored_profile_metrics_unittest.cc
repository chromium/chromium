// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/stored_profile_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

// Separate stored profile count metrics exist for every profile category. Test
// them in a parameterized way.
class StoredProfileMetricsTestByCategory
    : public testing::TestWithParam<AutofillProfileRecordTypeCategory> {
 public:
  StoredProfileMetricsTestByCategory() = default;

  AutofillProfileRecordTypeCategory Category() const { return GetParam(); }

  // Returns the suffix used for the metrics.
  std::string GetSuffix() const { return GetProfileCategorySuffix(Category()); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    StoredProfileMetricsTestByCategory,
    testing::ValuesIn({
        AutofillProfileRecordTypeCategory::kLocalOrSyncable,
        AutofillProfileRecordTypeCategory::kAccountChrome,
        AutofillProfileRecordTypeCategory::kAccountNonChrome,
        AutofillProfileRecordTypeCategory::kAccountHome,
        AutofillProfileRecordTypeCategory::kAccountWork,
        AutofillProfileRecordTypeCategory::kAccountNameEmail,
    }));

// Tests that no profile count metrics for the corresponding category are
// emitted when no profiles of that category are stored.
TEST_P(StoredProfileMetricsTestByCategory, NoProfiles) {
  base::HistogramTester histogram_tester;
  LogStoredProfileMetrics(/*profiles=*/{});

  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCount." + GetSuffix(), 0, 1);
  // The following metrics are expected not to be emitted when no profiles are
  // stored.
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfileDisusedCount." + GetSuffix(), 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfileUsedCount." + GetSuffix(), 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfileUsedPercentage." + GetSuffix(), 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredProfile." + GetSuffix(), 0);
}

// Tests that when profiles of a category exist, they metrics are emitted.
TEST_P(StoredProfileMetricsTestByCategory, StoredProfiles) {
  // Create a recently used (3 days ago) profile.
  AutofillProfile profile0 = test::GetFullProfile();
  profile0.usage_history().set_use_date(AutofillClock::Now() - base::Days(3));
  test::SetProfileCategory(profile0, Category());

  // Create a profile used a long time (200 days) ago.
  AutofillProfile profile1 = test::GetFullProfile2();
  profile1.usage_history().set_use_date(AutofillClock::Now() - base::Days(200));
  test::SetProfileCategory(profile1, Category());

  // Log the metrics and verify expectations.
  base::HistogramTester histogram_tester;
  LogStoredProfileMetrics({&profile0, &profile1});

  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCount." + GetSuffix(), 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileDisusedCount." + GetSuffix(), 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileUsedCount." + GetSuffix(), 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileUsedPercentage." + GetSuffix(), 50, 1);

  const std::string last_used_metric =
      "Autofill.DaysSinceLastUse.StoredProfile." + GetSuffix();
  histogram_tester.ExpectTotalCount(last_used_metric, 2);
  histogram_tester.ExpectBucketCount(last_used_metric, 3, 1);
  histogram_tester.ExpectBucketCount(last_used_metric, 200, 1);
}

TEST(StoredProfileMetricsTest, TotalPostalAddressProfiles) {
  // A full profile is a postal address.
  AutofillProfile postal_address = test::GetFullProfile();
  // A profile with only one address field is not a postal address.
  AutofillProfile partial_address(AddressCountryCode("US"));
  partial_address.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", "en-US");
  // A profile with two address fields is a postal address.
  AutofillProfile minimal_postal_address(AddressCountryCode("US"));
  minimal_postal_address.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", "en-US");
  minimal_postal_address.SetInfo(ADDRESS_HOME_ZIP, u"94043", "en-US");
  // A profile with just name and email is not a postal address.
  AutofillProfile name_and_email_profile(AddressCountryCode("US"));
  name_and_email_profile.SetInfo(NAME_FULL, u"John Doe", "en-US");
  name_and_email_profile.SetInfo(EMAIL_ADDRESS, u"john@doe.com", "en-US");

  base::HistogramTester histogram_tester;
  LogStoredProfileMetrics({&postal_address, &partial_address,
                           &minimal_postal_address, &name_and_email_profile});
  // `postal_address` and `minimal_postal_address` should be counted.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCount.TotalPostalAddressProfiles", 2, 1);
}

// Tests that if profiles contain an alternative name, the metrics are emitted.
TEST_P(StoredProfileMetricsTestByCategory, StoredProfilesWithAlternativeName) {
  // Create profiles with alternative names.
  AutofillProfile profile0 = test::GetFullProfile(AddressCountryCode("JP"));
  profile0.SetInfo(ALTERNATIVE_FULL_NAME, u"あおい", "ja");

  AutofillProfile profile1 = test::GetFullProfile2(AddressCountryCode("JP"));
  profile1.SetInfo(ALTERNATIVE_FULL_NAME, u"やまもと·あおい", "ja");

  // Create a profile without the alternative name set.
  AutofillProfile profile2 = test::GetFullProfile();

  // Log the metrics and verify expectations.
  base::HistogramTester histogram_tester;
  LogStoredProfileCountWithAlternativeName({&profile0, &profile1, &profile2});
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCount.WithAlternativeName", 2, 1);
}

// Tests that email addresses, phone numbers, company names, postal codes,
// country codes, and phonetic alternative names do not pollute the script
// classification.
TEST(StoredProfileMetricsTest, ExcludedFieldsDoNotPolluteClassification) {
  {
    AutofillProfile cjk_profile{AddressCountryCode("JP")};
    test::SetProfileTestValues(&cjk_profile,
                               {{NAME_FULL, "山田 太郎"},
                                {ADDRESS_HOME_STREET_ADDRESS, "千代田区1-1-1"},
                                {ADDRESS_HOME_CITY, "東京都"},
                                {ADDRESS_HOME_ZIP, "JP-100-0001"},
                                {EMAIL_ADDRESS, "yamada.taro@example.com"},
                                {PHONE_HOME_WHOLE_NUMBER, "+81 90-1234-5678"},
                                {COMPANY_NAME, "Google LLC"}});

    base::HistogramTester histogram_tester;
    LogStoredProfileScriptTypeMetrics({cjk_profile});
    histogram_tester.ExpectUniqueSample(
        "Autofill.StoredProfile.CharacterScriptType",
        AutofillProfileCharacterScriptType::kCjk, 1);
  }

  {
    AutofillProfile jp_ascii_profile{AddressCountryCode("JP")};
    test::SetProfileTestValues(&jp_ascii_profile,
                               {{NAME_FULL, "Taro Yamada"},
                                {ADDRESS_HOME_CITY, "Tokyo"},
                                {ALTERNATIVE_FULL_NAME, "やまだ たろう"}});

    base::HistogramTester histogram_tester;
    LogStoredProfileScriptTypeMetrics({jp_ascii_profile});
    histogram_tester.ExpectUniqueSample(
        "Autofill.StoredProfile.CharacterScriptType",
        AutofillProfileCharacterScriptType::kAsciiOnly, 1);
  }
}

struct SingleProfileTestCase {
  std::string test_name;
  std::string country_code;
  std::vector<test::ProfileTestData> test_data;
  AutofillProfileCharacterScriptType expected_script;
};

// Tests single-profile telemetry logging across all character script types.
class StoredProfileSingleProfileScriptMetricsTest
    : public testing::TestWithParam<SingleProfileTestCase> {};

TEST_P(StoredProfileSingleProfileScriptMetricsTest, MetricsRecordedCorrectly) {
  const SingleProfileTestCase& test_case = GetParam();
  AutofillProfile profile{AddressCountryCode(test_case.country_code)};
  test::SetProfileTestValues(&profile, test_case.test_data);

  base::HistogramTester histogram_tester;
  LogStoredProfileScriptTypeMetrics({profile});

  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfile.CharacterScriptType", test_case.expected_script,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCountByScriptType.AsciiOnly",
      test_case.expected_script ==
          AutofillProfileCharacterScriptType::kAsciiOnly,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCountByScriptType.Latin1Supplement",
      test_case.expected_script ==
          AutofillProfileCharacterScriptType::kLatin1Supplement,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCountByScriptType.LatinExtended",
      test_case.expected_script ==
          AutofillProfileCharacterScriptType::kLatinExtended,
      1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    StoredProfileSingleProfileScriptMetricsTest,
    testing::Values(
        SingleProfileTestCase{
            "AsciiLatinOnly",
            "US",
            {{NAME_FULL, "John Doe"},
             {ADDRESS_HOME_STREET_ADDRESS, "1600 Amphitheatre Pkwy"},
             {ADDRESS_HOME_CITY, "Mountain View"},
             {ADDRESS_HOME_STATE, "CA"},
             {ADDRESS_HOME_ZIP, "94043"}},
            AutofillProfileCharacterScriptType::kAsciiOnly},
        SingleProfileTestCase{
            "SpanishOrdinalIndicatorsTreatedAsNeutral",
            "ES",
            {{NAME_FULL, "Juan Garcia"},
             {ADDRESS_HOME_STREET_ADDRESS, "Calle Mayor 10, 2º, 1ª"},
             {ADDRESS_HOME_CITY, "Madrid"},
             {ADDRESS_HOME_STATE, "Madrid"},
             {ADDRESS_HOME_ZIP, "28013"}},
            AutofillProfileCharacterScriptType::kAsciiOnly},
        SingleProfileTestCase{
            "RomanNumeralsTreatedAsNeutral",
            "JP",
            {{NAME_FULL, "山田 太郎"},
             {ADDRESS_HOME_STREET_ADDRESS, "六本木 6-10-1 第Ⅰビル"},
             {ADDRESS_HOME_CITY, "港区"},
             {ADDRESS_HOME_STATE, "東京都"},
             {ADDRESS_HOME_ZIP, "106-6108"}},
            AutofillProfileCharacterScriptType::kCjk},
        SingleProfileTestCase{
            "LatinExtended_PolishWithBothLatin1AndExtended",
            "PL",
            {{NAME_FULL, "Przemysław Łódź"},
             {ADDRESS_HOME_STREET_ADDRESS, "ul. Świętokrzyska 12/4"},
             {ADDRESS_HOME_CITY, "Kraków"},
             {ADDRESS_HOME_STATE, "Małopolskie"},
             {ADDRESS_HOME_ZIP, "31-001"}},
            AutofillProfileCharacterScriptType::kLatinExtended},
        SingleProfileTestCase{
            "Latin1Supplement_French",
            "FR",
            {{NAME_FULL, "François Müller"},
             {ADDRESS_HOME_STREET_ADDRESS, "12 Rue de la Paix"},
             {ADDRESS_HOME_CITY, "Paris"},
             {ADDRESS_HOME_STATE, "Île-de-France"},
             {ADDRESS_HOME_ZIP, "75002"}},
            AutofillProfileCharacterScriptType::kLatin1Supplement},
        SingleProfileTestCase{"Cyrillic_Russian",
                              "RU",
                              {{NAME_FULL, "Иван Иванов"},
                               {ADDRESS_HOME_STREET_ADDRESS, "ул. Тверская 15"},
                               {ADDRESS_HOME_CITY, "Москва"},
                               {ADDRESS_HOME_STATE, "Московская область"},
                               {ADDRESS_HOME_ZIP, "125009"}},
                              AutofillProfileCharacterScriptType::kCyrillic},
        SingleProfileTestCase{"Arabic_Egyptian",
                              "EG",
                              {{NAME_FULL, "محمد علي"},
                               {ADDRESS_HOME_STREET_ADDRESS, "شارع التحرير 25"},
                               {ADDRESS_HOME_CITY, "القاهرة"},
                               {ADDRESS_HOME_STATE, "محافظة القاهرة"},
                               {ADDRESS_HOME_ZIP, "11511"}},
                              AutofillProfileCharacterScriptType::kArabic},
        SingleProfileTestCase{"Cjk_Japanese",
                              "JP",
                              {{NAME_FULL, "山田 太郎"},
                               {ADDRESS_HOME_STREET_ADDRESS, "六本木 6-10-1"},
                               {ADDRESS_HOME_CITY, "港区"},
                               {ADDRESS_HOME_STATE, "東京都"},
                               {ADDRESS_HOME_ZIP, "106-6108"}},
                              AutofillProfileCharacterScriptType::kCjk},
        SingleProfileTestCase{"Indic_Hindi",
                              "IN",
                              {{NAME_FULL, "राजेश शर्मा"},
                               {ADDRESS_HOME_STREET_ADDRESS, "शान्ति पथ 45"},
                               {ADDRESS_HOME_CITY, "नई दिल्ली"},
                               {ADDRESS_HOME_STATE, "दिल्ली"},
                               {ADDRESS_HOME_ZIP, "110001"}},
                              AutofillProfileCharacterScriptType::kIndic},
        SingleProfileTestCase{"Greek",
                              "GR",
                              {{NAME_FULL, "Νικόλαος Παπαδόπουλος"},
                               {ADDRESS_HOME_STREET_ADDRESS, "Ερμού 10"},
                               {ADDRESS_HOME_CITY, "Αθήνα"},
                               {ADDRESS_HOME_STATE, "Αττική"},
                               {ADDRESS_HOME_ZIP, "10563"}},
                              AutofillProfileCharacterScriptType::kGreek},
        SingleProfileTestCase{"Hebrew",
                              "IL",
                              {{NAME_FULL, "אברהם כהן"},
                               {ADDRESS_HOME_STREET_ADDRESS, "רחוב דיזנגוף 50"},
                               {ADDRESS_HOME_CITY, "תל אביב"},
                               {ADDRESS_HOME_STATE, "מחוז תל אביב"},
                               {ADDRESS_HOME_ZIP, "64332"}},
                              AutofillProfileCharacterScriptType::kHebrew},
        SingleProfileTestCase{
            "Latin1Supplement_German",
            "DE",
            {{NAME_FULL, "Jürgen Groß"},
             {ADDRESS_HOME_STREET_ADDRESS, "Goethestraße 5"},
             {ADDRESS_HOME_CITY, "München"}},
            AutofillProfileCharacterScriptType::kLatin1Supplement},
        SingleProfileTestCase{
            "LatinExtended_Czech",
            "CZ",
            {{NAME_FULL, "Dvořák Čech"},
             {ADDRESS_HOME_STREET_ADDRESS, "Náměstí Míru 3"},
             {ADDRESS_HOME_CITY, "Praha"}},
            AutofillProfileCharacterScriptType::kLatinExtended},
        SingleProfileTestCase{
            "Indic_Tamil",
            "IN",
            {{NAME_FULL, "அருண் குமார்"}, {ADDRESS_HOME_CITY, "சென்னை"}},
            AutofillProfileCharacterScriptType::kIndic},
        SingleProfileTestCase{"Thai",
                              "TH",
                              {{NAME_FULL, "สมชาย สุขใจ"},
                               {ADDRESS_HOME_STREET_ADDRESS, "ถนนสุขุมวิท 101"},
                               {ADDRESS_HOME_CITY, "พระโขนง"},
                               {ADDRESS_HOME_STATE, "กรุงเทพมหานคร"},
                               {ADDRESS_HOME_ZIP, "10260"}},
                              AutofillProfileCharacterScriptType::kThai},
        SingleProfileTestCase{
            "OtherNonLatin_Georgian",
            "GE",
            {{NAME_FULL, "გიორგი ბერიძე"},
             {ADDRESS_HOME_STREET_ADDRESS, "რუსთაველის გამზირი 12"},
             {ADDRESS_HOME_CITY, "თბილისი"},
             {ADDRESS_HOME_STATE, "თბილისი"},
             {ADDRESS_HOME_ZIP, "0108"}},
            AutofillProfileCharacterScriptType::kOtherNonLatin},
        SingleProfileTestCase{
            "NfdLatin1Supplement",
            "FR",
            {{NAME_FULL, "Franc\u0327ois Mu\u0308ller"},
             {ADDRESS_HOME_CITY, "Cafe\u0301"}},
            AutofillProfileCharacterScriptType::kLatin1Supplement},
        SingleProfileTestCase{
            "NfdLatinExtended",
            "CZ",
            {{NAME_FULL, "Dvor\u030Ca\u0301k C\u030Cech"}},
            AutofillProfileCharacterScriptType::kLatinExtended},
        SingleProfileTestCase{"NfdCyrillicWithCombiningMarks",
                              "UA",
                              {{NAME_FULL, "Іва́н Йосиф"}},
                              AutofillProfileCharacterScriptType::kCyrillic},
        SingleProfileTestCase{"NfdGreekWithCombiningMarks",
                              "GR",
                              {{NAME_FULL, "Νίκος"}},
                              AutofillProfileCharacterScriptType::kGreek},
        SingleProfileTestCase{
            "FullwidthLatinCharactersInCjkAddressIgnored",
            "JP",
            {{NAME_FULL, "山田 太郎"},
             {ADDRESS_HOME_STREET_ADDRESS, "六本木 6-10-1 Ａ棟"},
             {ADDRESS_HOME_CITY, "港区"},
             {ADDRESS_HOME_STATE, "東京都"},
             {ADDRESS_HOME_ZIP, "106-6108"}},
            AutofillProfileCharacterScriptType::kCjk},
        SingleProfileTestCase{"BopomofoCjk",
                              "TW",
                              {{NAME_FULL, "ㄅㄆㄇㄈ"}},
                              AutofillProfileCharacterScriptType::kCjk},
        SingleProfileTestCase{"EmptyProfile",
                              "US",
                              {},
                              AutofillProfileCharacterScriptType::kUnspecified},
        SingleProfileTestCase{"MixedScript_AsciiAndCjk",
                              "JP",
                              {{NAME_FULL, "John Doe"},
                               {ADDRESS_HOME_STREET_ADDRESS, "六本木 6-10-1"},
                               {ADDRESS_HOME_CITY, "港区"},
                               {ADDRESS_HOME_STATE, "東京都"},
                               {ADDRESS_HOME_ZIP, "106-6108"}},
                              AutofillProfileCharacterScriptType::kMixed},
        SingleProfileTestCase{
            "Unspecified_DigitsAndSymbolsOnly",
            "US",
            {{NAME_FULL, "12345"},
             {ADDRESS_HOME_STREET_ADDRESS, "#42-10"},
             {ADDRESS_HOME_CITY, "90210"},
             {ADDRESS_HOME_STATE, "11"},
             {ADDRESS_HOME_ZIP, "90210"}},
            AutofillProfileCharacterScriptType::kUnspecified}),
    [](const testing::TestParamInfo<SingleProfileTestCase>& info) {
      return info.param.test_name;
    });

// Tests emitting stored profile script type count metrics across multiple
// profiles.
TEST(StoredProfileMetricsTest, ScriptTypeCountMetrics) {
  AutofillProfile ascii_profile = test::StandardProfile();
  AutofillProfile latin1_profile{AddressCountryCode("FR")};
  test::SetProfileTestValues(&latin1_profile, {{NAME_FULL, "François Müller"},
                                               {ADDRESS_HOME_CITY, "Paris"}});
  AutofillProfile latin_extended_profile{AddressCountryCode("PL")};
  test::SetProfileTestValues(
      &latin_extended_profile,
      {{NAME_FULL, "Przemysław Łódź"}, {ADDRESS_HOME_CITY, "Warszawa"}});

  base::HistogramTester histogram_tester;
  LogStoredProfileScriptTypeMetrics(
      {ascii_profile, latin1_profile, latin_extended_profile});

  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfile.CharacterScriptType", 3);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCountByScriptType.AsciiOnly", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCountByScriptType.Latin1Supplement", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCountByScriptType.LatinExtended", 1, 1);
}

}  // namespace autofill::autofill_metrics
