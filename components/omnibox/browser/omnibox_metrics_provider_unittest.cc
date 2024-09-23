// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_metrics_provider.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"
#include "ui/base/window_open_disposition.h"

using ScoringSignals = ::metrics::OmniboxEventProto::Suggestion::ScoringSignals;
using OmniboxScoringSignals = ::metrics::OmniboxScoringSignals;

class OmniboxMetricsProviderTest : public testing::Test {
 public:
  OmniboxMetricsProviderTest() = default;
  ~OmniboxMetricsProviderTest() override = default;

  void SetUp() override {
    provider_ = std::make_unique<OmniboxMetricsProvider>();
  }

  void TearDown() override { provider_.reset(); }

  OmniboxLog BuildOmniboxLog(const AutocompleteResult& result,
                             size_t selected_index) {
    return OmniboxLog(
        u"my text", /*just_deleted_text=*/false, metrics::OmniboxInputType::URL,
        /*in_keyword_mode=*/false,
        metrics::OmniboxEventProto_KeywordModeEntryMethod_INVALID,
        /*is_popup_open=*/false,
        /*selection=*/OmniboxPopupSelection(selected_index),
        WindowOpenDisposition::CURRENT_TAB, /*is_paste_and_go=*/false,
        SessionID::NewUnique(),
        metrics::OmniboxEventProto::PageClassification::
            OmniboxEventProto_PageClassification_NTP_REALBOX,
        /*elapsed_time_since_user_first_modified_omnibox=*/base::TimeDelta(),
        /*completed_length=*/0,
        /*elapsed_time_since_last_change_to_default_match=*/base::TimeDelta(),
        result, GURL("https://www.example.com/"), false);
  }

  AutocompleteMatch BuildMatch(AutocompleteMatch::Type type) {
    return AutocompleteMatch(nullptr, 0, false, type);
  }

  void RecordLogAndVerifyClientSummarizedResultType(
      const OmniboxLog& log,
      int32_t expected_uma_sample,
      int64_t expected_ukm_value) {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    provider_->RecordOmniboxOpenedURLClientSummarizedResultType(log);

    // Verify the UMA histogram.
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionUsed.ClientSummarizedResultType",
        expected_uma_sample,
        /*expected_count=*/1);

    // Verify the UKM event.
    const char* entry_name = ukm::builders::Omnibox_SuggestionUsed::kEntryName;
    if (log.ukm_source_id != ukm::kInvalidSourceId) {
      EXPECT_EQ(ukm_recorder.GetEntriesByName(entry_name).size(), 1ul);
      auto* entry = ukm_recorder.GetEntriesByName(entry_name)[0].get();
      ukm_recorder.ExpectEntryMetric(
          entry, ukm::builders::Omnibox_SuggestionUsed::kResultTypeName,
          expected_ukm_value);
    } else {
      EXPECT_EQ(ukm_recorder.GetEntriesByName(entry_name).size(), 0ul);
    }
  }

  void RecordLogAndVerifyScoringSignals(
      const OmniboxLog& log,
      OmniboxScoringSignals& expected_scoring_signals) {
    // Clear the event cache so we start with a clean slate.
    provider_->omnibox_events_cache.clear_omnibox_event();

    provider_->RecordOmniboxOpenedURL(log);

    EXPECT_EQ(provider_->omnibox_events_cache.omnibox_event_size(), 1);
    const metrics::OmniboxEventProto& omnibox_event =
        provider_->omnibox_events_cache.omnibox_event(0);

    for (int i = 0; i < omnibox_event.suggestion_size(); i++) {
      const metrics::OmniboxEventProto::Suggestion& suggestion =
          omnibox_event.suggestion(i);
      // Scoring signals should not be logged when in incognito/off-the-record
      // mode, regardless of result type.
      if (log.is_incognito) {
        EXPECT_FALSE(suggestion.has_scoring_signals());
        continue;
      }

      // When not in incognito, scoring signals should only be logged for the
      // proper suggestion types. Check that the signals are logged correctly
      // for URL types and Search types, while not being logged for any others.
      if (suggestion.has_result_type()) {
        EXPECT_TRUE(suggestion.has_scoring_signals());

        if (suggestion.result_type() ==
            metrics::
                OmniboxEventProto_Suggestion_ResultType_SEARCH_WHAT_YOU_TYPED) {
          EXPECT_EQ(suggestion.scoring_signals().search_suggest_relevance(),
                    expected_scoring_signals.search_suggest_relevance());
          EXPECT_EQ(suggestion.scoring_signals().is_search_suggest_entity(),
                    expected_scoring_signals.is_search_suggest_entity());
        } else {
          EXPECT_EQ(
              suggestion.scoring_signals()
                  .first_bookmark_title_match_position(),
              expected_scoring_signals.first_bookmark_title_match_position());
          EXPECT_EQ(suggestion.scoring_signals().allowed_to_be_default_match(),
                    expected_scoring_signals.allowed_to_be_default_match());
          EXPECT_EQ(suggestion.scoring_signals().length_of_url(),
                    expected_scoring_signals.length_of_url());
        }
      } else {
        EXPECT_FALSE(suggestion.has_scoring_signals());
      }
    }

    // Clear the event cache.
    provider_->omnibox_events_cache.clear_omnibox_event();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<OmniboxMetricsProvider> provider_;
};

