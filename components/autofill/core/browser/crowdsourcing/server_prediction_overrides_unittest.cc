// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/server_prediction_overrides.h"

#include <string_view>
#include <utility>

#include "base/base64.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/signatures.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::Matcher;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::UnorderedElementsAre;

using FieldSuggestion = AutofillQueryResponse::FormSuggestion::FieldSuggestion;
using FieldPrediction = FieldSuggestion::FieldPrediction;

Matcher<FieldPrediction> EqualsPrediction(int prediction) {
  return AllOf(Property("override", &FieldPrediction::override, true),
               Property("type", &FieldPrediction::type, prediction),
               Property("source", &FieldPrediction::source,
                        FieldPrediction::SOURCE_MANUAL_OVERRIDE));
}

Matcher<FieldSuggestion> HasPredictions(auto predictions_matcher) {
  return Property("predictions", &FieldSuggestion::predictions,
                  std::move(predictions_matcher));
}

Matcher<FieldSuggestion> HasFormatString(
    Matcher<std::optional<std::pair<FormatString_Type, std::string>>>
        optional_format_string_matcher) {
  return ResultOf(
      "format_string",
      [](const FieldSuggestion& fs)
          -> std::optional<std::pair<FormatString_Type, std::string>> {
        if (!fs.has_format_string()) {
          return std::nullopt;
        }
        return std::pair(fs.format_string().type(),
                         fs.format_string().format_string());
      },
      std::move(optional_format_string_matcher));
}

TEST(ServerPredictionOverridesTest, AcceptsEmptyInput) {
  auto result = ParseServerPredictionOverrides("", OverrideFormat::kSpec);
  EXPECT_TRUE(result.has_value()) << result.error();

  result = ParseServerPredictionOverrides("--", OverrideFormat::kSpec);
  EXPECT_FALSE(result.has_value());
  EXPECT_THAT(
      result.error(),
      StartsWith("expected string of form formsignature_fieldsignature"));
}

TEST(ServerPredictionOverridesTest, DoesNotAcceptMalformedFormSignatures) {
  constexpr std::string_view kSampleInput1 = "10011880710453506489234_123_2";
  constexpr std::string_view kSampleInput2 = "2342343a_123_4";

  auto result =
      ParseServerPredictionOverrides(kSampleInput1, OverrideFormat::kSpec);
  EXPECT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), StartsWith("unable to parse form signature"));

  result = ParseServerPredictionOverrides(kSampleInput2, OverrideFormat::kSpec);
  EXPECT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), StartsWith("unable to parse form signature"));
}

TEST(ServerPredictionOverridesTest, DoesNotAcceptMalformedFieldSignatures) {
  constexpr std::string_view kSampleInput1 = "10011880710453534_asd_5";
  constexpr std::string_view kSampleInput2 = "10011880710453534_123123123123_5";

  auto result =
      ParseServerPredictionOverrides(kSampleInput1, OverrideFormat::kSpec);
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), StartsWith("unable to parse field signature"));

  result = ParseServerPredictionOverrides(kSampleInput2, OverrideFormat::kSpec);
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), StartsWith("unable to parse field signature"));
}

TEST(ServerPredictionOverridesTest, RequiresOverridesToHaveAtLeastTwoEntries) {
  constexpr std::string_view kSampleInput1 = "10011880710453534_234_3-2";
  constexpr std::string_view kSampleInput2 = "asd";

  auto result =
      ParseServerPredictionOverrides(kSampleInput1, OverrideFormat::kSpec);
  EXPECT_FALSE(result.has_value());
  EXPECT_THAT(
      result.error(),
      StartsWith("expected string of form formsignature_fieldsignature"));

  result = ParseServerPredictionOverrides(kSampleInput2, OverrideFormat::kSpec);
  EXPECT_FALSE(result.has_value());
  EXPECT_THAT(
      result.error(),
      StartsWith("expected string of form formsignature_fieldsignature"));
}

