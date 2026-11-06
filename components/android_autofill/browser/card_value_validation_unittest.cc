// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/card_value_validation.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class CardValueValidationTest : public testing::Test {
 public:
  // Pins the current year to 2026, so that years from 2026 to 2046 are
  // plausible.
  CardValueValidationTest() { AdvanceToYear(2026); }

 protected:
  // Advances the mock clock to noon on June 15 of `year`, local time.
  void AdvanceToYear(int year) {
    base::Time target;
    CHECK(base::Time::FromLocalExploded(
        {.year = year, .month = 6, .day_of_month = 15, .hour = 12}, &target));
    task_environment_.AdvanceClock(target - base::Time::Now());
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

bool IsPlausible(FieldType type,
                 std::u16string_view value,
                 bool is_select_element = false) {
  return IsPlausibleNonSensitiveCreditCardValue(type, value, is_select_element);
}

TEST_F(CardValueValidationTest, Names) {
  for (FieldType type :
       {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NAME_LAST}) {
    SCOPED_TRACE(FieldTypeToStringView(type));
    EXPECT_TRUE(IsPlausible(type, u"Jane Doe"));
    EXPECT_TRUE(IsPlausible(type, u"José O'Brien-Núñez"));
    EXPECT_TRUE(IsPlausible(type, std::u16string(100, u'a')));
    EXPECT_FALSE(IsPlausible(type, u""));
    EXPECT_FALSE(IsPlausible(type, u"  "));
    EXPECT_FALSE(IsPlausible(type, u"4111111111111111"));
    EXPECT_FALSE(IsPlausible(type, u"Jane 4111"));
    EXPECT_FALSE(IsPlausible(type, u"123"));
    EXPECT_FALSE(IsPlausible(type, std::u16string(101, u'a')));
    // IsValidNameOnCard() returns true above MatchesRegex()'s 5000-character
    // limit, even with digits.
    EXPECT_FALSE(IsPlausible(type, std::u16string(5001, u'1')));
  }
}

TEST_F(CardValueValidationTest, Months) {
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"1"));
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"01"));
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"12"));
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_MONTH, u" 9 "));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u""));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"0"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"00"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"13"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"+1"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"-1"));
  // Month names aren't supported, since any value without digits, e.g. a
  // password, would pass as one.
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"Jan"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"Dezember"));
  // Values with more than 2 digits, like CVCs, aren't months, even with
  // leading zeros.
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"007"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"4111"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"Jan 1"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_MONTH, u"12/27"));
}

TEST_F(CardValueValidationTest, Years) {
  for (FieldType type :
       {CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_EXP_4_DIGIT_YEAR}) {
    SCOPED_TRACE(FieldTypeToStringView(type));
    EXPECT_TRUE(IsPlausible(type, u"26"));
    EXPECT_TRUE(IsPlausible(type, u"46"));
    EXPECT_FALSE(IsPlausible(type, u"25"));
    EXPECT_FALSE(IsPlausible(type, u"47"));
    EXPECT_FALSE(IsPlausible(type, u"6"));
    EXPECT_FALSE(IsPlausible(type, u"026"));
    EXPECT_FALSE(IsPlausible(type, u"Jan"));
    // 4-digit years are only plausible in <select> elements, since CVCs can
    // have 4 digits.
    EXPECT_FALSE(IsPlausible(type, u"2030"));
    EXPECT_TRUE(IsPlausible(type, u"2026", /*is_select_element=*/true));
    EXPECT_TRUE(IsPlausible(type, u"2046", /*is_select_element=*/true));
    EXPECT_TRUE(IsPlausible(type, u"30", /*is_select_element=*/true));
    EXPECT_FALSE(IsPlausible(type, u"2025", /*is_select_element=*/true));
    EXPECT_FALSE(IsPlausible(type, u"2047", /*is_select_element=*/true));
    EXPECT_FALSE(IsPlausible(type, u"0026", /*is_select_element=*/true));
    EXPECT_FALSE(IsPlausible(type, u"20300", /*is_select_element=*/true));
  }
}

