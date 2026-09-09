// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/brave_search_suggestion_parser.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/search_engines/search_engine_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/device_form_factor.h"

namespace {

// Parses `suggestions_json` -- the second element of a Brave Search suggest
// response -- as though the user had typed `input_text`. Goes through
// SearchSuggestionParser so that the engine dispatch is covered too.
bool ParseSuggestions(std::string_view input_text,
                      std::string_view suggestions_json,
                      SearchSuggestionParser::Results* results) {
  std::optional<base::Value> suggestions = base::JSONReader::Read(
      suggestions_json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  CHECK(suggestions.has_value());
  CHECK(suggestions->is_list());

  base::ListValue root_list;
  root_list.Append(input_text);
  root_list.Append(std::move(*suggestions));

  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(base::UTF8ToUTF16(input_text),
                          metrics::OmniboxEventProto::NTP, scheme_classifier);
  return SearchSuggestionParser::ParseSuggestResults(
      root_list, input, scheme_classifier, /*default_result_relevance=*/400,
      /*is_keyword_result=*/false, {.search_engine_type = SEARCH_ENGINE_BRAVE},
      results);
}

}  // namespace

TEST(BraveSearchSuggestionParserTest, ParseSuggestResults) {
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(ParseSuggestions("hel", R"([
      {"is_entity": true, "q": "helldivers 2", "name": "Helldivers 2",
       "desc": "2024 video game developed by Arrowhead Game Studios",
       "category": "game", "img": "https://imgs.search.brave.com/a.png",
       "logo": false},
      {"is_entity": false, "q": "hello fresh"}
  ])",
                               &results));
  ASSERT_EQ(2u, results.suggest_results.size());

  const auto& entity = results.suggest_results[0];
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, entity.type());
  EXPECT_EQ(omnibox::TYPE_ENTITY, entity.suggest_type());
  EXPECT_EQ(u"helldivers 2", entity.suggestion());
  EXPECT_EQ(u"2024 video game developed by Arrowhead Game Studios",
            entity.annotation());
  EXPECT_EQ("Helldivers 2",
            entity.suggest_template_info()->primary_text().text());
  EXPECT_EQ("https://imgs.search.brave.com/a.png",
            entity.suggest_template_info()->image().url());
  EXPECT_EQ("2024 video game developed by Arrowhead Game Studios",
            entity.suggest_template_info()->secondary_text().text());

  const auto& query = results.suggest_results[1];
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST, query.type());
  EXPECT_EQ(omnibox::TYPE_QUERY, query.suggest_type());
  EXPECT_EQ(u"hello fresh", query.suggestion());
  EXPECT_TRUE(query.annotation().empty());
  EXPECT_FALSE(query.suggest_template_info().has_value());
}

TEST(BraveSearchSuggestionParserTest, RelevanceIsNotFromTheServer) {
  // The response carries no relevance scores, so the suggestions are left for
  // SearchProvider to score locally.
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(ParseSuggestions("hel", R"([{"q": "hello fresh"}])", &results));
  ASSERT_EQ(1u, results.suggest_results.size());
  EXPECT_EQ(400, results.suggest_results[0].relevance());
  EXPECT_FALSE(results.suggest_results[0].relevance_from_server());
  EXPECT_FALSE(results.relevances_from_server);
  EXPECT_EQ(-1, results.verbatim_relevance);
}

TEST(BraveSearchSuggestionParserTest, PlainResponseUsesTheDefaultFormat) {
  // The engine is derived from the search URL, while the response format
  // depends on the suggest URL. A Brave Search engine whose suggest URL does
  // not ask for the rich format -- a user-defined engine, or an existing
  // profile whose stored engine definition predates it -- still answers with
  // the default format, and has to keep working.
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(
      ParseSuggestions("hel", R"(["hello fresh", "helldivers 2"])", &results));
  ASSERT_EQ(2u, results.suggest_results.size());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST,
            results.suggest_results[0].type());
  EXPECT_EQ(u"hello fresh", results.suggest_results[0].suggestion());
  EXPECT_EQ(u"helldivers 2", results.suggest_results[1].suggestion());
}

TEST(BraveSearchSuggestionParserTest, EmptyResponseIsNotAnError) {
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(ParseSuggestions("hel", "[]", &results));
  EXPECT_TRUE(results.suggest_results.empty());
}

TEST(BraveSearchSuggestionParserTest, SuggestionWithoutQueryIsSkipped) {
  // A suggestion with no "q" has nothing to search for. Neither does an entry
  // that is not a dictionary at all.
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(ParseSuggestions(
      "hel", R"([{"is_entity": true, "name": "Helldivers 2"}, "hello"])",
      &results));
  EXPECT_TRUE(results.suggest_results.empty());
}

