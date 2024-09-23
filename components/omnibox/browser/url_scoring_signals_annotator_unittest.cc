// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/url_scoring_signals_annotator.h"

#include <optional>
#include <string>
#include <vector>

#include "components/history/core/browser/url_row.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/autocomplete_scoring_signals_annotator.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"
#include "url/gurl.h"

using ScoringSignals = ::metrics::OmniboxScoringSignals;

namespace {

AutocompleteMatch CreateUrlAutocompleteMatch(
    const std::string& url_text,
    AutocompleteMatchType::Type type = AutocompleteMatchType::HISTORY_URL) {
  AutocompleteMatch url_match;
  url_match.destination_url = GURL(url_text);
  url_match.type = type;
  return url_match;
}

ScoringSignals CreateUrlMatchingScoringSignals(
    bool is_host_only,
    size_t length_of_url,
    std::optional<size_t> first_url_match_position,
    std::optional<bool> host_match_at_word_boundary,
    std::optional<bool> has_non_scheme_www_match,
    size_t total_url_match_length,
    size_t total_host_match_length,
    size_t total_path_match_length,
    size_t total_query_or_ref_match_length,
    size_t num_input_terms_matched_by_url,
    bool allowed_to_be_default_match) {
  ScoringSignals scoring_signals;
  scoring_signals.set_length_of_url(length_of_url);
  scoring_signals.set_is_host_only(is_host_only);
  scoring_signals.set_allowed_to_be_default_match(allowed_to_be_default_match);

  if (first_url_match_position.has_value()) {
    scoring_signals.set_first_url_match_position(*first_url_match_position);
  }
  if (host_match_at_word_boundary.has_value()) {
    // Not set if there is no match in the host.
    scoring_signals.set_host_match_at_word_boundary(
        *host_match_at_word_boundary);
  }
  if (has_non_scheme_www_match.has_value()) {
    scoring_signals.set_has_non_scheme_www_match(*has_non_scheme_www_match);
  }
  scoring_signals.set_total_url_match_length(total_url_match_length);
  scoring_signals.set_total_host_match_length(total_host_match_length);
  scoring_signals.set_total_path_match_length(total_path_match_length);
  scoring_signals.set_total_query_or_ref_match_length(
      total_query_or_ref_match_length);
  scoring_signals.set_num_input_terms_matched_by_url(
      num_input_terms_matched_by_url);
  return scoring_signals;
}

}  // namespace

class UrlScoringSignalsAnnotatorTest : public testing::Test {
 public:
  UrlScoringSignalsAnnotatorTest() = default;
  UrlScoringSignalsAnnotatorTest(const UrlScoringSignalsAnnotatorTest&) =
      delete;
  UrlScoringSignalsAnnotatorTest& operator=(
      const UrlScoringSignalsAnnotatorTest&) = delete;

 protected:
  void CompareScoringSignals(const ScoringSignals& scoring_signals,
                             const ScoringSignals& expected_scoring_signals);
};

void UrlScoringSignalsAnnotatorTest::CompareScoringSignals(
    const ScoringSignals& scoring_signals,
    const ScoringSignals& expected_scoring_signals) {
  EXPECT_EQ(scoring_signals.is_host_only(),
            expected_scoring_signals.is_host_only());
  EXPECT_EQ(scoring_signals.length_of_url(),
            expected_scoring_signals.length_of_url());

  EXPECT_EQ(scoring_signals.has_first_url_match_position(),
            expected_scoring_signals.has_first_url_match_position());
  if (expected_scoring_signals.has_first_url_match_position() &&
      scoring_signals.has_first_url_match_position()) {
    EXPECT_EQ(scoring_signals.first_url_match_position(),
              expected_scoring_signals.first_url_match_position());
  }

  EXPECT_EQ(scoring_signals.has_host_match_at_word_boundary(),
            expected_scoring_signals.has_host_match_at_word_boundary());
  if (expected_scoring_signals.has_host_match_at_word_boundary() &&
      scoring_signals.has_host_match_at_word_boundary()) {
    EXPECT_EQ(scoring_signals.host_match_at_word_boundary(),
              expected_scoring_signals.host_match_at_word_boundary());
    EXPECT_EQ(scoring_signals.has_non_scheme_www_match(),
              expected_scoring_signals.has_non_scheme_www_match());
  }

  EXPECT_EQ(scoring_signals.total_url_match_length(),
            expected_scoring_signals.total_url_match_length());
  EXPECT_EQ(scoring_signals.total_host_match_length(),
            expected_scoring_signals.total_host_match_length());
  EXPECT_EQ(scoring_signals.total_path_match_length(),
            expected_scoring_signals.total_path_match_length());
  EXPECT_EQ(scoring_signals.total_query_or_ref_match_length(),
            expected_scoring_signals.total_query_or_ref_match_length());
  EXPECT_EQ(scoring_signals.num_input_terms_matched_by_url(),
            expected_scoring_signals.num_input_terms_matched_by_url());
  EXPECT_EQ(scoring_signals.allowed_to_be_default_match(),
            expected_scoring_signals.allowed_to_be_default_match());
}