// Tests that 2-digit years count from the current year across centuries.
TEST_F(CardValueValidationTest, TwoDigitYearsAcrossCenturies) {
  AdvanceToYear(2090);
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_2_DIGIT_YEAR, u"90"));
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_2_DIGIT_YEAR, u"99"));
  EXPECT_TRUE(IsPlausible(CREDIT_CARD_EXP_2_DIGIT_YEAR, u"10"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_2_DIGIT_YEAR, u"11"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_EXP_2_DIGIT_YEAR, u"89"));
}

TEST_F(CardValueValidationTest, ExpirationDates) {
  for (FieldType type :
       {CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR}) {
    SCOPED_TRACE(FieldTypeToStringView(type));
    EXPECT_TRUE(IsPlausible(type, u"12/27"));
    EXPECT_TRUE(IsPlausible(type, u"1/27"));
    EXPECT_TRUE(IsPlausible(type, u"12 / 27"));
    EXPECT_TRUE(IsPlausible(type, u"12-27"));
    EXPECT_TRUE(IsPlausible(type, u"12/2027"));
    // A separator is required, so that a 4-digit CVC can't pass as MMYY.
    EXPECT_FALSE(IsPlausible(type, u"1227"));
    EXPECT_FALSE(IsPlausible(type, u"122027"));
    EXPECT_FALSE(IsPlausible(type, u"00/27"));
    EXPECT_FALSE(IsPlausible(type, u"13/27"));
    EXPECT_FALSE(IsPlausible(type, u"012/27"));
    EXPECT_FALSE(IsPlausible(type, u"12/25"));
    EXPECT_FALSE(IsPlausible(type, u"12/2047"));
    EXPECT_FALSE(IsPlausible(type, u"12/27/30"));
    EXPECT_FALSE(IsPlausible(type, u"/27"));
    EXPECT_FALSE(IsPlausible(type, u"12/"));
  }
}

TEST_F(CardValueValidationTest, OtherTypes) {
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_NUMBER, u"4111111111111111"));
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_VERIFICATION_CODE, u"123"));
  // Android Autofill services don't fill card types.
  EXPECT_FALSE(IsPlausible(CREDIT_CARD_TYPE, u"Visa"));
  EXPECT_FALSE(IsPlausible(NAME_FULL, u"Jane Doe"));
  EXPECT_FALSE(IsPlausible(UNKNOWN_TYPE, u"12"));
}

class GetPlausibleNonSensitiveCreditCardTypesTest
    : public CardValueValidationTest {
 protected:
  // Returns the types for a fill of all fields of `form`, whose fields Autofill
  // classified as `types`.
  absl::flat_hash_map<FieldGlobalId, FieldType> GetTypes(
      const FormData& form,
      const std::vector<FieldType>& types,
      std::optional<url::Origin> triggered_origin = std::nullopt) {
    FormStructure form_structure(form);
    test_api(form_structure).SetFieldTypes(types);
    return GetPlausibleNonSensitiveCreditCardTypes(
        form_structure, form.fields(),
        triggered_origin.value_or(iframe_origin_));
  }

  const url::Origin main_origin_ =
      url::Origin::Create(GURL("https://merchant.test"));
  const url::Origin iframe_origin_ =
      url::Origin::Create(GURL("https://psp.test"));

 private:
  test::AutofillUnitTestEnvironment autofill_environment_;
};

constexpr char kValidationHistogram[] =
    "Autofill.WebView.CrossOriginMainFrameCreditCardFieldValidation";

