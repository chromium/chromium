// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match.h"

#include <stddef.h>

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
#include "components/omnibox/browser/actions/omnibox_answer_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"
#include "third_party/omnibox_proto/entity_info.pb.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

using ScoringSignals = ::metrics::OmniboxScoringSignals;

namespace {

class FakeOmniboxAction : public OmniboxAction {
 public:
  explicit FakeOmniboxAction(OmniboxActionId id)
      : OmniboxAction(LabelStrings(u"", u"", u"", u""), GURL{}), id_(id) {}
  OmniboxActionId ActionId() const override { return id_; }

 private:
  ~FakeOmniboxAction() override = default;
  OmniboxActionId id_{};
};

void TestSetAllowedToBeDefault(int caseI,
                               const std::string input_text,
                               bool input_prevent_inline_autocomplete,
                               const std::string match_inline_autocompletion,
                               const std::string match_prefix_autocompletion,
                               const std::string expected_inline_autocompletion,
                               bool expected_allowed_to_be_default_match) {
  AutocompleteInput input(base::UTF8ToUTF16(input_text),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_prevent_inline_autocomplete(input_prevent_inline_autocomplete);

  AutocompleteMatch match;
  match.inline_autocompletion = base::UTF8ToUTF16(match_inline_autocompletion);
  match.prefix_autocompletion = base::UTF8ToUTF16(match_prefix_autocompletion);

  match.SetAllowedToBeDefault(input);

  EXPECT_EQ(base::UTF16ToUTF8(match.inline_autocompletion).c_str(),
            expected_inline_autocompletion)
      << "case " << caseI;
  EXPECT_EQ(match.allowed_to_be_default_match,
            expected_allowed_to_be_default_match)
      << "case " << caseI;
}

AutocompleteMatch CreateACMatchWithScoringSignals(
    int typed_count,
    int visit_count,
    int elapsed_time_last_visit_secs,
    int shortcut_visit_count,
    int shortest_shortcut_len,
    bool is_host_only,
    int num_bookmarks_of_url,
    int first_bookmark_title_match_position,
    int total_bookmark_title_match_length,
    int num_input_terms_matched_by_bookmark_title,
    int first_url_match_position,
    int total_url_match_length,
    bool host_match_at_word_boundary,
    int total_path_match_length,
    int total_query_or_ref_match_length,
    int total_title_match_length,
    bool has_non_scheme_www_match,
    int num_input_terms_matched_by_title,
    int num_input_terms_matched_by_url,
    int length_of_url,
    float site_engagement,
    bool allowed_to_be_default_match) {
  AutocompleteMatch match;
  match.scoring_signals = std::make_optional<ScoringSignals>();
  match.scoring_signals->set_typed_count(typed_count);
  match.scoring_signals->set_visit_count(visit_count);
  match.scoring_signals->set_elapsed_time_last_visit_secs(
      elapsed_time_last_visit_secs);
  match.scoring_signals->set_shortcut_visit_count(shortcut_visit_count);
  match.scoring_signals->set_shortest_shortcut_len(shortest_shortcut_len);
  match.scoring_signals->set_is_host_only(is_host_only);
  match.scoring_signals->set_num_bookmarks_of_url(num_bookmarks_of_url);
  match.scoring_signals->set_first_bookmark_title_match_position(
      first_bookmark_title_match_position);
  match.scoring_signals->set_total_bookmark_title_match_length(
      total_bookmark_title_match_length);
  match.scoring_signals->set_num_input_terms_matched_by_bookmark_title(
      num_input_terms_matched_by_bookmark_title);
  match.scoring_signals->set_first_url_match_position(first_url_match_position);
  match.scoring_signals->set_total_url_match_length(total_url_match_length);
  match.scoring_signals->set_host_match_at_word_boundary(
      host_match_at_word_boundary);
  match.scoring_signals->set_total_path_match_length(total_path_match_length);
  match.scoring_signals->set_total_query_or_ref_match_length(
      total_query_or_ref_match_length);
  match.scoring_signals->set_total_title_match_length(total_title_match_length);
  match.scoring_signals->set_has_non_scheme_www_match(has_non_scheme_www_match);
  match.scoring_signals->set_num_input_terms_matched_by_title(
      num_input_terms_matched_by_title);
  match.scoring_signals->set_num_input_terms_matched_by_url(
      num_input_terms_matched_by_url);
  match.scoring_signals->set_length_of_url(length_of_url);
  match.scoring_signals->set_site_engagement(site_engagement);
  match.scoring_signals->set_allowed_to_be_default_match(
      allowed_to_be_default_match);

  return match;
}

// Use a test fixture to ensure that any scoped settings that are set during the
// test are cleared after the test is terminated.
class AutocompleteMatchTest : public testing::Test {
 protected:
  void TearDown() override {
    RichAutocompletionParams::ClearParamsForTesting();
  }
};

}  // namespace

TEST_F(AutocompleteMatchTest, MoreRelevant) {
  struct RelevantCases {
    int r1;
    int r2;
    bool expected_result;
  } cases[] = {
    {  10,   0, true  },
    {  10,  -5, true  },
    {  -5,  10, false },
    {   0,  10, false },
    { -10,  -5, false  },
    {  -5, -10, true },
  };

  AutocompleteMatch m1(nullptr, 0, false,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  AutocompleteMatch m2(nullptr, 0, false,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);

  for (const auto& caseI : cases) {
    m1.relevance = caseI.r1;
    m2.relevance = caseI.r2;
    EXPECT_EQ(caseI.expected_result, AutocompleteMatch::MoreRelevant(m1, m2));
  }
}

TEST_F(AutocompleteMatchTest, MergeClassifications) {
  // Merging two empty vectors should result in an empty vector.
  EXPECT_EQ(std::string(),
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ACMatchClassifications(),
              AutocompleteMatch::ACMatchClassifications())));

  // If one vector is empty and the other is "trivial" but non-empty (i.e. (0,
  // NONE)), the non-empty vector should be returned.
  EXPECT_EQ("0,0",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,0"),
              AutocompleteMatch::ACMatchClassifications())));
  EXPECT_EQ("0,0",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ACMatchClassifications(),
              AutocompleteMatch::ClassificationsFromString("0,0"))));

  // Ditto if the one-entry vector is non-trivial.
  EXPECT_EQ("0,1",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,1"),
              AutocompleteMatch::ACMatchClassifications())));
  EXPECT_EQ("0,1",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ACMatchClassifications(),
              AutocompleteMatch::ClassificationsFromString("0,1"))));

  // Merge an unstyled one-entry vector with a styled one-entry vector.
  EXPECT_EQ("0,1",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,0"),
              AutocompleteMatch::ClassificationsFromString("0,1"))));

  // Test simple cases of overlap.
  EXPECT_EQ("0,3," "1,2",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,1," "1,0"),
              AutocompleteMatch::ClassificationsFromString("0,2"))));
  EXPECT_EQ("0,3," "1,2",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,2"),
              AutocompleteMatch::ClassificationsFromString("0,1," "1,0"))));

  // Test the case where both vectors have classifications at the same
  // positions.
  EXPECT_EQ("0,3",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,1," "1,2"),
              AutocompleteMatch::ClassificationsFromString("0,2," "1,1"))));

  // Test an arbitrary complicated case.
  EXPECT_EQ("0,2," "1,0," "2,1," "4,3," "5,7," "6,3," "7,7," "15,1," "17,0",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString(
                  "0,0," "2,1," "4,3," "7,7," "10,6," "15,0"),
              AutocompleteMatch::ClassificationsFromString(
                  "0,2," "1,0," "5,7," "6,1," "17,0"))));
}

