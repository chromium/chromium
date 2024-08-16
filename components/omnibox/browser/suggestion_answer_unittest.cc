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

bool ParseAnswer(const std::string& answer_json, SuggestionAnswer* answer) {
  std::optional<base::Value> value = base::JSONReader::Read(answer_json);
  if (!value || !value->is_dict())
    return false;

  return SuggestionAnswer::ParseAnswer(value->GetDict(),
                                       omnibox::ANSWER_TYPE_WEATHER, answer);
}

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

TEST(SuggestionAnswerTest, DefaultAreEqual) {
  SuggestionAnswer answer1;
  SuggestionAnswer answer2;
  EXPECT_TRUE(answer1.Equals(answer2));
}

TEST(SuggestionAnswerTest, CopiesAreEqual) {
  SuggestionAnswer answer1;
  EXPECT_TRUE(answer1.Equals(SuggestionAnswer(answer1)));

  auto answer2 = std::make_unique<SuggestionAnswer>();
  EXPECT_TRUE(answer2->Equals(SuggestionAnswer(*answer2)));

  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }] } } "
      "] }";
  SuggestionAnswer answer3;
  ASSERT_TRUE(ParseAnswer(json, &answer3));
  EXPECT_TRUE(answer3.Equals(SuggestionAnswer(answer3)));
}

TEST(SuggestionAnswerTest, DifferentValuesAreUnequal) {
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }, "
      "                      { \"t\": \"moar text\", \"tt\": 0 }], "
      "              \"i\": { \"d\": \"//example.com/foo.jpg\" } } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }], "
      "              \"at\": { \"t\": \"slatfatf\", \"tt\": 42 }, "
      "              \"st\": { \"t\": \"oh hi, Mark\", \"tt\": 729347 } } } "
      "] }";
  SuggestionAnswer answer1;
  ASSERT_TRUE(ParseAnswer(json, &answer1));

  // Same but with a different type for one of the text fields.
  SuggestionAnswer answer2 = answer1;
  EXPECT_TRUE(answer1.Equals(answer2));
  answer2.first_line_.text_fields_[1].type_ = 1;
  EXPECT_FALSE(answer1.Equals(answer2));

  // Same but with different text for one of the text fields.
  answer2 = answer1;
  EXPECT_TRUE(answer1.Equals(answer2));
  answer2.first_line_.text_fields_[0].text_ = u"some text";
  EXPECT_FALSE(answer1.Equals(answer2));

  // Same but with a new URL on the second line.
  answer2 = answer1;
  EXPECT_TRUE(answer1.Equals(answer2));
  answer2.second_line_.image_url_ = GURL("http://foo.com/bar.png");
  EXPECT_FALSE(answer1.Equals(answer2));

  // Same but with the additional text removed from the second line.
  answer2 = answer1;
  EXPECT_TRUE(answer1.Equals(answer2));
  answer2.second_line_.additional_text_.reset();
  EXPECT_FALSE(answer1.Equals(answer2));

  // Same but with the status text removed from the second line.
  answer2 = answer1;
  EXPECT_TRUE(answer1.Equals(answer2));
  answer2.second_line_.status_text_.reset();
  EXPECT_FALSE(answer1.Equals(answer2));

  // Same but with the status text removed from the second line of the first
  // answer.
  answer2 = answer1;
  EXPECT_TRUE(answer1.Equals(answer2));
  answer1.second_line_.status_text_.reset();
  EXPECT_FALSE(answer1.Equals(answer2));

  // Same but with the additional text removed from the second line of the first
  // answer.
  answer2 = answer1;
  EXPECT_TRUE(answer1.Equals(answer2));
  answer1.second_line_.additional_text_.reset();
  EXPECT_FALSE(answer1.Equals(answer2));
}

TEST(SuggestionAnswerTest, EmptyJsonIsInvalid) {
  SuggestionAnswer answer;
  ASSERT_FALSE(ParseAnswer("", &answer));
}

TEST(SuggestionAnswerTest, MalformedJsonIsInvalid) {
  SuggestionAnswer answer;
  ASSERT_FALSE(ParseAnswer("} malformed json {", &answer));
}

