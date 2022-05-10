// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_answer_result.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/range/range.h"

namespace app_list {
namespace {

using Tag = ash::SearchResultTag;

class ClassicOmniboxAnswerResultTest : public testing::Test {
 public:
  ClassicOmniboxAnswerResultTest() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kProductivityLauncher);
  }
  ~ClassicOmniboxAnswerResultTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class OmniboxAnswerResultTest : public testing::Test {
 public:
  OmniboxAnswerResultTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kProductivityLauncher);
  }
  ~OmniboxAnswerResultTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

MATCHER_P(TagEquals, tag, "") {
  bool styles_match = arg.styles == tag.styles;
  bool range_match = arg.range == tag.range;
  return styles_match && range_match;
}

}  // namespace

TEST_F(ClassicOmniboxAnswerResultTest, ClassicCalculatorResult) {
  AutocompleteMatch match;
  match.type = AutocompleteMatchType::CALCULATOR;
  match.contents = u"2+2";
  match.description = u"4";

  OmniboxAnswerResult result(nullptr, nullptr, nullptr, match, u"query");
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kList);
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kOmnibox);
  EXPECT_EQ(result.metrics_type(), ash::OMNIBOX_CALCULATOR);
  EXPECT_EQ(result.title(), u"2+2");
  EXPECT_EQ(result.details(), u"4");
}

TEST_F(ClassicOmniboxAnswerResultTest, ClassicAnswerResult) {
  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text one\", \"tt\": 8 }], "
      "              \"at\": { \"t\": \"additional one\", \"tt\": 42 } } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"text two\", \"tt\": 5 }], "
      "              \"at\": { \"t\": \"additional two\", \"tt\": 6 } } } "
      "] }";
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  ASSERT_TRUE(value && value->is_dict());
  ASSERT_TRUE(SuggestionAnswer::ParseAnswer(value->GetDict(), u"-1", &answer));

  AutocompleteMatch match;
  match.answer = answer;
  match.contents = u"contents";

  OmniboxAnswerResult result(nullptr, nullptr, nullptr, match, u"query");
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kList);
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kOmnibox);
  EXPECT_EQ(result.metrics_type(), ash::OMNIBOX_ANSWER);
  EXPECT_EQ(result.title(), u"contents additional one");
  EXPECT_EQ(result.details(), u"text two additional two");
}

TEST_F(OmniboxAnswerResultTest, CalculatorResult) {
  AutocompleteMatch match;
  match.type = AutocompleteMatchType::CALCULATOR;
  match.contents = u"2+2";
  match.description = u"4";

  OmniboxAnswerResult result(nullptr, nullptr, nullptr, match, u"query");
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kOmnibox);
  EXPECT_EQ(result.metrics_type(), ash::OMNIBOX_CALCULATOR);

  ASSERT_EQ(result.title_text_vector().size(), 1);
  const auto& title = result.title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), u"2+2");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(result.details_text_vector().size(), 1);
  const auto& details = result.details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"4");
  EXPECT_TRUE(details.GetTextTags().empty());
}

TEST_F(OmniboxAnswerResultTest, CalculatorResultNoDescription) {
  AutocompleteMatch match;
  match.type = AutocompleteMatchType::CALCULATOR;
  match.contents = u"4";

  OmniboxAnswerResult result(nullptr, nullptr, nullptr, match, u"2+2");
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kOmnibox);
  EXPECT_EQ(result.metrics_type(), ash::OMNIBOX_CALCULATOR);

  ASSERT_EQ(result.title_text_vector().size(), 1);
  const auto& title = result.title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), u"2+2");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(result.details_text_vector().size(), 1);
  const auto& details = result.details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"4");
  EXPECT_TRUE(details.GetTextTags().empty());
}