TEST_F(AutocompleteMatchTest, GetMatchComponents) {
  struct MatchComponentsTestData {
    const std::string url;
    std::vector<std::string> input_terms;
    bool expected_match_in_scheme;
    bool expected_match_in_subdomain;
  };

  MatchComponentsTestData test_cases[] = {
      // Match in scheme.
      {"http://www.google.com", {"ht"}, true, false},
      // Match within the scheme, but not starting at the beginning, i.e. "ttp".
      {"http://www.google.com", {"tp"}, false, false},
      // Sanity check that HTTPS still works.
      {"https://www.google.com", {"http"}, true, false},

      // Match within the subdomain.
      {"http://www.google.com", {"www"}, false, true},
      {"http://www.google.com", {"www."}, false, true},
      // Don't consider matches on the '.' delimiter as a match_in_subdomain.
      {"http://www.google.com", {"."}, false, false},
      {"http://www.google.com", {".goo"}, false, false},
      // Matches within the domain.
      {"http://www.google.com", {"goo"}, false, false},
      // Verify that in private registries, we detect matches in subdomains.
      {"http://www.appspot.com", {"www"}, false, true},

      // Matches spanning the scheme, subdomain, and domain.
      {"http://www.google.com", {"http://www.goo"}, true, true},
      {"http://www.google.com", {"ht", "www"}, true, true},
      // But we should not flag match_in_subdomain if there is no subdomain.
      {"http://google.com", {"http://goo"}, true, false},

      // Matches spanning the subdomain and path.
      {"http://www.google.com/abc", {"www.google.com/ab"}, false, true},
      {"http://www.google.com/abc", {"www", "ab"}, false, true},

      // Matches spanning the scheme, subdomain, and path.
      {"http://www.google.com/abc", {"http://www.google.com/ab"}, true, true},
      {"http://www.google.com/abc", {"ht", "ww", "ab"}, true, true},

      // Intranet sites.
      {"http://foobar/biz", {"foobar"}, false, false},
      {"http://foobar/biz", {"biz"}, false, false},

      // Ensure something sane happens when the URL input is invalid.
      {"", {""}, false, false},
      {"foobar", {"bar"}, false, false},
  };
  for (auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << " url=" << test_case.url << " first input term="
                 << test_case.input_terms[0] << " expected_match_in_scheme="
                 << test_case.expected_match_in_scheme
                 << " expected_match_in_subdomain="
                 << test_case.expected_match_in_subdomain);
    bool match_in_scheme = false;
    bool match_in_subdomain = false;
    std::vector<AutocompleteMatch::MatchPosition> match_positions;
    for (auto& term : test_case.input_terms) {
      size_t start = test_case.url.find(term);
      ASSERT_NE(std::string::npos, start);
      size_t end = start + term.size();
      match_positions.push_back(std::make_pair(start, end));
    }
    AutocompleteMatch::GetMatchComponents(GURL(test_case.url), match_positions,
                                          &match_in_scheme,
                                          &match_in_subdomain);
    EXPECT_EQ(test_case.expected_match_in_scheme, match_in_scheme);
    EXPECT_EQ(test_case.expected_match_in_subdomain, match_in_subdomain);
  }
}

TEST_F(AutocompleteMatchTest, FormatUrlForSuggestionDisplay) {
  // This test does not need to verify url_formatter's functionality in-depth,
  // since url_formatter has its own unit tests. This test is to validate that
  // flipping feature flags and varying the trim_scheme parameter toggles the
  // correct behavior within AutocompleteMatch::GetFormatTypes.
  struct FormatUrlTestData {
    const std::string url;
    bool preserve_scheme;
    bool preserve_subdomain;
    const wchar_t* expected_result;

    void Validate() {
      SCOPED_TRACE(testing::Message()
                   << " url=" << url << " preserve_scheme=" << preserve_scheme
                   << " preserve_subdomain=" << preserve_subdomain
                   << " expected_result=" << expected_result);
      auto format_types = AutocompleteMatch::GetFormatTypes(preserve_scheme,
                                                            preserve_subdomain);
      EXPECT_EQ(base::WideToUTF16(expected_result),
                url_formatter::FormatUrl(GURL(url), format_types,
                                         base::UnescapeRule::SPACES, nullptr,
                                         nullptr, nullptr));
    }
  };

  FormatUrlTestData normal_cases[] = {
      // Test the |preserve_scheme| parameter.
      {"http://google.com", false, false, L"google.com"},
      {"https://google.com", false, false, L"google.com"},
      {"http://google.com", true, false, L"http://google.com"},
      {"https://google.com", true, false, L"https://google.com"},

      // Test the |preserve_subdomain| parameter.
      {"http://www.google.com", false, false, L"google.com"},
      {"http://www.google.com", false, true, L"www.google.com"},

      // Test that paths are preserved in the default case.
      {"http://google.com/foobar", false, false, L"google.com/foobar"},
  };
  for (FormatUrlTestData& test_case : normal_cases)
    test_case.Validate();
}