TEST_F(UrlScoringSignalsAnnotatorTest, AnnotateResultHostOnly) {
  UrlScoringSignalsAnnotator annotator;
  AutocompleteInput input(u"a www test", std::u16string::npos,
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  AutocompleteResult result;
  std::vector<AutocompleteMatch> matches{
      CreateUrlAutocompleteMatch("http://test.com/")};
  result.AppendMatches(matches);
  annotator.AnnotateResult(input, &result);

  const auto expected_scoring_signals = CreateUrlMatchingScoringSignals(
      /*is_host_only=*/true,
      /*length_of_url=*/16, /*first_url_match_position=*/7,
      /*host_match_at_word_boundary=*/true, /*has_non_scheme_www_match=*/true,
      /*total_url_match_length=*/4,
      /*total_host_match_length=*/4, /*total_path_match_length=*/0,
      /*total_query_or_ref_match_length=*/0,
      /*num_input_terms_matched_by_url=*/1,
      /*allowed_to_be_default_match=*/false);
  CompareScoringSignals(*result.match_at(0)->scoring_signals,
                        expected_scoring_signals);
}

TEST_F(UrlScoringSignalsAnnotatorTest, AnnotateResultUrlWithPath) {
  UrlScoringSignalsAnnotator annotator;
  AutocompleteInput input(u"a www test", std::u16string::npos,
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  AutocompleteResult result;
  std::vector<AutocompleteMatch> matches{
      // Match `a` in [path] of `path_a` will be fitered.
      CreateUrlAutocompleteMatch("http://gtest.com/path_a")};
  result.AppendMatches(matches);
  annotator.AnnotateResult(input, &result);

  const auto expected_scoring_signals = CreateUrlMatchingScoringSignals(
      /*is_host_only=*/false,
      /*length_of_url=*/23, /*first_url_match_position=*/8,
      /*host_match_at_word_boundary=*/false, /*has_non_scheme_www_match=*/true,
      /*total_url_match_length=*/5,
      /*total_host_match_length=*/4, /*total_path_match_length=*/1,
      /*total_query_or_ref_match_length=*/0,
      /*num_input_terms_matched_by_url=*/2,
      /*allowed_to_be_default_match=*/false);
  CompareScoringSignals(*result.match_at(0)->scoring_signals,
                        expected_scoring_signals);
}

TEST_F(UrlScoringSignalsAnnotatorTest, AnnotateResultPathMatchOnly) {
  UrlScoringSignalsAnnotator annotator;
  AutocompleteInput input(u"path", std::u16string::npos,
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  AutocompleteResult result;
  std::vector<AutocompleteMatch> matches{
      // Match `a` in [path] of `path_a` will be fitered.
      CreateUrlAutocompleteMatch("http://test.com/path_a")};
  result.AppendMatches(matches);
  annotator.AnnotateResult(input, &result);

  const auto expected_scoring_signals = CreateUrlMatchingScoringSignals(
      /*is_host_only=*/false,
      /*length_of_url=*/22, /*first_url_match_position=*/16,
      /*host_match_at_word_boundary=*/std::nullopt,
      /*has_non_scheme_www_match=*/std::nullopt,
      /*total_url_match_length=*/4,
      /*total_host_match_length=*/0, /*total_path_match_length=*/4,
      /*total_query_or_ref_match_length=*/0,
      /*num_input_terms_matched_by_url=*/1,
      /*allowed_to_be_default_match=*/false);
  CompareScoringSignals(*result.match_at(0)->scoring_signals,
                        expected_scoring_signals);
}

TEST_F(UrlScoringSignalsAnnotatorTest, AnnotateResultWWWOnly) {
  UrlScoringSignalsAnnotator annotator;
  AutocompleteInput input(u"a www test", std::u16string::npos,
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  AutocompleteResult result;
  std::vector<AutocompleteMatch> matches{
      CreateUrlAutocompleteMatch("http://www.host.com/")};
  result.AppendMatches(matches);
  annotator.AnnotateResult(input, &result);

  const auto expected_scoring_signals = CreateUrlMatchingScoringSignals(
      /*is_host_only=*/true,
      /*length_of_url=*/20, /*first_url_match_position=*/7,
      /*host_match_at_word_boundary=*/true, /*has_non_scheme_www_match=*/false,
      /*total_url_match_length=*/3,
      /*total_host_match_length=*/3, /*total_path_match_length=*/0,
      /*total_query_or_ref_match_length=*/0,
      /*num_input_terms_matched_by_url=*/1,
      /*allowed_to_be_default_match=*/false);
  CompareScoringSignals(*result.match_at(0)->scoring_signals,
                        expected_scoring_signals);
}