TEST(BraveSearchSuggestionParserTest, ImageUrl) {
  // The native UI cannot render SVGs, so an image whose path names one is
  // dropped. The decision is made on the path, so a query or a fragment neither
  // hides the extension nor is mistaken for one. An image that cannot be shown
  // must not take the whole suggestion down with it.
  struct {
    std::string_view image_url;
    bool expected_usable;
  } kCases[] = {
      {"https://imgs.search.brave.com/a.png", true},
      {"https://imgs.search.brave.com/a.svg", false},
      // The extension is still there, just not at the end of the URL.
      {"https://imgs.search.brave.com/a.svg?v=2", false},
      {"https://imgs.search.brave.com/a.svg#frag", false},
      {"https://imgs.search.brave.com/a.SVG", false},
      // Only the path counts: this one is a PNG.
      {"https://imgs.search.brave.com/a.png?original=x.svg", true},
      // Not something to hand to an image fetcher.
      {"data:image/svg+xml;base64,PHN2Zy8+", false},
      {"javascript:alert(1)", false},
      {"not a url", false},
      {"", false},
      // The real shape: the original URL, extension and all, is base64 inside
      // the proxy path, so an SVG is not recognizable and is kept.
      {"https://imgs.search.brave.com/kBj18O32SmYBztqnmMElL2MEFfcE_2e3LoiUNIu"
       "Yy8Q/rs:fit:60:60:1:1/g:ce/aHR0cHM6Ly91cGxvYWQud2lraW1lZGlhLm9yZy9G"
       "aXJlZm94X2xvZ28uc3Zn",
       true},
  };

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.image_url);
    const std::string suggestions_json = base::StrCat(
        {R"([{"is_entity": true, "q": "helldivers 2", "name": "Helldivers 2",)",
         R"( "img": ")", test_case.image_url, R"("}])"});

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(ParseSuggestions("hel", suggestions_json, &results));
    ASSERT_EQ(1u, results.suggest_results.size());

    const auto& suggest_template_info =
        results.suggest_results[0].suggest_template_info();
    EXPECT_EQ(test_case.expected_usable, suggest_template_info->has_image());
    if (test_case.expected_usable) {
      EXPECT_EQ(test_case.image_url, suggest_template_info->image().url());
    }
    // The suggestion itself survives either way.
    EXPECT_EQ(u"helldivers 2", results.suggest_results[0].suggestion());
  }
}

TEST(BraveSearchSuggestionParserTest, EntityFlagWinsOverType) {
  // "is_entity" predates "type" and stays authoritative.
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(ParseSuggestions("hel", R"([
      {"type": "query", "is_entity": true, "q": "helldivers 2",
       "name": "Helldivers 2"}
  ])",
                               &results));
  ASSERT_EQ(1u, results.suggest_results.size());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
            results.suggest_results[0].type());
  EXPECT_EQ(omnibox::TYPE_ENTITY, results.suggest_results[0].suggest_type());
}

TEST(BraveSearchSuggestionParserTest, UnknownVerticalIsAQuerySuggestion) {
  // An unrecognized vertical degrades to a plain query suggestion rather than
  // being dropped.
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(ParseSuggestions(
      "weather", R"([{"type": "weather", "q": "weather in toronto"}])",
      &results));
  ASSERT_EQ(1u, results.suggest_results.size());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST,
            results.suggest_results[0].type());
  EXPECT_EQ(u"weather in toronto", results.suggest_results[0].suggestion());
}

TEST(BraveSearchSuggestionParserTest, CalculatorVertical) {
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(ParseSuggestions("5-2", R"([
      {"type": "calculator", "is_entity": false, "q": "5-2",
       "expression": "5-2", "answer": 3.0},
      {"type": "query", "q": "52 states of america list"}
  ])",
                               &results));
  ASSERT_EQ(2u, results.suggest_results.size());

  // The suggestion is the answer, so accepting the match searches the typed
  // text. Desktop additionally shows "<expression> = <answer>" as the contents.
  const auto& calculator = results.suggest_results[0];
  EXPECT_EQ(AutocompleteMatchType::CALCULATOR, calculator.type());
  EXPECT_EQ(omnibox::TYPE_CALCULATOR, calculator.suggest_type());
  EXPECT_EQ(u"3", calculator.suggestion());
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP) {
    EXPECT_EQ(u"5-2 = 3", calculator.match_contents());
  } else {
    EXPECT_EQ(u"3", calculator.match_contents());
  }

  const auto& query = results.suggest_results[1];
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST, query.type());
  EXPECT_EQ(u"52 states of america list", query.suggestion());
}