TEST_F(AutocompleteMatchTest, SupportsDeletion) {
  // A non-deletable match with no duplicates.
  AutocompleteMatch m(nullptr, 0, false,
                      AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  EXPECT_FALSE(m.SupportsDeletion());

  // A deletable match with no duplicates.
  AutocompleteMatch m1(nullptr, 0, true,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  EXPECT_TRUE(m1.SupportsDeletion());

  // A non-deletable match, with non-deletable duplicates.
  m.duplicate_matches.push_back(AutocompleteMatch(
      nullptr, 0, false, AutocompleteMatchType::URL_WHAT_YOU_TYPED));
  m.duplicate_matches.push_back(AutocompleteMatch(
      nullptr, 0, false, AutocompleteMatchType::URL_WHAT_YOU_TYPED));
  EXPECT_FALSE(m.SupportsDeletion());

  // A non-deletable match, with at least one deletable duplicate.
  m.duplicate_matches.push_back(AutocompleteMatch(
      nullptr, 0, true, AutocompleteMatchType::URL_WHAT_YOU_TYPED));
  EXPECT_TRUE(m.SupportsDeletion());
}

// Structure containing URL pairs for deduping-related tests.
struct DuplicateCase {
  const wchar_t* input;
  const std::string url1;
  const std::string url2;
  const bool expected_duplicate;
};

// Runs deduping logic against URLs in |duplicate_case| and makes sure they are
// unique or matched as duplicates as expected.
void CheckDuplicateCase(const DuplicateCase& duplicate_case) {
  SCOPED_TRACE("input=" + base::WideToUTF8(duplicate_case.input) +
               " url1=" + duplicate_case.url1 + " url2=" + duplicate_case.url2);
  AutocompleteInput input(base::WideToUTF16(duplicate_case.input),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteMatch m1(nullptr, 100, false,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  m1.destination_url = GURL(duplicate_case.url1);
  m1.ComputeStrippedDestinationURL(input, nullptr);
  AutocompleteMatch m2(nullptr, 100, false,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  m2.destination_url = GURL(duplicate_case.url2);
  m2.ComputeStrippedDestinationURL(input, nullptr);
  EXPECT_EQ(duplicate_case.expected_duplicate,
            m1.stripped_destination_url == m2.stripped_destination_url);
  EXPECT_TRUE(m1.stripped_destination_url.is_valid());
  EXPECT_TRUE(m2.stripped_destination_url.is_valid());
}

TEST_F(AutocompleteMatchTest, Duplicates) {
  DuplicateCase cases[] = {
    { L"g", "http://www.google.com/",  "https://www.google.com/",    true },
    { L"g", "http://www.google.com/",  "http://www.google.com",      true },
    { L"g", "http://google.com/",      "http://www.google.com/",     true },
    { L"g", "http://www.google.com/",  "HTTP://www.GOOGLE.com/",     true },
    { L"g", "http://www.google.com/",  "http://www.google.com",      true },
    { L"g", "https://www.google.com/", "http://google.com",          true },
    { L"g", "http://www.google.com/",  "wss://www.google.com/",      false },
    { L"g", "http://www.google.com/1", "http://www.google.com/1/",   false },
    { L"g", "http://www.google.com/",  "http://www.google.com/1",    false },
    { L"g", "http://www.google.com/",  "http://www.goo.com/",        false },
    { L"g", "http://www.google.com/",  "http://w2.google.com/",      false },
    { L"g", "http://www.google.com/",  "http://m.google.com/",       false },
    { L"g", "http://www.google.com/",  "http://www.google.com/?foo", false },

    // Don't allow URLs with different schemes to be considered duplicates for
    // certain inputs.
    { L"http://g", "http://google.com/",
                   "https://google.com/",  false },
    { L"http://g", "http://blah.com/",
                   "https://blah.com/",    true  },
    { L"http://g", "http://google.com/1",
                   "https://google.com/1", false },
    { L"http://g hello",    "http://google.com/",
                            "https://google.com/", false },
    { L"hello http://g",    "http://google.com/",
                            "https://google.com/", false },
    { L"hello http://g",    "http://blah.com/",
                            "https://blah.com/",   true  },
    { L"http://b http://g", "http://google.com/",
                            "https://google.com/", false },
    { L"http://b http://g", "http://blah.com/",
                            "https://blah.com/",   false },

    // If the user types unicode that matches the beginning of a
    // punycode-encoded hostname then consider that a match.
    { L"x",               "http://xn--1lq90ic7f1rc.cn/",
                          "https://xn--1lq90ic7f1rc.cn/", true  },
    { L"http://\x5317 x", "http://xn--1lq90ic7f1rc.cn/",
                          "https://xn--1lq90ic7f1rc.cn/", false },
    { L"http://\x89c6 x", "http://xn--1lq90ic7f1rc.cn/",
                          "https://xn--1lq90ic7f1rc.cn/", true  },

    // URLs with hosts containing only `www.` should produce valid stripped urls
    { L"http://www./", "http://www./", "http://google.com/", false },
  };

  for (const auto& caseI : cases)
    CheckDuplicateCase(caseI);
}

TEST_F(AutocompleteMatchTest, DedupeDriveURLs) {
  DuplicateCase cases[] = {
      // Document URLs pointing to the same document, perhaps with different
      // /edit points, hashes, or cgiargs, are deduped.
      {L"docs", "https://docs.google.com/spreadsheets/d/the_doc-id/preview?x=1",
       "https://docs.google.com/spreadsheets/d/the_doc-id/edit?x=2#y=3", true},
      {L"report", "https://drive.google.com/open?id=the-doc-id",
       "https://docs.google.com/spreadsheets/d/the-doc-id/edit?x=2#y=3", true},
      // Similar but different URLs should not be deduped.
      {L"docs", "https://docs.google.com/spreadsheets/d/the_doc-id/preview",
       "https://docs.google.com/spreadsheets/d/another_doc-id/preview", false},
      {L"report", "https://drive.google.com/open?id=the-doc-id",
       "https://drive.google.com/open?id=another-doc-id", false},
  };

  for (const auto& caseI : cases)
    CheckDuplicateCase(caseI);
}

TEST_F(AutocompleteMatchTest, UpgradeMatchWithPropertiesFrom) {
  scoped_refptr<FakeAutocompleteProvider> bookmark_provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_BOOKMARK);
  scoped_refptr<FakeAutocompleteProvider> history_provider =
      new FakeAutocompleteProvider(
          AutocompleteProvider::Type::TYPE_HISTORY_QUICK);
  scoped_refptr<FakeAutocompleteProvider> search_provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_SEARCH);

  AutocompleteMatch search_history_match(search_provider.get(), 500, true,
                                         AutocompleteMatchType::SEARCH_HISTORY);

  // Entity match should get the increased score, but not change types.
  AutocompleteMatch entity_match(search_provider.get(), 400, false,
                                 AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);
  entity_match.UpgradeMatchWithPropertiesFrom(search_history_match);
  EXPECT_EQ(entity_match.relevance, 500);
  EXPECT_EQ(entity_match.type, AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);

  // Suggest and search-what-typed matches should get the search history type.
  AutocompleteMatch suggest_match(search_provider.get(), 400, true,
                                  AutocompleteMatchType::SEARCH_SUGGEST);
  AutocompleteMatch search_what_you_typed(
      search_provider.get(), 400, true,
      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  suggest_match.UpgradeMatchWithPropertiesFrom(search_history_match);
  search_what_you_typed.UpgradeMatchWithPropertiesFrom(search_history_match);
  EXPECT_EQ(suggest_match.relevance, 500);
  EXPECT_EQ(search_what_you_typed.relevance, 500);
  EXPECT_EQ(suggest_match.type, AutocompleteMatchType::SEARCH_HISTORY);
  EXPECT_EQ(search_what_you_typed.type, AutocompleteMatchType::SEARCH_HISTORY);

  // Some providers should bestow their suggestion texts even if not the primary
  // duplicate.
  AutocompleteMatch history_match(history_provider.get(), 800, true,
                                  AutocompleteMatchType::HISTORY_TITLE);
  AutocompleteMatch bookmark_match(bookmark_provider.get(), 400, true,
                                   AutocompleteMatchType::BOOKMARK_TITLE);
  history_match.contents = u"overwrite";
  history_match.inline_autocompletion = u"preserve";
  bookmark_match.contents = u"propagate";
  bookmark_match.inline_autocompletion = u"discard";
  history_match.UpgradeMatchWithPropertiesFrom(bookmark_match);
  EXPECT_EQ(history_match.type, AutocompleteMatchType::HISTORY_TITLE);
  EXPECT_EQ(history_match.contents, u"propagate");
  EXPECT_EQ(history_match.inline_autocompletion, u"preserve");

  omnibox::RichAnswerTemplate answer_template;
  omnibox::SuggestionEnhancement* enhancement =
      answer_template.mutable_enhancements()->add_enhancements();
  enhancement->set_display_text("Similar and opposite words");
  AutocompleteMatch match_with_answer_actions(
      search_provider.get(), 400, true, AutocompleteMatchType::SEARCH_SUGGEST);
  match_with_answer_actions.actions.push_back(
      base::MakeRefCounted<OmniboxAnswerAction>(
          std::move(*enhancement), TemplateURLRef::SearchTermsArgs(),
          omnibox::ANSWER_TYPE_DICTIONARY));
  AutocompleteMatch match_with_no_answer_actions(
      search_provider.get(), 400, true,
      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  match_with_no_answer_actions.UpgradeMatchWithPropertiesFrom(
      match_with_answer_actions);
  EXPECT_EQ(0u, match_with_no_answer_actions.actions.size());
}

TEST_F(AutocompleteMatchTest, MergeScoringSignals) {
  AutocompleteMatch match = CreateACMatchWithScoringSignals(
      /*typed_count*/ 3, /*visit_count*/ 10,
      /*elapsed_time_last_visit_secs*/ 100, /*shortcut_visit_count*/ 5,
      /*shortest_shortcut_len*/ 3, /*is_host_only*/ true,
      /*num_bookmarks_of_url*/ 5, /*first_bookmark_title_match_position*/ 1,
      /*total_bookmark_title_match_length*/ 8,
      /*num_input_terms_matched_by_bookmark_title*/ 2,
      /*first_url_match_position*/ 2, /*total_url_match_length*/ 5,
      /*host_match_at_word_boundary*/ true, /*total_path_match_length*/ 0,
      /*total_query_or_ref_match_length*/ 0,
      /*total_title_match_length*/ 5, /*has_non_scheme_www_match*/ true,
      /*num_input_terms_matched_by_title*/ 2,
      /*num_input_terms_matched_by_url*/ 2, /*length_of_url*/ 10,
      /*site_engagement*/ 0.6, /*allowed_to_be_default_match*/ true);

  AutocompleteMatch other_match = CreateACMatchWithScoringSignals(
      /*typed_count*/ 1, /*visit_count*/ 2, /*elapsed_time_last_visit_secs*/ 50,
      /*shortcut_visit_count*/ 1,
      /*shortest_shortcut_len*/ 2, /*is_host_only*/ false,
      /*num_bookmarks_of_url*/ 1, /*first_bookmark_title_match_position*/ 2,
      /*total_bookmark_title_match_length*/ 6,
      /*num_input_terms_matched_by_bookmark_title*/ 3,
      /*first_url_match_position*/ 5, /*total_url_match_length*/ 3,
      /*host_match_at_word_boundary*/ false, /*total_path_match_length*/ 1,
      /*total_query_or_ref_match_length*/ 2,
      /*total_title_match_length*/ 3, /*has_non_scheme_www_match*/ false,
      /*num_input_terms_matched_by_title*/ 0,
      /*num_input_terms_matched_by_url*/ 1, /*length_of_url*/ 12,
      /*site_engagement*/ 0.5, /*allowed_to_be_default_match*/ false);

  match.MergeScoringSignals(other_match);

  EXPECT_EQ(match.scoring_signals->typed_count(), 3);
  EXPECT_EQ(match.scoring_signals->visit_count(), 10);
  EXPECT_EQ(match.scoring_signals->elapsed_time_last_visit_secs(), 50);
  EXPECT_EQ(match.scoring_signals->shortcut_visit_count(), 5);
  EXPECT_EQ(match.scoring_signals->shortest_shortcut_len(), 2);
  EXPECT_TRUE(match.scoring_signals->is_host_only());
  EXPECT_EQ(match.scoring_signals->num_bookmarks_of_url(), 5);
  EXPECT_EQ(match.scoring_signals->first_bookmark_title_match_position(), 1);
  EXPECT_EQ(match.scoring_signals->total_bookmark_title_match_length(), 8);
  EXPECT_EQ(match.scoring_signals->num_input_terms_matched_by_bookmark_title(),
            3);
  EXPECT_EQ(match.scoring_signals->first_url_match_position(), 2);
  EXPECT_EQ(match.scoring_signals->total_url_match_length(), 5);
  EXPECT_TRUE(match.scoring_signals->host_match_at_word_boundary());
  EXPECT_EQ(match.scoring_signals->total_path_match_length(), 1);
  EXPECT_EQ(match.scoring_signals->total_query_or_ref_match_length(), 2);
  EXPECT_EQ(match.scoring_signals->total_title_match_length(), 5);
  EXPECT_TRUE(match.scoring_signals->has_non_scheme_www_match());
  EXPECT_EQ(match.scoring_signals->num_input_terms_matched_by_title(), 2);
  EXPECT_EQ(match.scoring_signals->num_input_terms_matched_by_url(), 2);
  EXPECT_EQ(match.scoring_signals->length_of_url(), 10);
  EXPECT_EQ(match.scoring_signals->site_engagement(), 0.6f);
  EXPECT_TRUE(match.scoring_signals->allowed_to_be_default_match());
}

TEST_F(AutocompleteMatchTest, SetAllowedToBeDefault) {
  // Test all combinations of:
  // 1) input text in ["goo", "goo ", "goo  "]
  // 2) input prevent_inline_autocomplete in [false, true]
  // 3) match inline_autocompletion in ["", "gle.com", " gle.com", "  gle.com"]
  // match_prefix_autocompletion will be "" for all these cases
  TestSetAllowedToBeDefault(1, "goo", false, "", "", "", true);
  TestSetAllowedToBeDefault(2, "goo", false, "gle.com", "", "gle.com", true);
  TestSetAllowedToBeDefault(3, "goo", false, " gle.com", "", " gle.com", true);
  TestSetAllowedToBeDefault(4, "goo", false, "  gle.com", "", "  gle.com",
                            true);
  TestSetAllowedToBeDefault(5, "goo ", false, "", "", "", true);
  TestSetAllowedToBeDefault(6, "goo ", false, "gle.com", "", "gle.com", false);
  TestSetAllowedToBeDefault(7, "goo ", false, " gle.com", "", "gle.com", true);
  TestSetAllowedToBeDefault(8, "goo ", false, "  gle.com", "", " gle.com",
                            true);
  TestSetAllowedToBeDefault(9, "goo  ", false, "", "", "", true);
  TestSetAllowedToBeDefault(10, "goo  ", false, "gle.com", "", "gle.com",
                            false);
  TestSetAllowedToBeDefault(11, "goo  ", false, " gle.com", "", " gle.com",
                            false);
  TestSetAllowedToBeDefault(12, "goo  ", false, "  gle.com", "", "gle.com",
                            true);
  TestSetAllowedToBeDefault(13, "goo", true, "", "", "", true);
  TestSetAllowedToBeDefault(14, "goo", true, "gle.com", "", "gle.com", false);
  TestSetAllowedToBeDefault(15, "goo", true, " gle.com", "", " gle.com", false);
  TestSetAllowedToBeDefault(16, "goo", true, "  gle.com", "", "  gle.com",
                            false);
  TestSetAllowedToBeDefault(17, "goo ", true, "", "", "", true);
  TestSetAllowedToBeDefault(18, "goo ", true, "gle.com", "", "gle.com", false);
  TestSetAllowedToBeDefault(19, "goo ", true, " gle.com", "", " gle.com",
                            false);
  TestSetAllowedToBeDefault(20, "goo ", true, "  gle.com", "", "  gle.com",
                            false);
  TestSetAllowedToBeDefault(21, "goo  ", true, "", "", "", true);
  TestSetAllowedToBeDefault(22, "goo  ", true, "gle.com", "", "gle.com", false);
  TestSetAllowedToBeDefault(23, "goo  ", true, " gle.com", "", " gle.com",
                            false);
  TestSetAllowedToBeDefault(24, "goo  ", true, "  gle.com", "", "  gle.com",
                            false);
}

TEST_F(AutocompleteMatchTest, SetAllowedToBeDefault_PrefixAutocompletion) {
  // Verify that a non-empty prefix autocompletion will prevent an empty inline
  // autocompletion from bypassing the other default match requirements.
  TestSetAllowedToBeDefault(0, "xyz", true, "", "prefix", "", false);
}

TEST_F(AutocompleteMatchTest, TryRichAutocompletion) {
  auto test = [](const std::string input_text,
                 bool input_prevent_inline_autocomplete,
                 const std::string primary_text,
                 const std::string secondary_text, bool shortcut_provider,
                 bool expected_return,
                 AutocompleteMatch::RichAutocompletionType
                     expected_rich_autocompletion_triggered,
                 const std::string expected_inline_autocompletion,
                 const std::string expected_prefix_autocompletion,
                 const std::string expected_additional_text,
                 bool expected_allowed_to_be_default_match) {
    AutocompleteInput input(base::UTF8ToUTF16(input_text),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_prevent_inline_autocomplete(input_prevent_inline_autocomplete);

    AutocompleteMatch match;
    EXPECT_EQ(
        match.TryRichAutocompletion(base::UTF8ToUTF16(primary_text),
                                    base::UTF8ToUTF16(secondary_text), input,
                                    shortcut_provider ? u"non-empty" : u""),
        expected_return);

    EXPECT_EQ(match.rich_autocompletion_triggered,
              expected_rich_autocompletion_triggered);

    EXPECT_EQ(base::UTF16ToUTF8(match.inline_autocompletion).c_str(),
              expected_inline_autocompletion);
    EXPECT_EQ(base::UTF16ToUTF8(match.prefix_autocompletion).c_str(),
              expected_prefix_autocompletion);
    EXPECT_EQ(base::UTF16ToUTF8(match.additional_text).c_str(),
              expected_additional_text);
    EXPECT_EQ(match.allowed_to_be_default_match,
              expected_allowed_to_be_default_match);
  };

  // We won't test every possible combination of rich autocompletion parameters,
  // but for now, only the state with all enabled. If we decide to launch a
  // different combination, we can update these tests.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kRichAutocompletion,
        {
            {"RichAutocompletionAutocompleteTitles", "true"},
            {"RichAutocompletionAutocompleteNonPrefixAll", "true"},
            {"RichAutocompletionAutocompleteTitlesMinChar", "0"},
            {"RichAutocompletionAutocompleteNonPrefixMinChar", "0"},
        });
    RichAutocompletionParams::ClearParamsForTesting();

    // Prefer autocompleting primary text prefix. Should not set
    // |rich_autocompletion_triggered|.
    {
      SCOPED_TRACE("primary prefix");
      test("x", false, "x_mixd_x_primary", "x_mixd_x_secondary", false, true,
           AutocompleteMatch::RichAutocompletionType::kNone, "_mixd_x_primary",
           "", "", true);
    }

    // Otherwise, prefer secondary text prefix.
    {
      SCOPED_TRACE("secondary prefix");
      test("x", false, "y_mixd_x_primary", "x_mixd_x_secondary", false, true,
           AutocompleteMatch::RichAutocompletionType::kTitlePrefix,
           "_mixd_x_secondary", "", "y_mixd_x_primary", true);
    }

    // Otherwise, prefer primary text non-prefix (wordbreak).
    {
      SCOPED_TRACE("primary non-prefix");
      test("x", false, "y_mixd_x_primary", "y_mixd_x_secondary", false, true,
           AutocompleteMatch::RichAutocompletionType::kUrlNonPrefix, "_primary",
           "y_mixd_", "", true);
    }

    // Otherwise, prefer secondary text non-prefix (wordbreak).
    {
      SCOPED_TRACE("secondary non-prefix");
      test("x", false, "y_mid_y_primary", "y_mixd_x_secondary", false, true,
           AutocompleteMatch::RichAutocompletionType::kTitleNonPrefix,
           "_secondary", "y_mixd_", "y_mid_y_primary", true);
    }

    // We don't explicitly test that non-wordbreak matches aren't autocompleted,
    // because we rely on providers to not provide suggestions that only match
    // the input at non-wordbreaks.

    // Otherwise, don't autocomplete but still set |additional_text|.
    {
      SCOPED_TRACE("no autocompletion applicable");
      test("x", false, "y_mid_y_primary", "y_mid_y_secondary", false, false,
           AutocompleteMatch::RichAutocompletionType::kNone, "", "", "", false);
    }

    // Don't autocomplete if |prevent_inline_autocomplete| is true.
    {
      SCOPED_TRACE("prevent inline autocomplete");
      test("x", true, "x_mixd_x_primary", "x_mixd_x_secondary", false, false,
           AutocompleteMatch::RichAutocompletionType::kNone, "", "", "", false);
    }
  }

  // Check min char limits.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kRichAutocompletion,
        {
            {"RichAutocompletionAutocompleteTitles", "true"},
            {"RichAutocompletionAutocompleteNonPrefixAll", "true"},
            {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
            {"RichAutocompletionAutocompleteNonPrefixMinChar", "2"},
        });
    RichAutocompletionParams::ClearParamsForTesting();

    // Do autocomplete URL non-prefix if input is greater than limits.
    {
      SCOPED_TRACE("min char shorter than input");
      test("x_prim", false, "y_mixd_x_primary", "x_mixd_x_secondary", false,
           true, AutocompleteMatch::RichAutocompletionType::kUrlNonPrefix,
           "ary", "y_mixd_", "", true);
    }

    // Usually, title autocompletion is preferred to non-prefix. Autocomplete
    // non-prefix if title autocompletion has a limit larger than the input.
    {
      SCOPED_TRACE(
          "title min char longer & non-prefix min char shorter than input");
      test("x_", false, "y_mixd_x_primary", "x_mixd_x_secondary", false, true,
           AutocompleteMatch::RichAutocompletionType::kUrlNonPrefix, "primary",
           "y_mixd_", "", true);
    }

    // Don't autocomplete title and non-prefix if input is less than limits.
    {
      SCOPED_TRACE("min char longer than input");
      test("x", false, "y_mixd_x_primary", "x_mixd_x_secondary", false, false,
           AutocompleteMatch::RichAutocompletionType::kNone, "", "", "", false);
    }
  }

  // Don't autocomplete if IsRichAutocompletionEnabled is disabled
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(omnibox::kRichAutocompletion);
    RichAutocompletionParams::ClearParamsForTesting();
    SCOPED_TRACE("feature disabled");
    test("x", false, "x_mixd_x_primary", "x_mixd_x_secondary", false, false,
         AutocompleteMatch::RichAutocompletionType::kNone, "", "", "", false);
  }

  // Don't autocomplete if the RichAutocompletionCounterfactual param is
  // enabled; do set |rich_autocompletion_triggered| if it would have
  // autocompleted.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kRichAutocompletion,
        {
            {"RichAutocompletionAutocompleteTitles", "true"},
            {"RichAutocompletionAutocompleteNonPrefixAll", "true"},
            {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
            {"RichAutocompletionAutocompleteNonPrefixMinChar", "2"},
            {"RichAutocompletionCounterfactual", "true"},
        });
    RichAutocompletionParams::ClearParamsForTesting();

    // Do trigger if input is greater than limits.
    {
      SCOPED_TRACE("min char shorter than input, counterfactual");
      test("x_prim", false, "y_mixd_x_primary", "x_mixd_x_secondary", false,
           false, AutocompleteMatch::RichAutocompletionType::kUrlNonPrefix, "",
           "", "", false);
    }

    {
      SCOPED_TRACE(
          "title min char longer & non-prefix min char shorter than input, "
          "counterfactual");
      test("x_", false, "y_mixd_x_primary", "x_mixd_x_secondary", false, false,
           AutocompleteMatch::RichAutocompletionType::kUrlNonPrefix, "", "", "",
           false);
    }

    // Don't trigger if input is less than limits.
    {
      SCOPED_TRACE("min char longer than input, counterfactual");
      test("x", false, "y_mixd_x_primary", "x_mixd_x_secondary", false, false,
           AutocompleteMatch::RichAutocompletionType::kNone, "", "", "", false);
    }
  }

  // Prefer non-prefix URLs to prefix title autocompletion only if the
  // appropriate param is set.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kRichAutocompletion,
        {
            {"RichAutocompletionAutocompleteTitles", "true"},
            {"RichAutocompletionAutocompleteNonPrefixAll", "true"},
            {"RichAutocompletionAutocompletePreferUrlsOverPrefixes", "true"},
        });
    RichAutocompletionParams::ClearParamsForTesting();

    {
      SCOPED_TRACE("prefer URLs over prefixes");
      test("x", false, "y_mixd_x_primary", "x_mixd_x_secondary", false, true,
           AutocompleteMatch::RichAutocompletionType::kUrlNonPrefix, "_primary",
           "y_mixd_", "", true);
    }
  }

  // Autocomplete only shortcut suggestions.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kRichAutocompletion,
        {
            {"RichAutocompletionAutocompleteTitlesShortcutProvider", "true"},
            {"RichAutocompletionAutocompleteNonPrefixShortcutProvider", "true"},
            {"RichAutocompletionAutocompleteTitlesMinChar", "0"},
            {"RichAutocompletionAutocompleteNonPrefixMinChar", "0"},
        });
    RichAutocompletionParams::ClearParamsForTesting();
    // Trigger if the suggestion is from the shortcut provider.
    {
      SCOPED_TRACE("shortcut");
      test("x", false, "primary x x", "x x secondary", true, true,
           AutocompleteMatch::RichAutocompletionType::kTitlePrefix,
           " x secondary", "", "primary x x", true);
    }

    // Don't trigger if the suggestion is not from the shortcut provider.
    {
      SCOPED_TRACE("not shortcut");
      test("x", false, "primary x x", "x x secondary", false, false,
           AutocompleteMatch::RichAutocompletionType::kNone, "", "", "", false);
    }
  }

  // Autocomplete inputs with spaces.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kRichAutocompletion,
        {
            {"RichAutocompletionAutocompleteTitles", "true"},
            {"RichAutocompletionAutocompleteNonPrefixAll", "true"},
        });
    RichAutocompletionParams::ClearParamsForTesting();
    {
      SCOPED_TRACE("input with spaces");
      test("x x", false, "primary x x", "secondary x x", true, true,
           AutocompleteMatch::RichAutocompletionType::kUrlNonPrefix, "",
           "primary ", "", true);
    }
  }
}