TEST(SuggestionAnswerTest, TextFieldsRequireBothTextAndType) {
  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\" }] } }, "
      "] }";
  ASSERT_FALSE(ParseAnswer(json, &answer));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"tt\": 8 }] } }, "
      "] }";
  ASSERT_FALSE(ParseAnswer(json, &answer));
}

TEST(SuggestionAnswerTest, ImageLinesMustContainAtLeastOneTextField) {
  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }, "
      "                      { \"t\": \"moar text\", \"tt\": 0 }], "
      "              \"i\": { \"d\": \"//example.com/foo.jpg\" } } }, "
      "  { \"il\": { \"t\": [], "
      "              \"at\": { \"t\": \"slatfatf\", \"tt\": 42 }, "
      "              \"st\": { \"t\": \"oh hi, Mark\", \"tt\": 729347 } } } "
      "] }";
  ASSERT_FALSE(ParseAnswer(json, &answer));
}

TEST(SuggestionAnswerTest, ExactlyTwoLinesRequired) {
  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "] }";
  ASSERT_FALSE(ParseAnswer(json, &answer));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }] } } "
      "] }";
  ASSERT_TRUE(ParseAnswer(json, &answer));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }] } } "
      "  { \"il\": { \"t\": [{ \"t\": \"yet more text\", \"tt\": 13 }] } } "
      "] }";
  ASSERT_FALSE(ParseAnswer(json, &answer));
}

TEST(SuggestionAnswerTest, URLPresent) {
  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }], "
      "              \"i\": { \"d\": \"\" } } } "
      "] }";
  ASSERT_FALSE(ParseAnswer(json, &answer));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }], "
      "              \"i\": { \"d\": \"https://example.com/foo.jpg\" } } } "
      "] }";
  ASSERT_TRUE(ParseAnswer(json, &answer));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }], "
      "              \"i\": { \"d\": \"//example.com/foo.jpg\" } } } "
      "] }";
  ASSERT_TRUE(ParseAnswer(json, &answer));
}

TEST(SuggestionAnswerTest, ValidPropertyValues) {
  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }, "
      "                      { \"t\": \"moar text\", \"tt\": 0 }], "
      "              \"i\": { \"d\": \"//example.com/foo.jpg\" } } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5, \"ln\": 3 }], "
      "              \"at\": { \"t\": \"slatfatf\", \"tt\": 42 }, "
      "              \"st\": { \"t\": \"oh hi, Mark\", \"tt\": 729347 } } } "
      "] }";
  ASSERT_TRUE(ParseAnswer(json, &answer));

  const SuggestionAnswer::ImageLine& first_line = answer.first_line();
  EXPECT_EQ(2U, first_line.text_fields().size());
  EXPECT_EQ(u"text", first_line.text_fields()[0].text());
  EXPECT_EQ(8, first_line.text_fields()[0].type());
  EXPECT_EQ(u"moar text", first_line.text_fields()[1].text());
  EXPECT_EQ(0, first_line.text_fields()[1].type());
  EXPECT_FALSE(first_line.text_fields()[1].has_num_lines());
  EXPECT_EQ(1, first_line.num_text_lines());

  EXPECT_FALSE(first_line.additional_text());
  EXPECT_FALSE(first_line.status_text());

  EXPECT_TRUE(first_line.image_url().is_valid());
  EXPECT_EQ(GURL("https://example.com/foo.jpg"), first_line.image_url());

  const SuggestionAnswer::ImageLine& second_line = answer.second_line();
  EXPECT_EQ(1U, second_line.text_fields().size());
  EXPECT_EQ(u"other text", second_line.text_fields()[0].text());
  EXPECT_EQ(5, second_line.text_fields()[0].type());
  EXPECT_TRUE(second_line.text_fields()[0].has_num_lines());
  EXPECT_EQ(3, second_line.text_fields()[0].num_lines());
  EXPECT_EQ(3, second_line.num_text_lines());

  EXPECT_TRUE(second_line.additional_text());
  EXPECT_EQ(u"slatfatf", second_line.additional_text()->text());
  EXPECT_EQ(42, second_line.additional_text()->type());

  EXPECT_TRUE(second_line.status_text());
  EXPECT_EQ(u"oh hi, Mark", second_line.status_text()->text());
  EXPECT_EQ(729347, second_line.status_text()->type());

  EXPECT_FALSE(second_line.image_url().is_valid());
}