TEST(BraveSearchSuggestionParserTest, CalculatorWithStringAnswer) {
  // Unit conversions send "answer" as a string rather than a number.
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(ParseSuggestions("5000 m in km", R"([
      {"type": "calculator", "q": "5000 m in km",
       "expression": "5000 m in km", "answer": "5 km"}
  ])",
                               &results));
  ASSERT_EQ(1u, results.suggest_results.size());

  const auto& calculator = results.suggest_results[0];
  EXPECT_EQ(AutocompleteMatchType::CALCULATOR, calculator.type());
  EXPECT_EQ(u"5 km", calculator.suggestion());
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP) {
    EXPECT_EQ(u"5000 m in km = 5 km", calculator.match_contents());
  } else {
    EXPECT_EQ(u"5 km", calculator.match_contents());
  }
}

TEST(BraveSearchSuggestionParserTest, CalculatorAnswerNumberFormatting) {
  // A whole number must not pick up a decimal point, and a fractional answer
  // must keep the one it has. `answer` arrives as a JSON integer or a JSON
  // double depending on the result, and both reach the same conversion.
  struct {
    std::string_view answer_json;
    std::u16string_view expected;
  } kCases[] = {{"12", u"12"},      // JSON integer.
                {"12.0", u"12"},    // JSON double, nothing after the point.
                {"2.5", u"2.5"},    // Fractional.
                {"-2.5", u"-2.5"},  // Negative fractional.
                {"0", u"0"},        // Zero is a result, not a missing answer.
                {"1234567", u"1234567"}};

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.answer_json);
    const std::string suggestions_json =
        base::StrCat({R"([{"type": "calculator", "q": "6*2", "answer": )",
                      test_case.answer_json, "}]"});

    SearchSuggestionParser::Results results;
    ASSERT_TRUE(ParseSuggestions("6*2", suggestions_json, &results));
    ASSERT_EQ(1u, results.suggest_results.size());
    EXPECT_EQ(test_case.expected, results.suggest_results[0].suggestion());
  }
}

TEST(BraveSearchSuggestionParserTest, CalculatorWithoutExpression) {
  // Without "expression" the query stands in for it.
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(ParseSuggestions(
      "5-2", R"([{"type": "calculator", "q": "5-2", "answer": 3.0}])",
      &results));
  ASSERT_EQ(1u, results.suggest_results.size());

  const auto& calculator = results.suggest_results[0];
  EXPECT_EQ(u"3", calculator.suggestion());
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP) {
    EXPECT_EQ(u"5-2 = 3", calculator.match_contents());
  } else {
    EXPECT_EQ(u"3", calculator.match_contents());
  }
}

TEST(BraveSearchSuggestionParserTest, CalculatorDescriptionIsDropped) {
  // A description would restore the separator the desktop match cell
  // suppresses for CALCULATOR, so it is dropped even if the server sends one.
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(ParseSuggestions("5-2", R"([
      {"type": "calculator", "q": "5-2", "expression": "5-2", "answer": 3.0,
       "desc": "arithmetic"}
  ])",
                               &results));
  ASSERT_EQ(1u, results.suggest_results.size());
  EXPECT_TRUE(results.suggest_results[0].annotation().empty());
}

TEST(BraveSearchSuggestionParserTest, CalculatorWithoutAnswerIsSkipped) {
  // Nothing to display, so the suggestion is skipped.
  SearchSuggestionParser::Results results;
  ASSERT_TRUE(ParseSuggestions(
      "5-2", R"([{"type": "calculator", "q": "5-2", "expression": "5-2"}])",
      &results));
  EXPECT_TRUE(results.suggest_results.empty());
}

TEST(BraveSearchSuggestionParserTest, ResponseForAnotherQueryIsRejected) {
  // The response must be for the text that is currently in the omnibox. This is
  // enforced by the shared parser, before the response format matters.
  std::string json_data = R"(["chr", [{"q": "christmas"}]])";
  std::optional<base::Value> root_val =
      base::JSONReader::Read(json_data, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(root_val.has_value());
  TestSchemeClassifier scheme_classifier;
  AutocompleteInput input(u"chris", metrics::OmniboxEventProto::NTP,
                          scheme_classifier);
  SearchSuggestionParser::Results results;
  EXPECT_FALSE(SearchSuggestionParser::ParseSuggestResults(
      root_val->GetList(), input, scheme_classifier,
      /*default_result_relevance=*/400, /*is_keyword_result=*/false,
      {.search_engine_type = SEARCH_ENGINE_BRAVE}, &results));
}