// Tests that only the cardholder name and expiration date fields in the main
// frame's origin get a type.
TEST_F(GetPlausibleNonSensitiveCreditCardTypesTest, MainFrameNameAndExpiry) {
  base::HistogramTester histogram_tester;
  FormData form = test::GetFormData(
      {.fields = {{.value = u"Jane Doe", .origin = main_origin_},
                  {.value = u"4111111111111111", .origin = iframe_origin_},
                  {.value = u"12", .origin = main_origin_},
                  {.value = u"30", .origin = main_origin_},
                  {.value = u"123", .origin = iframe_origin_},
                  {.value = u"Jane Doe", .origin = iframe_origin_}},
       .main_frame_origin = main_origin_});
  EXPECT_THAT(
      GetTypes(form, {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR,
                      CREDIT_CARD_VERIFICATION_CODE, CREDIT_CARD_NAME_FULL}),
      UnorderedElementsAre(
          Pair(form.fields()[0].global_id(), CREDIT_CARD_NAME_FULL),
          Pair(form.fields()[2].global_id(), CREDIT_CARD_EXP_MONTH),
          Pair(form.fields()[3].global_id(), CREDIT_CARD_EXP_2_DIGIT_YEAR)));
  histogram_tester.ExpectUniqueSample(
      kValidationHistogram, MainFrameCreditCardFieldValidationResult::kAllowed,
      3);
}

// Tests that card numbers, CVCs and other types in the main frame don't get a
// type.
TEST_F(GetPlausibleNonSensitiveCreditCardTypesTest, OtherTypesInMainFrame) {
  base::HistogramTester histogram_tester;
  FormData form = test::GetFormData(
      {.fields = {{.value = u"4111111111111111", .origin = main_origin_},
                  {.value = u"123", .origin = main_origin_},
                  {.value = u"Jane Doe", .origin = main_origin_},
                  {.value = u"4111111111111111", .origin = iframe_origin_}},
       .main_frame_origin = main_origin_});
  EXPECT_THAT(GetTypes(form, {CREDIT_CARD_NUMBER, CREDIT_CARD_VERIFICATION_CODE,
                              NAME_FULL, CREDIT_CARD_NUMBER}),
              IsEmpty());
  histogram_tester.ExpectTotalCount(kValidationHistogram, 0);
}

// Tests that fields whose values don't look like their type don't get a type.
TEST_F(GetPlausibleNonSensitiveCreditCardTypesTest, ImplausibleValues) {
  base::HistogramTester histogram_tester;
  FormData form = test::GetFormData(
      {.fields = {{.value = u"4111111111111111", .origin = main_origin_},
                  {.value = u"2030", .origin = main_origin_},
                  {.value = u"2030",
                   .form_control_type = FormControlType::kSelectOne,
                   .origin = main_origin_,
                   .select_options = {{{.value = u"2030", .text = u"2030"}}}},
                  {.value = u"4111111111111111", .origin = iframe_origin_}},
       .main_frame_origin = main_origin_});
  // A 4-digit year only gets a type in a <select> element.
  EXPECT_THAT(
      GetTypes(form, {CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_4_DIGIT_YEAR,
                      CREDIT_CARD_EXP_4_DIGIT_YEAR, CREDIT_CARD_NUMBER}),
      UnorderedElementsAre(
          Pair(form.fields()[2].global_id(), CREDIT_CARD_EXP_4_DIGIT_YEAR)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kValidationHistogram),
      base::BucketsAre(
          base::Bucket(MainFrameCreditCardFieldValidationResult::kAllowed, 1),
          base::Bucket(
              MainFrameCreditCardFieldValidationResult::kImplausibleValue, 2)));
}

// Tests that fills triggered from the main frame itself return an empty map and
// do not record cross-origin validation metrics.
TEST_F(GetPlausibleNonSensitiveCreditCardTypesTest,
       SameOriginTriggerReturnsEmpty) {
  base::HistogramTester histogram_tester;
  FormData form = test::GetFormData(
      {.fields = {{.value = u"Jane Doe", .origin = main_origin_},
                  {.value = u"12", .origin = main_origin_},
                  {.value = u"30", .origin = main_origin_},
                  {.value = u"4111111111111111", .origin = main_origin_}},
       .main_frame_origin = main_origin_});
  EXPECT_THAT(GetTypes(form,
                       {CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_MONTH,
                        CREDIT_CARD_EXP_2_DIGIT_YEAR, CREDIT_CARD_NUMBER},
                       main_origin_),
              IsEmpty());
  histogram_tester.ExpectTotalCount(kValidationHistogram, 0);
}