TEST(SuggestionAnswerTest, ParseAccessibilityLabel) {
  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"al\": \"accessibility label\", "
      "              \"at\": { \"t\": \"additional text\", \"tt\": 12 }, "
      "              \"t\": [{ \"t\": \"other text\", \"tt\": 5 }] } }] }";
  ASSERT_TRUE(ParseAnswer(json, &answer));

  EXPECT_FALSE(answer.first_line().accessibility_label());

  const std::u16string* label = answer.second_line().accessibility_label();
  ASSERT_NE(label, nullptr);
  EXPECT_EQ(*label, u"accessibility label");
}

// The following test similarities and differences between SuggestionAnswer and
// RichAnswerTemplate/AnswerData.
TEST(SuggestionAnswerTest, AnswerData_EmptyJsonIsInvalid) {
  omnibox::RichAnswerTemplate answer_template;
  SuggestionAnswer answer;

  bool parsed_answer_data = ParseJsonToAnswerData("", &answer_template);
  ASSERT_FALSE(parsed_answer_data);
  EXPECT_EQ(parsed_answer_data, ParseAnswer("", &answer));
}

TEST(SuggestionAnswerTest, AnswerData_MalformedJsonIsInvalid) {
  omnibox::RichAnswerTemplate answer_template;
  SuggestionAnswer answer;
  std::string json = "} malformed json {";

  bool parsed_answer_data = ParseJsonToAnswerData(json, &answer_template);
  ASSERT_FALSE(parsed_answer_data);
  EXPECT_EQ(parsed_answer_data, ParseAnswer(json, &answer));
}

TEST(SuggestionAnswerTest, AnswerData_ImageLinesMustContainAtLeastOneFragment) {
  omnibox::RichAnswerTemplate answer_template;
  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }, "
      "                      { \"t\": \"moar text\", \"tt\": 0 }], "
      "              \"i\": { \"d\": \"//example.com/foo.jpg\" } } }, "
      "  { \"il\": { \"t\": [], "
      "              \"at\": { \"t\": \"slatfatf\", \"tt\": 42 }, "
      "              \"st\": { \"t\": \"oh hi, Mark\", \"tt\": 729347 } } } "
      "] }";
  bool parsed_answer_data = ParseJsonToAnswerData(json, &answer_template);
  ASSERT_FALSE(parsed_answer_data);
  EXPECT_EQ(parsed_answer_data, ParseAnswer(json, &answer));
}

TEST(SuggestionAnswerTest, AnswerData_ExactlyTwoImageLinesRequired) {
  omnibox::RichAnswerTemplate answer_template;
  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "] }";
  bool parsed_answer_data = ParseJsonToAnswerData(json, &answer_template);
  ASSERT_FALSE(parsed_answer_data);
  EXPECT_EQ(parsed_answer_data, ParseAnswer(json, &answer));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }] } } "
      "] }";
  parsed_answer_data = ParseJsonToAnswerData(json, &answer_template);
  ASSERT_TRUE(parsed_answer_data);
  EXPECT_EQ(parsed_answer_data, ParseAnswer(json, &answer));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }] } } "
      "  { \"il\": { \"t\": [{ \"t\": \"yet more text\", \"tt\": 13 }] } } "
      "] }";
  parsed_answer_data = ParseJsonToAnswerData(json, &answer_template);
  ASSERT_FALSE(parsed_answer_data);
  EXPECT_EQ(parsed_answer_data, ParseAnswer(json, &answer));
}

TEST(SuggestionAnswerTest, AnswerData_FragmentsRequireBothTextAndType) {
  omnibox::RichAnswerTemplate answer_template;
  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\" }] } }, "
      "] }";
  bool parsed_answer_data = ParseJsonToAnswerData(json, &answer_template);
  ASSERT_FALSE(parsed_answer_data);
  EXPECT_EQ(parsed_answer_data, ParseAnswer(json, &answer));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"tt\": 8 }] } }, "
      "] }";
  parsed_answer_data = ParseJsonToAnswerData(json, &answer_template);
  ASSERT_FALSE(parsed_answer_data);
  EXPECT_EQ(parsed_answer_data, ParseAnswer(json, &answer));
}