TEST_F(AutocompleteMatchTest, TryRichAutocompletionShortcutText) {
  auto test = [](const std::string input_text, const std::string primary_text,
                 const std::string secondary_text,
                 const std::string shortcut_text, bool expected_return,
                 AutocompleteMatch::RichAutocompletionType
                     expected_rich_autocompletion_triggered,
                 const std::string expected_inline_autocompletion,
                 const std::string expected_additional_text,
                 bool expected_allowed_to_be_default_match) {
    AutocompleteInput input(base::UTF8ToUTF16(input_text),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());

    AutocompleteMatch match;
    EXPECT_EQ(
        match.TryRichAutocompletion(base::UTF8ToUTF16(primary_text),
                                    base::UTF8ToUTF16(secondary_text), input,
                                    base::UTF8ToUTF16(shortcut_text)),
        expected_return);

    EXPECT_EQ(match.rich_autocompletion_triggered,
              expected_rich_autocompletion_triggered);

    EXPECT_EQ(base::UTF16ToUTF8(match.inline_autocompletion).c_str(),
              expected_inline_autocompletion);
    EXPECT_TRUE(match.prefix_autocompletion.empty());
    EXPECT_EQ(base::UTF16ToUTF8(match.additional_text).c_str(),
              expected_additional_text);
    EXPECT_EQ(match.allowed_to_be_default_match,
              expected_allowed_to_be_default_match);
  };

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kRichAutocompletion,
      {
          {"RichAutocompletionAutocompleteTitles", "true"},
          {"RichAutocompletionAutocompleteShortcutText", "true"},
      });
  RichAutocompletionParams::ClearParamsForTesting();

  // Prefer URL prefix AC when the input prefix matches the URL, title, and
  // shortcut text.
  {
    SCOPED_TRACE("URL");
    test("prefix", "prefix-url.com/suffix", "prefix title suffix",
         "prefix shortcut text suffix", true,
         AutocompleteMatch::RichAutocompletionType::kNone, "-url.com/suffix",
         "", true);
  }

  // Prefer title prefix AC when the input prefix matches the title and shortcut
  // text.
  {
    SCOPED_TRACE("Title");
    test("prefix ", "prefix-url.com/suffix", "prefix title suffix",
         "prefix shortcut text suffix", true,
         AutocompleteMatch::RichAutocompletionType::kTitlePrefix,
         "title suffix", "prefix-url.com/suffix", true);
  }

  // Do shortcut text prefix AC when title and URL don't prefix match, even if
  // they non-prefix match.
  {
    SCOPED_TRACE("Shortcut text");
    test("short", "url.com/shortcut", "title shortcut", "shortcut text", true,
         AutocompleteMatch::RichAutocompletionType::kShortcutTextPrefix,
         "cut text", "url.com/shortcut", true);
  }

  // Don't shortcut text AC when the shortcut text doesn't prefix match, even if
  // it does non-prefix match.
  {
    SCOPED_TRACE("None");
    test("suffix", "prefix-url.com/suffix", "prefix title suffix",
         "prefix shortcut text suffix", false,
         AutocompleteMatch::RichAutocompletionType::kNone, "", "", false);
  }
}