// Tests that field properties are checked on `filled_fields` rather than on the
// potentially outdated `FormStructure`.
TEST_F(GetPlausibleNonSensitiveCreditCardTypesTest,
       ChecksFilledFieldsInsteadOfCachedFormStructure) {
  base::HistogramTester histogram_tester;
  FormData cached_form = test::GetFormData(
      {.fields = {{.origin = main_origin_},
                  {.origin = iframe_origin_},
                  {.form_control_type = FormControlType::kSelectOne,
                   .origin = main_origin_,
                   .select_options = {{{.value = u"12", .text = u"December"}}}},
                  {.origin = iframe_origin_}},
       .main_frame_origin = main_origin_});
  FormStructure form_structure(cached_form);
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NAME_LAST,
                      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_NUMBER});

  FormData filled_form = cached_form;
  test_api(filled_form).field(0).set_origin(iframe_origin_);
  test_api(filled_form).field(0).set_value(u"Jane");
  test_api(filled_form).field(1).set_origin(main_origin_);
  test_api(filled_form).field(1).set_value(u"Doe");
  test_api(filled_form).field(2).set_options({{.value = u"12", .text = u"12"}});
  test_api(filled_form).field(2).set_value(u"12");
  test_api(filled_form).field(3).set_value(u"4111111111111111");

  EXPECT_THAT(
      GetPlausibleNonSensitiveCreditCardTypes(
          form_structure, filled_form.fields(), iframe_origin_),
      UnorderedElementsAre(
          Pair(filled_form.fields()[1].global_id(), CREDIT_CARD_NAME_LAST),
          Pair(filled_form.fields()[2].global_id(), CREDIT_CARD_EXP_MONTH)));
  histogram_tester.ExpectUniqueSample(
      kValidationHistogram, MainFrameCreditCardFieldValidationResult::kAllowed,
      2);
}