TEST(SuggestionAnswerTest, AnswerData_URLPresent) {
  omnibox::RichAnswerTemplate answer_template;
  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }], "
      "              \"i\": { \"d\": \"\" } } } "
      "] }";
  // If an image is present, there should be a valid URL.
  bool parsed_answer_data = ParseJsonToAnswerData(json, &answer_template);
  ASSERT_FALSE(parsed_answer_data);
  EXPECT_EQ(parsed_answer_data, ParseAnswer(json, &answer));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }], "
      "              \"i\": { \"d\": \"https://example.com/foo.jpg\" } } } "
      "] }";
  parsed_answer_data = ParseJsonToAnswerData(json, &answer_template);
  ASSERT_TRUE(parsed_answer_data);
  EXPECT_EQ(parsed_answer_data, ParseAnswer(json, &answer));

  json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"other text\", \"tt\": 5 }], "
      "              \"i\": { \"d\": \"//example.com/foo.jpg\" } } } "
      "] }";
  parsed_answer_data = ParseJsonToAnswerData(json, &answer_template);
  ASSERT_TRUE(parsed_answer_data);
  EXPECT_EQ(parsed_answer_data, ParseAnswer(json, &answer));
}

TEST(SuggestionAnswerTest, AnswerData_ValidPropertyValues) {
  omnibox::RichAnswerTemplate answer_template;
  SuggestionAnswer answer;
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
  ASSERT_TRUE(ParseAnswer(json, &answer));

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
  // SuggestionAnswer's `first_line` TextFields hold the same text as
  // AnswerData's `headline` FormattedStringFragments.
  const SuggestionAnswer::ImageLine& first_line = answer.first_line();
  EXPECT_EQ(base::UTF16ToUTF8(first_line.text_fields()[0].text()),
            headline.fragments(0).text());
  EXPECT_EQ(base::UTF16ToUTF8(first_line.text_fields()[1].text()),
            headline.fragments(1).text());
  EXPECT_TRUE(answer_data.has_image());
  EXPECT_TRUE(GURL(answer_data.image().url()).is_valid());
  EXPECT_EQ("https://example.com/foo.jpg", answer_data.image().url());
  // SuggestionAnswer and AnswerData image URL should be equal.
  EXPECT_EQ(first_line.image_url().spec(), answer_data.image().url());

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

  const SuggestionAnswer::ImageLine& second_line = answer.second_line();
  EXPECT_EQ(base::UTF16ToUTF8(second_line.text_fields()[0].text()),
            subhead.fragments(0).text());
  // Additional text and status are no longer treated as TextFields (equivalent
  // to FormattedString) and are now instead treated as
  // FormattedStringFragments.
  EXPECT_TRUE(second_line.additional_text());
  EXPECT_EQ(base::UTF16ToUTF8(second_line.additional_text()->text()),
            subhead.fragments(1).text());
  EXPECT_TRUE(second_line.status_text());
  EXPECT_EQ(base::UTF16ToUTF8(second_line.status_text()->text()),
            subhead.fragments(2).text());
}

TEST(SuggestionAnswerTest, AnswerData_ParseAccessibilityLabel) {
  omnibox::RichAnswerTemplate answer_template;
  SuggestionAnswer answer;
  std::string json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"al\": \"accessibility label\", "
      "              \"at\": { \"t\": \"additional text\", \"tt\": 12 }, "
      "              \"t\": [{ \"t\": \"other text\", \"tt\": 5 }] } }] }";
  ASSERT_TRUE(ParseJsonToAnswerData(json, &answer_template));
  ASSERT_TRUE(ParseAnswer(json, &answer));

  const omnibox::AnswerData& answer_data = answer_template.answers(0);
  EXPECT_FALSE(answer_data.headline().has_a11y_text());

  EXPECT_TRUE(answer_data.subhead().has_a11y_text());
  const std::string template_label = answer_data.subhead().a11y_text();
  EXPECT_EQ(template_label, "accessibility label");

  const std::string suggestion_answer_label =
      base::UTF16ToUTF8(*answer.second_line().accessibility_label());
  ASSERT_EQ(suggestion_answer_label, template_label);
}