TEST(ServerPredictionOverridesTest, DoesNotAcceptMalformedFieldPredictions) {
  constexpr std::string_view kSampleInput1 = "10011880710453534_456545_a";
  constexpr std::string_view kSampleInput2 =
      "2342343465465_123123123_3456565656556565";

  auto result =
      ParseServerPredictionOverrides(kSampleInput1, OverrideFormat::kSpec);
  EXPECT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), StartsWith("unable to parse field prediction"));

  result = ParseServerPredictionOverrides(kSampleInput2, OverrideFormat::kSpec);
  EXPECT_FALSE(result.has_value());
  EXPECT_THAT(result.error(), StartsWith("unable to parse field prediction"));
}

TEST(ServerPredictionOverridesTest, ParsesWellFormedOverridesCorrectly) {
  constexpr std::string_view kSampleInput =
      "10011880710453506489_1654523497_3-10011880710453506489_1564345998_5-"
      "10011880710453506489_1900909900_9_100-13453454354354545026_2001230230_7";

  constexpr FormSignature kFormSignature1 =
      FormSignature(10011880710453506489ul);
  constexpr FormSignature kFormSignature2 =
      FormSignature(13453454354354545026ul);
  constexpr std::pair<FormSignature, FieldSignature> kOverrides[] = {
      std::make_pair(kFormSignature1, FieldSignature(1654523497u)),
      std::make_pair(kFormSignature1, FieldSignature(1564345998u)),
      std::make_pair(kFormSignature1, FieldSignature(1900909900u)),
      std::make_pair(kFormSignature2, FieldSignature(2001230230u))};

  auto result =
      ParseServerPredictionOverrides(kSampleInput, OverrideFormat::kSpec);
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_THAT(result.value(), SizeIs(4));
  ServerPredictionOverrides overrides = result.value();
  EXPECT_THAT(overrides,
              UnorderedElementsAre(Key(kOverrides[0]), Key(kOverrides[1]),
                                   Key(kOverrides[2]), Key(kOverrides[3])));

  // General sanity checks.
  for (const std::pair<FormSignature, FieldSignature>& override : kOverrides) {
    ASSERT_THAT(overrides.at(override), SizeIs(1));
    EXPECT_EQ(FieldSignature(overrides.at(override).front().field_signature()),
              override.second);
  }

  // Prediction content checks.
  EXPECT_THAT(overrides.at(kOverrides[0]).front().predictions(),
              ElementsAre(EqualsPrediction(3)));
  EXPECT_THAT(overrides.at(kOverrides[1]).front().predictions(),
              ElementsAre(EqualsPrediction(5)));
  EXPECT_THAT(overrides.at(kOverrides[2]).front().predictions(),
              ElementsAre(EqualsPrediction(9), EqualsPrediction(100)));
  EXPECT_THAT(overrides.at(kOverrides[3]).front().predictions(),
              ElementsAre(EqualsPrediction(7)));
}

TEST(ServerPredictionOverridesTest, AcceptsIdenticalFormAndFieldSignatures) {
  constexpr std::string_view kSampleInput =
      "10011880710453506489_1654523497_3-10011880710453506489_1654523497_8";

  constexpr FormSignature kFormSignature =
      FormSignature(10011880710453506489ul);
  constexpr FieldSignature kFieldSignature = FieldSignature(1654523497u);
  constexpr auto kOverride = std::make_pair(kFormSignature, kFieldSignature);

  auto result =
      ParseServerPredictionOverrides(kSampleInput, OverrideFormat::kSpec);
  EXPECT_TRUE(result.has_value()) << result.error();
  ServerPredictionOverrides overrides = result.value();
  EXPECT_THAT(overrides, ElementsAre(Key(kOverride)));

  // General sanity checks
  ASSERT_THAT(overrides.at(kOverride), SizeIs(2));
  EXPECT_EQ(FieldSignature(overrides.at(kOverride)[0].field_signature()),
            kFieldSignature);
  EXPECT_EQ(FieldSignature(overrides.at(kOverride)[1].field_signature()),
            kFieldSignature);

  // Prediction content checks.
  EXPECT_THAT(overrides.at(kOverride),
              ElementsAre(Property(&FieldSuggestion::predictions,
                                   ElementsAre(EqualsPrediction(3))),
                          Property(&FieldSuggestion::predictions,
                                   ElementsAre(EqualsPrediction(8)))));
}