// Tests that at most one month, one year and one expiration date field get a
// type, while names aren't limited.
TEST_F(GetPlausibleNonSensitiveCreditCardTypesTest,
       AtMostOneFieldPerExpiryType) {
  base::HistogramTester histogram_tester;
  FormData form = test::GetFormData(
      {.fields = {{.value = u"Jane", .origin = main_origin_},
                  {.value = u"Doe", .origin = main_origin_},
                  {.value = u"13", .origin = main_origin_},
                  {.value = u"12", .origin = main_origin_},
                  {.value = u"11", .origin = main_origin_},
                  {.value = u"30", .origin = main_origin_},
                  {.value = u"31", .origin = main_origin_},
                  {.value = u"12/30", .origin = main_origin_},
                  {.value = u"11/31", .origin = main_origin_},
                  {.value = u"4111111111111111", .origin = iframe_origin_}},
       .main_frame_origin = main_origin_});
  // The first month fails the check, so the second one gets the type.
  EXPECT_THAT(
      GetTypes(form,
               {CREDIT_CARD_NAME_FIRST, CREDIT_CARD_NAME_LAST,
                CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_MONTH,
                CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR,
                CREDIT_CARD_EXP_4_DIGIT_YEAR, CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, CREDIT_CARD_NUMBER}),
      UnorderedElementsAre(
          Pair(form.fields()[0].global_id(), CREDIT_CARD_NAME_FIRST),
          Pair(form.fields()[1].global_id(), CREDIT_CARD_NAME_LAST),
          Pair(form.fields()[3].global_id(), CREDIT_CARD_EXP_MONTH),
          Pair(form.fields()[5].global_id(), CREDIT_CARD_EXP_2_DIGIT_YEAR),
          Pair(form.fields()[7].global_id(),
               CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kValidationHistogram),
      base::BucketsAre(
          base::Bucket(MainFrameCreditCardFieldValidationResult::kAllowed, 5),
          base::Bucket(
              MainFrameCreditCardFieldValidationResult::kImplausibleValue, 1),
          base::Bucket(MainFrameCreditCardFieldValidationResult::
                           kDuplicateExpirationPart,
                       3)));
}

// Tests that a <select> element only gets a type if it has an option with the
// filled value and all such options have a plausible text, since the service
// may have picked the option by its text.
TEST_F(GetPlausibleNonSensitiveCreditCardTypesTest, SelectOptionTexts) {
  struct TestCase {
    FieldType type;
    std::u16string value;
    std::vector<SelectOption> options;
    bool gets_type;
  };
  const TestCase kTestCases[] = {
      {CREDIT_CARD_EXP_MONTH, u"12", {{.value = u"12", .text = u"12"}}, true},
      {CREDIT_CARD_EXP_4_DIGIT_YEAR,
       u"2030",
       {{.value = u"2030", .text = u"2030"}},
       true},
      // Month names aren't plausible months.
      {CREDIT_CARD_EXP_MONTH,
       u"12",
       {{.value = u"12", .text = u"December"}},
       false},
      // A crafted option may pair a CVC text with a plausible value.
      {CREDIT_CARD_EXP_4_DIGIT_YEAR,
       u"2030",
       {{.value = u"2030", .text = u"1234"}},
       false},
      {CREDIT_CARD_EXP_4_DIGIT_YEAR,
       u"2030",
       {{.value = u"2030", .text = u"2030"},
        {.value = u"2030", .text = u"123"}},
       false},
      {CREDIT_CARD_EXP_4_DIGIT_YEAR,
       u"2030",
       {{.value = u"2031", .text = u"2031"}},
       false},
  };
  for (size_t i = 0; const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Test case " << i++);
    base::HistogramTester histogram_tester;
    FormData form = test::GetFormData(
        {.fields = {{.value = test_case.value,
                     .form_control_type = FormControlType::kSelectOne,
                     .origin = main_origin_,
                     .select_options = test_case.options},
                    {.value = u"4111111111111111", .origin = iframe_origin_}},
         .main_frame_origin = main_origin_});
    EXPECT_EQ(GetTypes(form, {test_case.type, CREDIT_CARD_NUMBER}).size(),
              test_case.gets_type ? 1u : 0u);
    histogram_tester.ExpectUniqueSample(
        kValidationHistogram,
        test_case.gets_type ? MainFrameCreditCardFieldValidationResult::kAllowed
                            : MainFrameCreditCardFieldValidationResult::
                                  kImplausibleSelectOptionText,
        1);
  }
}

// Tests that the fill must put a valid card number into a card number field of
// `triggered_origin` before any main-frame field gets a type.
TEST_F(GetPlausibleNonSensitiveCreditCardTypesTest,
       RequiresValidCardNumberFillInTriggeredOrigin) {
  const url::Origin other_iframe_origin =
      url::Origin::Create(GURL("https://other.test"));

  // A card number field in the main frame or in a different iframe is not
  // sufficient when `other_iframe_origin` triggered the fill.
  {
    base::HistogramTester histogram_tester;
    FormData form = test::GetFormData(
        {.fields = {{.value = u"Jane Doe", .origin = main_origin_},
                    {.value = u"4111111111111111", .origin = main_origin_},
                    {.value = u"4111111111111111", .origin = iframe_origin_},
                    {.value = u"Jane Doe", .origin = other_iframe_origin}},
         .main_frame_origin = main_origin_});
    EXPECT_THAT(GetTypes(form,
                         {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                          CREDIT_CARD_NUMBER, USERNAME},
                         other_iframe_origin),
                IsEmpty());
    histogram_tester.ExpectTotalCount(kValidationHistogram, 0);
  }

  // Main-frame fields can make Autofill classify a field of another frame as
  // a card number, e.g. a username field labeled "Acct #" next to a cardholder
  // name field. A value that isn't a valid card number is not sufficient.
  for (std::u16string_view value : {u"JaneDoe", u"4111111111111112", u"4111"}) {
    SCOPED_TRACE(base::UTF16ToUTF8(value));
    base::HistogramTester histogram_tester;
    FormData form = test::GetFormData(
        {.fields = {{.value = u"JaneDoe", .origin = main_origin_},
                    {.value = std::u16string(value), .origin = iframe_origin_}},
         .main_frame_origin = main_origin_});
    EXPECT_THAT(GetTypes(form, {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER}),
                IsEmpty());
    histogram_tester.ExpectTotalCount(kValidationHistogram, 0);
  }

  // A CVC alone is not sufficient.
  {
    base::HistogramTester histogram_tester;
    FormData form = test::GetFormData(
        {.fields = {{.value = u"Jane Doe", .origin = main_origin_},
                    {.value = u"123", .origin = iframe_origin_}},
         .main_frame_origin = main_origin_});
    EXPECT_THAT(
        GetTypes(form, {CREDIT_CARD_NAME_FULL, CREDIT_CARD_VERIFICATION_CODE}),
        IsEmpty());
    histogram_tester.ExpectTotalCount(kValidationHistogram, 0);
  }

  // A card number field of `triggered_origin` that the fill doesn't fill is not
  // sufficient.
  {
    base::HistogramTester histogram_tester;
    FormData form = test::GetFormData(
        {.fields = {{.value = u"Jane Doe", .origin = main_origin_},
                    {.value = u"4111111111111111", .origin = iframe_origin_}},
         .main_frame_origin = main_origin_});
    FormStructure form_structure(form);
    test_api(form_structure)
        .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER});
    EXPECT_THAT(GetPlausibleNonSensitiveCreditCardTypes(
                    form_structure, base::span(form.fields()).first(1u),
                    iframe_origin_),
                IsEmpty());
    histogram_tester.ExpectTotalCount(kValidationHistogram, 0);
  }

  // Card numbers may contain separators.
  {
    base::HistogramTester histogram_tester;
    FormData form = test::GetFormData(
        {.fields = {{.value = u"Jane Doe", .origin = main_origin_},
                    {.value = u"4111 1111 1111 1111",
                     .origin = iframe_origin_}},
         .main_frame_origin = main_origin_});
    EXPECT_THAT(GetTypes(form, {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER}),
                UnorderedElementsAre(
                    Pair(form.fields()[0].global_id(), CREDIT_CARD_NAME_FULL)));
    histogram_tester.ExpectUniqueSample(
        kValidationHistogram,
        MainFrameCreditCardFieldValidationResult::kAllowed, 1);
  }
}