TEST_F(AutocompleteMatchTest, BetterDuplicate) {
  const auto create_match = [](scoped_refptr<FakeAutocompleteProvider> provider,
                               int relevance,
                               AutocompleteMatchType::Type match_type =
                                   AutocompleteMatchType::URL_WHAT_YOU_TYPED) {
    return AutocompleteMatch{provider.get(), relevance, false, match_type};
  };

  scoped_refptr<FakeAutocompleteProvider> document_provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_DOCUMENT);

  scoped_refptr<FakeAutocompleteProvider> bookmark_provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_BOOKMARK);

  scoped_refptr<FakeAutocompleteProvider> history_provider =
      new FakeAutocompleteProvider(
          AutocompleteProvider::Type::TYPE_HISTORY_QUICK);

  scoped_refptr<FakeAutocompleteProvider> shortcuts_provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_SHORTCUTS);

  scoped_refptr<FakeAutocompleteProvider> featured_search_provider =
      new FakeAutocompleteProvider(
          AutocompleteProvider::Type::TYPE_FEATURED_SEARCH);

  // Prefer document provider matches over other providers, even if scored
  // lower.
  EXPECT_TRUE(
      AutocompleteMatch::BetterDuplicate(create_match(document_provider, 0),
                                         create_match(history_provider, 1000)));

  // Prefer document provider matches over other providers, even if scored
  // lower.
  EXPECT_TRUE(
      AutocompleteMatch::BetterDuplicate(create_match(bookmark_provider, 0),
                                         create_match(history_provider, 1000)));

  // Prefer document provider matches over bookmark provider matches.
  EXPECT_TRUE(AutocompleteMatch::BetterDuplicate(
      create_match(document_provider, 0),
      create_match(bookmark_provider, 1000)));

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Prefer non-shortcuts provider matches over shortcuts provider matches.
  EXPECT_TRUE(AutocompleteMatch::BetterDuplicate(
      create_match(history_provider, 0),
      create_match(shortcuts_provider, 1000)));

  // Prefer featured enterprise search over other matches.
  EXPECT_TRUE(AutocompleteMatch::BetterDuplicate(
      create_match(featured_search_provider, 100,
                   AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH),
      create_match(featured_search_provider, 500,
                   AutocompleteMatchType::STARTER_PACK)));

  EXPECT_FALSE(AutocompleteMatch::BetterDuplicate(
      create_match(featured_search_provider, 500,
                   AutocompleteMatchType::STARTER_PACK),
      create_match(featured_search_provider, 100,
                   AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH)));

  EXPECT_TRUE(AutocompleteMatch::BetterDuplicate(
      create_match(featured_search_provider, 100,
                   AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH),
      create_match(bookmark_provider, 500)));

  EXPECT_FALSE(AutocompleteMatch::BetterDuplicate(
      create_match(bookmark_provider, 500),
      create_match(featured_search_provider, 100,
                   AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH)));

  // Prefer stater pack matches over other matches.
  EXPECT_TRUE(AutocompleteMatch::BetterDuplicate(
      create_match(featured_search_provider, 100,
                   AutocompleteMatchType::STARTER_PACK),
      create_match(bookmark_provider, 500)));

  EXPECT_FALSE(AutocompleteMatch::BetterDuplicate(
      create_match(bookmark_provider, 500),
      create_match(featured_search_provider, 100,
                   AutocompleteMatchType::STARTER_PACK)));
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // Prefer more relevant matches.
  EXPECT_FALSE(
      AutocompleteMatch::BetterDuplicate(create_match(history_provider, 500),
                                         create_match(history_provider, 510)));
}