TEST_F(OmniboxMetricsProviderTest, ClientSummarizedResultTypeSingleURL) {
  AutocompleteResult result;
  result.AppendMatches(
      {BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED)});
  OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0);
  log.ukm_source_id = ukm::NoURLSourceId();
  RecordLogAndVerifyClientSummarizedResultType(log, /*expected_uma_sample=*/0,
                                               /*expected_ukm_value=*/0);
}

TEST_F(OmniboxMetricsProviderTest, ClientSummarizedResultTypeSingleSearch) {
  AutocompleteResult result;
  result.AppendMatches({BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST)});
  OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0);
  log.ukm_source_id = ukm::NoURLSourceId();
  RecordLogAndVerifyClientSummarizedResultType(log, /*expected_uma_sample=*/1,
                                               /*expected_ukm_value=*/1);
}

TEST_F(OmniboxMetricsProviderTest, ClientSummarizedResultTypeMultipleSearch) {
  AutocompleteResult result;
  result.AppendMatches(
      {BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED),
       BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST),
       BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED)});
  OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/1);
  log.ukm_source_id = ukm::NoURLSourceId();
  RecordLogAndVerifyClientSummarizedResultType(log, /*expected_uma_sample=*/1,
                                               /*expected_ukm_value=*/1);
}

TEST_F(OmniboxMetricsProviderTest,
       ClientSummarizedResultTypeInvalidUkmSourceId) {
  AutocompleteResult result;
  result.AppendMatches(
      {BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED)});
  OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0);
  RecordLogAndVerifyClientSummarizedResultType(log, /*expected_uma_sample=*/0,
                                               /*expected_ukm_value=*/0);
}

// TODO(b/261895038): This test is flaky on android.  Currently scoring signals
// logging is only enabled on desktop, so disable for mobile.
#if !(BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID))
TEST_F(OmniboxMetricsProviderTest, LogScoringSignals) {
  // Enable feature flag to log scoring signals.
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;
  scoped_ml_config.GetMLConfig().log_url_scoring_signals = true;

  // Populate a set of scoring signals with some test values. This will be used
  // to ensure the scoring signals are being propagated correctly.
  OmniboxScoringSignals expected_url_scoring_signals;
  expected_url_scoring_signals.set_first_bookmark_title_match_position(3);
  expected_url_scoring_signals.set_allowed_to_be_default_match(true);
  expected_url_scoring_signals.set_length_of_url(20);

  OmniboxScoringSignals expected_search_scoring_signals;
  expected_search_scoring_signals.set_search_suggest_relevance(1000);
  expected_search_scoring_signals.set_is_search_suggest_entity(true);

  // Create matches and populate the scoring signals. Signals should only be
  // logged for non-search suggestions.
  ACMatches matches = {
      BuildMatch(AutocompleteMatchType::Type::BOOKMARK_TITLE),
      BuildMatch(AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED)};
  for (auto& match : matches) {
    match.scoring_signals = AutocompleteMatch::IsSearchHistoryType(match.type)
                                ? expected_search_scoring_signals
                                : expected_url_scoring_signals;
  }
  AutocompleteResult result;
  result.AppendMatches(matches);

  // Create the log and call simulate logging.
  OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/1);
  RecordLogAndVerifyScoringSignals(log, *matches[0].scoring_signals);

  // Now, "turn on" incognito mode, scoring signals should not be logged.
  log.is_incognito = true;
  RecordLogAndVerifyScoringSignals(log, *matches[0].scoring_signals);
}
#endif  // !(BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID))