// Tests that main-frame password controls never receive cardholder name or
// expiration date types, based on the control types in `filled_fields` (i.e.
// what the service saw when the session started) rather than those in the
// possibly newer `FormStructure`, while unrelated main-frame password controls
// do not block sibling card fields.
TEST_F(GetPlausibleNonSensitiveCreditCardTypesTest,
       SkipsMainFramePasswordControls) {
  base::HistogramTester histogram_tester;
  FormData cached_form = test::GetFormData(
      {.fields = {{.form_control_type = FormControlType::kInputPassword,
                   .origin = main_origin_},
                  {.form_control_type = FormControlType::kInputPassword,
                   .origin = main_origin_},
                  {.form_control_type = FormControlType::kInputText,
                   .origin = main_origin_},
                  {.form_control_type = FormControlType::kInputPassword,
                   .origin = main_origin_},
                  {.origin = main_origin_},
                  {.origin = iframe_origin_}},
       .main_frame_origin = main_origin_});
  FormStructure form_structure(cached_form);
  test_api(form_structure)
      .SetFieldTypes({CREDIT_CARD_NAME_FULL, CREDIT_CARD_NAME_FIRST,
                      CREDIT_CARD_NAME_LAST, PASSWORD, CREDIT_CARD_EXP_MONTH,
                      CREDIT_CARD_NUMBER});

  FormData filled_form = cached_form;
  test_api(filled_form).field(0).set_value(u"Jane Doe");
  // A text field when the session started, which only became a password
  // control in the newer `FormStructure`.
  test_api(filled_form)
      .field(1)
      .set_form_control_type(FormControlType::kInputText);
  test_api(filled_form).field(1).set_value(u"Jane");
  // A password control when the session started, which only became a text
  // field in the newer `FormStructure`.
  test_api(filled_form)
      .field(2)
      .set_form_control_type(FormControlType::kInputPassword);
  test_api(filled_form).field(2).set_value(u"Doe");
  // Pre-existing main-frame password field (e.g. account creation on checkout).
  test_api(filled_form).field(3).set_value(u"AccountPass1!");
  test_api(filled_form).field(4).set_value(u"12");
  test_api(filled_form).field(5).set_value(u"4111111111111111");

  EXPECT_THAT(
      GetPlausibleNonSensitiveCreditCardTypes(
          form_structure, filled_form.fields(), iframe_origin_),
      UnorderedElementsAre(
          Pair(filled_form.fields()[1].global_id(), CREDIT_CARD_NAME_FIRST),
          Pair(filled_form.fields()[4].global_id(), CREDIT_CARD_EXP_MONTH)));
  histogram_tester.ExpectUniqueSample(
      kValidationHistogram, MainFrameCreditCardFieldValidationResult::kAllowed,
      2);
}