TEST_F(AutocompleteMatchTest, FilterOmniboxActions) {
  scoped_refptr<FakeAutocompleteProvider> provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_SEARCH);
  const OmniboxAction::LabelStrings dummy_labels(u"", u"", u"", u"");

  using OmniboxActionId::ACTION_IN_SUGGEST;
  using OmniboxActionId::HISTORY_CLUSTERS;
  using OmniboxActionId::PEDAL;

  struct FilterOmniboxActionsTestData {
    std::string test_name;
    // This is what will get added to the AutocompleteMatch.
    std::vector<OmniboxActionId> actions_attached_to_match;
    // This is the filter. Order of elements specifies the preference.
    std::vector<OmniboxActionId> allowed_actions;
    // This is the expected result.
    std::vector<OmniboxActionId> resulting_actions;
  } test_cases[]{
      {"have nothing, want nothing", {}, {}, {}},
      {"have nothing, want Pedals", {}, {PEDAL}, {}},
      {"have Pedals, want nothing", {PEDAL}, {}, {}},
      {"have Pedals, want Pedals", {PEDAL}, {PEDAL}, {PEDAL}},
      {"have Pedals, want History Clusters", {PEDAL}, {HISTORY_CLUSTERS}, {}},
      {"have Pedals, want History Clusters, then Pedals",
       {PEDAL},
       {HISTORY_CLUSTERS, PEDAL},
       {PEDAL}},
      {"have Pedals and History Clusters, want History Clusters, then Pedals",
       {PEDAL, HISTORY_CLUSTERS},
       {HISTORY_CLUSTERS, PEDAL},
       {HISTORY_CLUSTERS}},
      {"have Pedals and History Clusters, want Pedals, then History Clusters",
       {PEDAL, HISTORY_CLUSTERS},
       {PEDAL, HISTORY_CLUSTERS},
       {PEDAL}},
      {"have Pedals and History Clusters, want Actions in Suggest",
       {PEDAL, HISTORY_CLUSTERS},
       {ACTION_IN_SUGGEST},
       {}},
      {"have Pedals and History Clusters, want Actions in Suggest, then Pedals",
       {PEDAL, HISTORY_CLUSTERS},
       {ACTION_IN_SUGGEST, PEDAL},
       {PEDAL}},
      {"have Pedals, Actions and History Clusters, want Pedals",
       {ACTION_IN_SUGGEST, PEDAL, HISTORY_CLUSTERS},
       {PEDAL},
       {PEDAL}},
      {"have multiple, want Actions, then History Clusters, then Pedals",
       // Mix: 4 pedals, 3 history clusters, 2 actions.
       {PEDAL, ACTION_IN_SUGGEST, HISTORY_CLUSTERS, PEDAL, ACTION_IN_SUGGEST,
        HISTORY_CLUSTERS, PEDAL, HISTORY_CLUSTERS, PEDAL},
       {ACTION_IN_SUGGEST, HISTORY_CLUSTERS, PEDAL},
       {ACTION_IN_SUGGEST, ACTION_IN_SUGGEST}},
      {"have multiple, want History Clusters, then Actions, then Pedals",
       // Mix: 4 pedals, 3 history clusters, 2 actions.
       {PEDAL, ACTION_IN_SUGGEST, HISTORY_CLUSTERS, PEDAL, ACTION_IN_SUGGEST,
        HISTORY_CLUSTERS, PEDAL, HISTORY_CLUSTERS, PEDAL},
       {HISTORY_CLUSTERS, ACTION_IN_SUGGEST, PEDAL},
       {HISTORY_CLUSTERS, HISTORY_CLUSTERS, HISTORY_CLUSTERS}},
      {"have multiple, want Pedals, then History Clusters, then Actions",
       // Mix: 4 pedals, 3 history clusters, 2 actions.
       {PEDAL, ACTION_IN_SUGGEST, HISTORY_CLUSTERS, PEDAL, ACTION_IN_SUGGEST,
        HISTORY_CLUSTERS, PEDAL, HISTORY_CLUSTERS, PEDAL},
       {PEDAL, HISTORY_CLUSTERS, ACTION_IN_SUGGEST},
       {PEDAL, PEDAL, PEDAL, PEDAL}},
      {"have multiple, want nothing",
       // Mix: 4 pedals, 3 history clusters, 2 actions.
       {PEDAL, ACTION_IN_SUGGEST, HISTORY_CLUSTERS, PEDAL, ACTION_IN_SUGGEST,
        HISTORY_CLUSTERS, PEDAL, HISTORY_CLUSTERS, PEDAL},
       {},
       {}}};

  for (const auto& test_case : test_cases) {
    AutocompleteMatch match(provider.get(), 1, false,
                            AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);

    // Populate match with requested actions.
    for (auto& action_id : test_case.actions_attached_to_match) {
      match.actions.push_back(
          base::MakeRefCounted<FakeOmniboxAction>(action_id));
    }

    match.FilterOmniboxActions(test_case.allowed_actions);
    EXPECT_EQ(match.actions.size(), test_case.resulting_actions.size())
        << "while testing variant: " << test_case.test_name;

    for (size_t index = 0u; index < match.actions.size(); ++index) {
      EXPECT_EQ(match.actions[index]->ActionId(),
                test_case.resulting_actions[index])
          << "while testing variant: " << test_case.test_name;
    }
  }
}