TEST(ServerPredictionOverridesTest, AcceptsMissingPredictionFields) {
  constexpr std::string_view kSampleInput =
      "10011880710453506489_1654523497_3-10011880710453506489_1654523497_8-"
      "10011880710453506489_1654523497";

  constexpr FormSignature kFormSignature =
      FormSignature(10011880710453506489ul);
  constexpr FieldSignature kFieldSignature = FieldSignature(1654523497u);
  constexpr auto kOverride = std::make_pair(kFormSignature, kFieldSignature);

  auto result =
      ParseServerPredictionOverrides(kSampleInput, OverrideFormat::kSpec);
  EXPECT_TRUE(result.has_value()) << result.error();
  ServerPredictionOverrides overrides = result.value();
  EXPECT_THAT(overrides, ElementsAre(Key(kOverride)));

  // General sanity checks
  ASSERT_THAT(overrides.at(kOverride), SizeIs(3));
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(FieldSignature(overrides.at(kOverride)[i].field_signature()),
              kFieldSignature);
  }

  // Prediction content checks.
  EXPECT_THAT(overrides.at(kOverride),
              ElementsAre(Property(&FieldSuggestion::predictions,
                                   ElementsAre(EqualsPrediction(3))),
                          Property(&FieldSuggestion::predictions,
                                   ElementsAre(EqualsPrediction(8))),
                          Property(&FieldSuggestion::predictions, IsEmpty())));
}

TEST(ServerPredictionOverridesTest, Json) {
  static constexpr char kOriginalJson[] = R"(
    {
      "12345": {
        "123": [
          { "predictions": ["PASSPORT_NUMBER"] },
          { "predictions": ["PASSPORT_EXPIRATION_DATE"],
            "format_string_type": "DATE",
            "format_string": "DD/MM/YYYY" }
        ]
      },
      "67890": {
        "123": [
          { "predictions": ["NAME_FIRST", "PASSPORT_NAME_TAG"] },
          { "predictions": ["NAME_LAST", "PASSPORT_NAME_TAG"] }
        ],
        "456": [
          { "predictions": ["ADDRESS_HOME_COUNTRY", 170] }
        ]
      }
    }
  )";
  std::string base64_json = base::Base64Encode(kOriginalJson);
  base::expected<ServerPredictionOverrides, std::string> expected_overrides =
      ParseServerPredictionOverrides(base64_json, OverrideFormat::kJson);
  ASSERT_TRUE(expected_overrides.has_value());
  ServerPredictionOverrides& overrides = expected_overrides.value();

  auto form_and_field = [](uint64_t form_signature, uint64_t field_signature) {
    return std::pair(FormSignature(form_signature),
                     FieldSignature(field_signature));
  };

  EXPECT_THAT(
      overrides,
      ElementsAre(
          Pair(form_and_field(12345, 123),
               ElementsAre(AllOf(HasPredictions(ElementsAre(
                                     EqualsPrediction(PASSPORT_NUMBER))),
                                 HasFormatString(Eq(std::nullopt))),
                           AllOf(HasPredictions(ElementsAre(EqualsPrediction(
                                     PASSPORT_EXPIRATION_DATE))),
                                 HasFormatString(Optional(Pair(
                                     FormatString_Type_DATE, "DD/MM/YYYY")))))),
          Pair(form_and_field(67890, 123),
               ElementsAre(AllOf(HasPredictions(ElementsAre(
                                     EqualsPrediction(NAME_FIRST),
                                     EqualsPrediction(PASSPORT_NAME_TAG))),
                                 HasFormatString(Eq(std::nullopt))),
                           AllOf(HasPredictions(ElementsAre(
                                     EqualsPrediction(NAME_LAST),
                                     EqualsPrediction(PASSPORT_NAME_TAG))),
                                 HasFormatString(Eq(std::nullopt))))),
          Pair(form_and_field(67890, 456),
               ElementsAre(
                   AllOf(HasPredictions(ElementsAre(
                             EqualsPrediction(ADDRESS_HOME_COUNTRY),
                             EqualsPrediction(PASSPORT_ISSUING_COUNTRY))),
                         HasFormatString(Eq(std::nullopt)))))));
}

}  // namespace
}  // namespace autofill