// Tests that cross-origin fills containing a password control return an empty
// map unless the password control is a card number or CVC field whose value
// matches that type.
TEST_F(GetPlausibleNonSensitiveCreditCardTypesTest,
       RejectsCrossOriginPasswordFillsExceptMaskedCardFields) {
  // Non-card password field filled in the iframe.
  {
    base::HistogramTester histogram_tester;
    FormData form = test::GetFormData(
        {.fields = {{.value = u"Jane Doe", .origin = main_origin_},
                    {.value = u"4111111111111111", .origin = iframe_origin_},
                    {.value = u"SecretPassphrase",
                     .form_control_type = FormControlType::kInputPassword,
                     .origin = iframe_origin_}},
         .main_frame_origin = main_origin_});
    EXPECT_THAT(
        GetTypes(form, {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER, PASSWORD}),
        IsEmpty());
    histogram_tester.ExpectTotalCount(kValidationHistogram, 0);
  }

  // Password control classified as CVC, but filled with a non-CVC password.
  {
    base::HistogramTester histogram_tester;
    FormData form = test::GetFormData(
        {.fields = {{.value = u"Jane Doe", .origin = main_origin_},
                    {.value = u"4111111111111111", .origin = iframe_origin_},
                    {.value = u"Hunter2!",
                     .form_control_type = FormControlType::kInputPassword,
                     .origin = iframe_origin_}},
         .main_frame_origin = main_origin_});
    EXPECT_THAT(GetTypes(form, {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                                CREDIT_CARD_VERIFICATION_CODE}),
                IsEmpty());
    histogram_tester.ExpectTotalCount(kValidationHistogram, 0);
  }

  // Payment iframe masking card number and CVC with `kInputPassword`.
  {
    base::HistogramTester histogram_tester;
    FormData form = test::GetFormData(
        {.fields = {{.value = u"Jane Doe", .origin = main_origin_},
                    {.value = u"12/30", .origin = main_origin_},
                    {.value = u"4111111111111111",
                     .form_control_type = FormControlType::kInputPassword,
                     .origin = iframe_origin_},
                    {.value = u"123",
                     .form_control_type = FormControlType::kInputPassword,
                     .origin = iframe_origin_}},
         .main_frame_origin = main_origin_});
    EXPECT_THAT(
        GetTypes(form,
                 {CREDIT_CARD_NAME_FULL, CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                  CREDIT_CARD_NUMBER, CREDIT_CARD_VERIFICATION_CODE}),
        UnorderedElementsAre(
            Pair(form.fields()[0].global_id(), CREDIT_CARD_NAME_FULL),
            Pair(form.fields()[1].global_id(),
                 CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR)));
    histogram_tester.ExpectUniqueSample(
        kValidationHistogram,
        MainFrameCreditCardFieldValidationResult::kAllowed, 2);
  }
}

}  // namespace
}  // namespace autofill
