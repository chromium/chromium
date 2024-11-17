// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_answer.h"

#include <algorithm>
#include <memory>

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/answer_data.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"

namespace {

bool ParseJsonToAnswerData(const std::string& answer_json,
                           omnibox::RichAnswerTemplate* answer_template) {
  std::optional<base::Value> value = base::JSONReader::Read(answer_json);
  if (!value || !value->is_dict()) {
    return false;
  }

  return omnibox::answer_data_parser::ParseJsonToAnswerData(value->GetDict(),
                                                            answer_template);
}

}  // namespace

TEST(SuggestionAnswerTest, EmptyJsonIsInvalid) {
  omnibox::RichAnswerTemplate answer_template;
  ASSERT_FALSE(ParseJsonToAnswerData("", &answer_template));
}

TEST(SuggestionAnswerTest, MalformedJsonIsInvalid) {
  omnibox::RichAnswerTemplate answer_template;
  std::string json = "} malformed json {";
  ASSERT_FALSE(ParseJsonToAnswerData(json, &answer_template));
}

TEST(SuggestionAnswerTest, ImageLinesMustContainAtLeastOneFragment) {
  omnibox::RichAnswerTemplate answer_template;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }, "
      "                      { \"t\": \"moar text\", \"tt\": 0 }], "
      "              \"i\": { \"d\": \"//example.com/foo.jpg\" } } }, "
      "  { \"il\": { \"t\": [], "
      "              \"at\": { \"t\": \"slatfatf\", \"tt\": 42 }, "
      "              \"st\": { \"t\": \"oh hi, Mark\", \"tt\": 729347 } } } "
      "] }";
  ASSERT_FALSE(ParseJsonToAnswerData(json, &answer_template));
}

TEST(SuggestionAnswerTest, ExactlyTwoImageLinesRequired) {
  omnibox::RichAnswerTemplate answer_template;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "] }";
  ASSERT_FALSE(ParseJsonToAnswerData(json, &answer_template));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }] } } "
      "] }";
  ASSERT_TRUE(ParseJsonToAnswerData(json, &answer_template));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }] } } "
      "  { \"il\": { \"t\": [{ \"t\": \"yet more text\", \"tt\": 13 }] } } "
      "] }";
  ASSERT_FALSE(ParseJsonToAnswerData(json, &answer_template));
}

TEST(SuggestionAnswerTest, FragmentsRequireBothTextAndType) {
  omnibox::RichAnswerTemplate answer_template;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\" }] } }, "
      "] }";
  ASSERT_FALSE(ParseJsonToAnswerData(json, &answer_template));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"tt\": 8 }] } }, "
      "] }";
  ASSERT_FALSE(ParseJsonToAnswerData(json, &answer_template));
}

TEST(SuggestionAnswerTest, ImageURLPresent) {
  omnibox::RichAnswerTemplate answer_template;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }], "
      "              \"i\": { \"d\": \"\" } } } "
      "] }";
  // If an image is present, there should be a valid URL.
  ASSERT_FALSE(ParseJsonToAnswerData(json, &answer_template));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }], "
      "              \"i\": { \"d\": \"https://example.com/foo.jpg\" } } } "
      "] }";
  ASSERT_TRUE(ParseJsonToAnswerData(json, &answer_template));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }], "
      "              \"i\": { \"d\": \"//example.com/foo.jpg\" } } } "
      "] }";
  ASSERT_TRUE(ParseJsonToAnswerData(json, &answer_template));
}

TEST(SuggestionAnswerTest, ValidPropertyValues) {
  omnibox::RichAnswerTemplate answer_template;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }, "
      "                      { \"t\": \"moar text\", \"tt\": 0 }], "
      "              \"i\": { \"d\": \"//example.com/foo.jpg\" } } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5, \"ln\": 3 }], "
      "              \"at\": { \"t\": \"slatfatf\", \"tt\": 6 }, "
      "              \"st\": { \"t\": \"oh hi, Mark\", \"tt\": 729347 } } } "
      "] }";
  ASSERT_TRUE(ParseJsonToAnswerData(json, &answer_template));

  const omnibox::AnswerData& answer_data = answer_template.answers(0);
  const omnibox::FormattedString& headline = answer_data.headline();
  EXPECT_EQ(2, headline.fragments_size());
  EXPECT_EQ("text", headline.fragments(0).text());
  EXPECT_EQ("moar text", headline.fragments(1).text());
  // Neither fragment should have a ColorType because of their respective text
  // types.
  EXPECT_FALSE(headline.fragments(0).has_color());
  EXPECT_FALSE(headline.fragments(1).has_color());
  EXPECT_EQ(0u, headline.fragments(0).start_index());
  EXPECT_EQ(headline.fragments(0).text().size() + 1,
            headline.fragments(1).start_index());
  // The full headline text.
  EXPECT_EQ("text moar text", headline.text());

  EXPECT_TRUE(answer_data.has_image());
  EXPECT_TRUE(GURL(answer_data.image().url()).is_valid());
  EXPECT_EQ("https://example.com/foo.jpg", answer_data.image().url());

  const omnibox::FormattedString& subhead = answer_data.subhead();
  EXPECT_EQ(3, subhead.fragments_size());
  EXPECT_EQ("other text", subhead.fragments(0).text());
  EXPECT_EQ("slatfatf", subhead.fragments(1).text());
  EXPECT_EQ("oh hi, Mark", subhead.fragments(2).text());
  EXPECT_EQ(omnibox::FormattedString::COLOR_ON_SURFACE_NEGATIVE,
            subhead.fragments(0).color());
  EXPECT_EQ(omnibox::FormattedString::COLOR_ON_SURFACE_POSITIVE,
            subhead.fragments(1).color());
  EXPECT_FALSE(subhead.fragments(2).has_color());
  EXPECT_EQ(0u, subhead.fragments(0).start_index());
  uint32_t first_fragment_size = subhead.fragments(0).text().size();
  uint32_t second_fragment_size = subhead.fragments(1).text().size();
  EXPECT_EQ(first_fragment_size + 1, subhead.fragments(1).start_index());
  EXPECT_EQ(first_fragment_size + second_fragment_size + 2,
            subhead.fragments(2).start_index());
  // The full subhead text.
  EXPECT_EQ("other text slatfatf oh hi, Mark", subhead.text());
}

TEST(SuggestionAnswerTest, ParseAccessibilityLabel) {
  omnibox::RichAnswerTemplate answer_template;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"al\": \"accessibility label\", "
      "              \"at\": { \"t\": \"additional text\", \"tt\": 12 }, "
      "              \"t\": [{ \"t\": \"other text\", \"tt\": 5 }] } }] }";
  ASSERT_TRUE(ParseJsonToAnswerData(json, &answer_template));

  const omnibox::AnswerData& answer_data = answer_template.answers(0);
  EXPECT_FALSE(answer_data.headline().has_a11y_text());

  EXPECT_TRUE(answer_data.subhead().has_a11y_text());
  const std::string template_label = answer_data.subhead().a11y_text();
  EXPECT_EQ(template_label, "accessibility label");
}