TEST_F(OmniboxAnswerResultTest, WeatherResult) {
  // This comes from SuggestionAnswer::AnswerType::ANSWER_TYPE_WEATHER.
  const std::u16string kWeatherType = u"8";

  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text one\", \"tt\": 8 }], "
      "              \"at\": { \"t\": \"additional one\", \"tt\": 42 } } }, "
      "  { \"il\": { \"al\": \"accessibility label\", "
      "              \"t\": [{ \"t\": \"-5°C\", \"tt\": 8 }], "
      "              \"at\": { \"t\": \"additional two\", \"tt\": 42 } } } "
      "] }";
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  ASSERT_TRUE(value && value->is_dict());
  ASSERT_TRUE(
      SuggestionAnswer::ParseAnswer(value->GetDict(), kWeatherType, &answer));

  AutocompleteMatch match;
  match.answer = answer;
  match.contents = u"contents";
  match.description = u"description";

  OmniboxAnswerResult result(nullptr, nullptr, nullptr, match, u"query");
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kOmnibox);
  EXPECT_EQ(result.metrics_type(), ash::OMNIBOX_ANSWER);

  ASSERT_EQ(result.big_title_text_vector().size(), 1);
  const auto& big_title = result.big_title_text_vector()[0];
  ASSERT_EQ(big_title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(big_title.GetText(), u"-5");
  EXPECT_TRUE(big_title.GetTextTags().empty());

  ASSERT_EQ(result.big_title_superscript_text_vector().size(), 1);
  const auto& superscript = result.big_title_superscript_text_vector()[0];
  ASSERT_EQ(superscript.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(superscript.GetText(), u"°C");
  EXPECT_TRUE(big_title.GetTextTags().empty());

  ASSERT_EQ(result.title_text_vector().size(), 1);
  const auto& title = result.title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), u"accessibility label");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(result.details_text_vector().size(), 1);
  const auto& details = result.details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"additional two");
  EXPECT_TRUE(details.GetTextTags().empty());
}

TEST_F(OmniboxAnswerResultTest, AnswerResult) {
  // This comes from SuggestionAnswer::AnswerType::ANSWER_TYPE_FINANCE.
  const std::u16string kWeatherType = u"2";

  SuggestionAnswer answer;
  // Text tags ("tt") 5 and 6 are SuggestionAnswer::TextType::NEGATIVE and
  // SuggestionAnswer::TextType::POSITIVE respectively.
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text one\", \"tt\": 8 }], "
      "              \"at\": { \"t\": \"additional one\", \"tt\": 42 } } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"text two\", \"tt\": 5 }], "
      "              \"at\": { \"t\": \"additional two\", \"tt\": 6 } } } "
      "] }";
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  ASSERT_TRUE(value && value->is_dict());
  ASSERT_TRUE(
      SuggestionAnswer::ParseAnswer(value->GetDict(), kWeatherType, &answer));

  AutocompleteMatch match;
  match.answer = answer;
  match.contents = u"contents";
  match.description = u"description";

  OmniboxAnswerResult result(nullptr, nullptr, nullptr, match, u"query");
  EXPECT_EQ(result.display_type(), ash::SearchResultDisplayType::kAnswerCard);
  EXPECT_EQ(result.result_type(), ash::AppListSearchResultType::kOmnibox);
  EXPECT_EQ(result.metrics_type(), ash::OMNIBOX_ANSWER);

  const auto& title = result.title_text_vector();
  ASSERT_EQ(title.size(), 3);
  ASSERT_EQ(title[0].GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title[0].GetText(), u"contents");
  EXPECT_TRUE(title[0].GetTextTags().empty());

  ASSERT_EQ(title[1].GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title[1].GetText(), u" ");
  EXPECT_TRUE(title[1].GetTextTags().empty());

  ASSERT_EQ(title[2].GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title[2].GetText(), u"additional one");
  EXPECT_TRUE(title[2].GetTextTags().empty());

  // The NEGATIVE text type should have RED tags, and the POSITIVE text type
  // should have GREEN tags.
  const auto& details = result.details_text_vector();
  ASSERT_EQ(details.size(), 3);
  ASSERT_EQ(details[0].GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details[0].GetText(), u"text two");
  size_t length = details[0].GetText().length();
  EXPECT_THAT(details[0].GetTextTags(), testing::UnorderedElementsAre(TagEquals(
                                            Tag(Tag::Style::RED, 0, length))));

  ASSERT_EQ(details[1].GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details[1].GetText(), u" ");
  length = details[1].GetText().length();
  EXPECT_TRUE(details[1].GetTextTags().empty());

  ASSERT_EQ(details[2].GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details[2].GetText(), u"additional two");
  length = details[2].GetText().length();
  EXPECT_THAT(details[2].GetTextTags(),
              testing::UnorderedElementsAre(
                  TagEquals(Tag(Tag::Style::GREEN, 0, length))));
}

TEST_F(OmniboxAnswerResultTest, DictionaryResultMultiline) {
  // This comes from SuggestionAnswer::AnswerType::ANSWER_TYPE_DICTIONARY.
  const std::u16string kDictionaryType = u"1";

  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text one\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"text two\", \"tt\": 5 }] } } "
      "] }";
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  ASSERT_TRUE(value && value->is_dict());
  ASSERT_TRUE(SuggestionAnswer::ParseAnswer(value->GetDict(), kDictionaryType,
                                            &answer));

  AutocompleteMatch match;
  match.answer = answer;
  match.contents = u"contents";
  match.description = u"description";

  OmniboxAnswerResult result(nullptr, nullptr, nullptr, match, u"query");
  EXPECT_TRUE(result.multiline_details());
}

}  // namespace app_list