TEST_F(AutocompleteMatchTest, RearrangeActionsInSuggest) {
  scoped_refptr<FakeAutocompleteProvider> provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_SEARCH);
  const OmniboxAction::LabelStrings dummy_labels(u"", u"", u"", u"");

  using ActionType = omnibox::ActionInfo::ActionType;
  constexpr auto CALL = omnibox::ActionInfo_ActionType_CALL;
  constexpr auto NAV = omnibox::ActionInfo_ActionType_DIRECTIONS;
  constexpr auto REVS = omnibox::ActionInfo_ActionType_REVIEWS;

  struct FilterOmniboxActionsTestData {
    std::string test_name;
    // This is what will get added to the AutocompleteMatch.
    std::vector<ActionType> types_to_add;
    // Whether to show Reviews (true) or Calls (false) first.
    bool promote_reviews;
    // Retention variant to apply. See ActionsInSuggestRemoveActionTypes.
    const char* retention_variant;
    // This is the expected result (and order).
    std::vector<ActionType> types_to_expect;
  } test_cases[]{
      // clang-format off
      // Retain all
      {"retain all - no actions, promote reviews", {}, true, "", {}},
      {"retain all - no actions, promote calls", {}, false, "", {}},
      {"retain all - have no reviews, promote reviews",
       {CALL, CALL, CALL}, true, "", {CALL, CALL, CALL}},
      {"retain all - have reviews, promote reviews",
       {CALL, CALL, REVS}, true, "", {REVS, CALL, CALL}},
      {"retain all - have all types, promote reviews",
       {CALL, NAV, REVS}, true, "", {REVS, NAV, CALL}},
      {"retain all - have all types, promote calls",
       {CALL, NAV, REVS}, false, "", {CALL, NAV, REVS}},
      {"retain all - have multiple reviews, promote reviews",
       {REVS, NAV, REVS}, true, "", {REVS, REVS, NAV}},
      {"retain all - have multiple reviews, promote calls",
       {REVS, NAV, REVS}, false, "", {NAV, REVS, REVS}},

      // Prune calls.
      {"prine calls - no actions, promote reviews",
       {}, true, "call", {}},
      {"prune calls - no actions, promote calls",
       {}, false, "call", {}},
      {"prune calls - have no reviews, promote reviews",
       {CALL, CALL, CALL}, true, "call", {}},
      {"prune calls - have reviews, promote reviews",
       {CALL, CALL, REVS}, true, "call", {REVS}},
      {"prune calls - have all types, promote reviews",
       {CALL, NAV, REVS}, true, "call", {REVS, NAV}},
      {"prune calls - have all types, promote calls",
       {CALL, NAV, REVS}, false, "call", {NAV, REVS}},
      {"prune calls - have multiple reviews, promote reviews",
       {REVS, NAV, REVS}, true, "call", {REVS, REVS, NAV}},
      {"prune calls - have multiple reviews, promote calls",
       {REVS, NAV, REVS}, false, "call", {NAV, REVS, REVS}},

      // Prune directions.
      {"prune directions - no actions, promote reviews",
       {}, true, "directions", {}},
      {"prune directions - no actions, promote calls",
       {}, false, "directions", {}},
      {"prune directions - have no reviews, promote reviews",
       {CALL, CALL, CALL}, true, "directions", {CALL, CALL, CALL}},
      {"prune directions - have reviews, promote reviews",
       {CALL, CALL, REVS}, true, "directions", {REVS, CALL, CALL}},
      {"prune directions - have all types, promote reviews",
       {CALL, NAV, REVS}, true, "directions", {REVS, CALL}},
      {"prune directions - have all types, promote calls",
       {CALL, NAV, REVS}, false, "directions", {CALL, REVS}},
      {"prune directions - have multiple reviews, promote reviews",
       {REVS, NAV, REVS}, true, "directions", {REVS, REVS}},
      {"prune directions - have multiple reviews, promote calls",
       {REVS, NAV, REVS}, false, "directions", {REVS, REVS}},

      // Prune reviews.
      {"prune reviews - no actions, promote reviews",
       {}, true, "reviews", {}},
      {"prune reviews - no actions, promote calls",
       {}, false, "reviews", {}},
      {"prune reviews - have no reviews, promote reviews",
       {CALL, CALL, CALL}, true, "reviews", {CALL, CALL, CALL}},
      {"prune reviews - have reviews, promote reviews",
       {CALL, CALL, REVS}, true, "reviews", {CALL, CALL}},
      {"prune reviews - have all types, promote reviews",
       {CALL, NAV, REVS}, true, "reviews", {NAV, CALL}},
      {"prune reviews - have all types, promote calls",
       {CALL, NAV, REVS}, false, "reviews", {CALL, NAV}},
      {"prune reviews - have multiple reviews, promote reviews",
       {REVS, NAV, REVS}, true, "reviews", {NAV}},
      {"prune reviews - have multiple reviews, promote calls",
       {REVS, NAV, REVS}, true, "reviews", {NAV}},
      // clang-format on
  };

  for (const auto& test_case : test_cases) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kActionsInSuggest,
        {{OmniboxFieldTrial::kActionsInSuggestRemoveActionTypes.name,
          test_case.retention_variant},
         {OmniboxFieldTrial::kActionsInSuggestPromoteReviewsAction.name,
          test_case.promote_reviews ? "true" : "false"}});
    AutocompleteMatch match(provider.get(), 1, false,
                            AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);

    // Populate match with requested actions.
    for (auto& action_type : test_case.types_to_add) {
      omnibox::ActionInfo info;
      info.set_action_type(action_type);
      match.actions.push_back(base::MakeRefCounted<OmniboxActionInSuggest>(
          std::move(info), std::nullopt));
    }

    match.FilterAndSortActionsInSuggest();

    EXPECT_EQ(match.actions.size(), test_case.types_to_expect.size())
        << "while testing variant: " << test_case.test_name;

    for (size_t index = 0u; index < match.actions.size(); ++index) {
      const auto* action =
          OmniboxActionInSuggest::FromAction(match.actions[index].get());
      EXPECT_NE(nullptr, action)
          << "while testing variant: " << test_case.test_name;

      EXPECT_EQ(action->Type(), test_case.types_to_expect[index])
          << "at position " << index
          << " while testing variant: " << test_case.test_name;
    }
  }
}

