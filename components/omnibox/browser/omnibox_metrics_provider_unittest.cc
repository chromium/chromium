// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_metrics_provider.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/window_open_disposition.h"

using ScoringSignals = ::metrics::OmniboxEventProto::Suggestion::ScoringSignals;

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
        /*is_popup_open=*/false, /*selected_index=*/selected_index,
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

  void RecordLogAndVerifyClientSummarizedResultType(const OmniboxLog& log,
                                                    int32_t sample,
                                                    int32_t expected_count) {
    base::HistogramTester histogram_tester;
    provider_->RecordOmniboxOpenedURLClientSummarizedResultType(log);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionUsed.ClientSummarizedResultType", sample,
        expected_count);
  }

  void RecordLogAndVerifyScoringSignals(
      const OmniboxLog& log,
      ScoringSignals& expected_scoring_signals) {
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
        ASSERT_FALSE(suggestion.has_scoring_signals());
        continue;
      }

      // When not in incognito, scoring signals should only be logged for URL
      // (not search) types. Check that the signals are logged correctly for URL
      // types, and not logged at all for search types.
      if (suggestion.has_result_type() &&
          suggestion.result_type() !=
              metrics::
                  OmniboxEventProto_Suggestion_ResultType_SEARCH_WHAT_YOU_TYPED) {
        ASSERT_TRUE(suggestion.has_scoring_signals());
        ASSERT_EQ(
            expected_scoring_signals.first_bookmark_title_match_position(),
            suggestion.scoring_signals().first_bookmark_title_match_position());
        ASSERT_EQ(expected_scoring_signals.allowed_to_be_default_match(),
                  suggestion.scoring_signals().allowed_to_be_default_match());
        ASSERT_EQ(expected_scoring_signals.length_of_url(),
                  suggestion.scoring_signals().length_of_url());
      } else {
        ASSERT_FALSE(suggestion.has_scoring_signals());
      }
    }

    // Clear the event cache.
    provider_->omnibox_events_cache.clear_omnibox_event();
  }

 protected:
  std::unique_ptr<OmniboxMetricsProvider> provider_;
};

TEST_F(OmniboxMetricsProviderTest, ClientSummarizedResultTypeSingleURL) {
  AutocompleteResult result;
  result.AppendMatches(
      {BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED)});
  OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0);
  RecordLogAndVerifyClientSummarizedResultType(log, /*sample=*/0,
                                               /*expected_count=*/1);
}

TEST_F(OmniboxMetricsProviderTest, ClientSummarizedResultTypeSingleSearch) {
  AutocompleteResult result;
  result.AppendMatches({BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST)});
  OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0);
  RecordLogAndVerifyClientSummarizedResultType(log, /*sample=*/1,
                                               /*expected_count=*/1);
}

TEST_F(OmniboxMetricsProviderTest, ClientSummarizedResultTypeMultipleSearch) {
  AutocompleteResult result;
  result.AppendMatches(
      {BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED),
       BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST),
       BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED)});
  OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/1);
  RecordLogAndVerifyClientSummarizedResultType(log, /*sample=*/1,
                                               /*expected_count=*/1);
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
  ScoringSignals expected_scoring_signals;
  expected_scoring_signals.set_first_bookmark_title_match_position(3);
  expected_scoring_signals.set_allowed_to_be_default_match(true);
  expected_scoring_signals.set_length_of_url(20);

  // Create matches and populate the scoring signals. Signals should only be
  // logged for non-search suggestions.
  ACMatches matches = {
      BuildMatch(AutocompleteMatchType::Type::BOOKMARK_TITLE),
      BuildMatch(AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED)};
  for (auto& match : matches) {
    match.scoring_signals = expected_scoring_signals;
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