#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
TEST_F(AutocompleteMatchTest, ValidateGetVectorIcons) {
  AutocompleteMatch match;

  // Irrespective of match type, bookmark suggestions should have a non-empty
  // icon.
  EXPECT_FALSE(match.GetVectorIcon(/*is_bookmark=*/true).is_empty());

  for (int type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
       type != AutocompleteMatchType::NUM_TYPES; type++) {
    match.type = static_cast<AutocompleteMatchType::Type>(type);

    if (match.type == AutocompleteMatchType::STARTER_PACK) {
      // All STARTER_PACK suggestions should have non-empty vector icons.
      for (int starter_pack_id = TemplateURLStarterPackData::kBookmarks;
           starter_pack_id != TemplateURLStarterPackData::kMaxStarterPackID;
           starter_pack_id++) {
        TemplateURLData turl_data;
        turl_data.starter_pack_id = starter_pack_id;
        TemplateURL turl(turl_data);
        EXPECT_FALSE(
            match.GetVectorIcon(/*is_bookmark=*/false, &turl).is_empty());
      }
    } else if (match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL ||
               (match.type == AutocompleteMatchType::NULL_RESULT_MESSAGE &&
                !match.IsIPHSuggestion())) {
      // SEARCH_SUGGEST_TAIL and non-IPH NULL_RESULT_MESSAGE suggestions use an
      // empty vector icon.
      EXPECT_TRUE(match.GetVectorIcon(/*is_bookmark=*/false).is_empty());
    } else {
      // All other suggestion types should result in non-empty vector icons.
      EXPECT_FALSE(match.GetVectorIcon(/*is_bookmark=*/false).is_empty());
    }
  }
}
#endif

TEST_F(AutocompleteMatchTest, IsClipboardType) {
  std::set<int> clipboard_types{AutocompleteMatchType::CLIPBOARD_TEXT,
                                AutocompleteMatchType::CLIPBOARD_URL,
                                AutocompleteMatchType::CLIPBOARD_IMAGE};

  for (int type = 0; type < AutocompleteMatchType::NUM_TYPES; type++) {
    EXPECT_EQ(
        AutocompleteMatch::IsClipboardType((AutocompleteMatchType::Type)type),
        clipboard_types.contains(type));
  }
}
