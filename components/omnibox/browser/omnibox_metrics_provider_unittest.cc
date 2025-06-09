// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_metrics_provider.h"

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/actions/contextual_search_action.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
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
    autocomplete_provider_ =
        new FakeAutocompleteProvider(AutocompleteProvider::TYPE_SEARCH);
    metrics_provider_ = std::make_unique<OmniboxMetricsProvider>();
  }

  void TearDown() override { metrics_provider_.reset(); }

  OmniboxLog BuildOmniboxLog(const AutocompleteResult& result,
                             size_t selected_index,
                             SessionData session_data) {
    return OmniboxLog(
        /*text=*/u"my text", /*just_deleted_text=*/false,
        /*input_type=*/metrics::OmniboxInputType::URL,
        /*in_keyword_mode=*/false,
        /*entry_method=*/
        metrics::OmniboxEventProto_KeywordModeEntryMethod_INVALID,
        /*is_popup_open=*/false,
        /*selection=*/OmniboxPopupSelection(selected_index),
        /*disposition=*/WindowOpenDisposition::CURRENT_TAB,
        /*is_paste_and_go=*/false,
        /*tab_id=*/SessionID::NewUnique(),
        /*current_page_classification=*/
        metrics::OmniboxEventProto_PageClassification_NTP_REALBOX,
        /*elapsed_time_since_user_first_modified_omnibox=*/base::TimeDelta(),
        /*completed_length=*/0,
        /*elapsed_time_since_last_change_to_default_match=*/base::TimeDelta(),
        /*result=*/result, /*destination_url=*/GURL("https://www.example.com/"),
        /*is_incognito=*/false,
        /*is_zero_suggest=*/false,
        /*session=*/session_data);
  }

  AutocompleteMatch BuildMatch(AutocompleteMatch::Type type) {
    return AutocompleteMatch(autocomplete_provider_.get(), /*relevance=*/0,
                             /*deletable=*/false, type);
  }

  void RecordMetrics(const OmniboxLog& log) {
    metrics_provider_->RecordMetrics(log);
  }

  void RecordContextualSearchPrecisionRecallUsage(const OmniboxLog& log) {
    metrics_provider_->RecordContextualSearchPrecisionRecallUsage(log);
  }

  void RecordLogAndVerifyScoringSignals(
      const OmniboxLog& log,
      OmniboxScoringSignals& expected_scoring_signals) {
    // Clear the event cache so we start with a clean slate.
    metrics_provider_->omnibox_events_cache.clear_omnibox_event();

    metrics_provider_->RecordOmniboxEvent(log);

    EXPECT_EQ(metrics_provider_->omnibox_events_cache.omnibox_event_size(), 1);
    const metrics::OmniboxEventProto& omnibox_event =
        metrics_provider_->omnibox_events_cache.omnibox_event(0);

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
    metrics_provider_->omnibox_events_cache.clear_omnibox_event();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<FakeAutocompleteProvider> autocomplete_provider_;
  std::unique_ptr<OmniboxMetricsProvider> metrics_provider_;
};

TEST_F(OmniboxMetricsProviderTest, RecordMetrics_SingleURL) {
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    AutocompleteResult result;
    result.AppendMatches(
        {BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED)});
    SessionData session;
    session.typed_suggestions_shown_in_session = true;
    session.typed_url_suggestions_shown_in_session = true;
    OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0,
                                     /*session_data=*/session);
    log.ukm_source_id = ukm::NoURLSourceId();
    RecordMetrics(log);

    // Verify the UMA histograms.
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionUsed.ClientSummarizedResultType",
        ClientSummarizedResultType::kUrl,
        /*expected_count=*/1);

    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType",
        ClientSummarizedResultType::kUrl, /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType.ByPageContext.NTP_"
        "REALBOX",
        ClientSummarizedResultType::kUrl, /*expected_count=*/1);

    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.TypedSuggest.ClientSummarizedResultType",
        ClientSummarizedResultType::kUrl, /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.TypedSuggest.ClientSummarizedResultType."
        "ByPageContext.NTP_REALBOX",
        ClientSummarizedResultType::kUrl, /*expected_count=*/1);

    // Verify the UKM event.
    const char* entry_name = ukm::builders::Omnibox_SuggestionUsed::kEntryName;
    EXPECT_EQ(ukm_recorder.GetEntriesByName(entry_name).size(), 1ul);
    auto* entry = ukm_recorder.GetEntriesByName(entry_name)[0].get();
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kResultTypeGroupName,
        static_cast<uint64_t>(ClientSummarizedResultType::kUrl));
  }

  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    AutocompleteResult result;
    result.AppendMatches(
        {BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED)});
    SessionData session;
    session.zero_prefix_suggestions_shown_in_session = true;
    session.zero_prefix_url_suggestions_shown_in_session = true;
    OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0,
                                     /*session_data=*/session);
    log.text = u"";
    log.ukm_source_id = ukm::NoURLSourceId();
    RecordMetrics(log);

    // Verify the UMA histograms.
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionUsed.ClientSummarizedResultType",
        ClientSummarizedResultType::kUrl,
        /*expected_count=*/1);

    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType",
        ClientSummarizedResultType::kUrl, /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType.ByPageContext.NTP_"
        "REALBOX",
        ClientSummarizedResultType::kUrl, /*expected_count=*/1);

    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ZeroSuggest.ClientSummarizedResultType",
        ClientSummarizedResultType::kUrl, /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ZeroSuggest.ClientSummarizedResultType."
        "ByPageContext.NTP_REALBOX",
        ClientSummarizedResultType::kUrl, /*expected_count=*/1);

    // Verify the UKM event.
    const char* entry_name = ukm::builders::Omnibox_SuggestionUsed::kEntryName;
    EXPECT_EQ(ukm_recorder.GetEntriesByName(entry_name).size(), 1ul);
    auto* entry = ukm_recorder.GetEntriesByName(entry_name)[0].get();
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kResultTypeGroupName,
        static_cast<uint64_t>(ClientSummarizedResultType::kUrl));
  }
}

TEST_F(OmniboxMetricsProviderTest, RecordMetrics_SingleSearch) {
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    AutocompleteResult result;
    result.AppendMatches({BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST)});
    SessionData session;
    session.typed_suggestions_shown_in_session = true;
    session.typed_search_suggestions_shown_in_session = true;
    OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0,
                                     /*session_data=*/session);
    log.ukm_source_id = ukm::NoURLSourceId();
    RecordMetrics(log);

    // Verify the UMA histograms.
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionUsed.ClientSummarizedResultType",
        ClientSummarizedResultType::kSearch,
        /*expected_count=*/1);

    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType",
        ClientSummarizedResultType::kSearch, /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType.ByPageContext.NTP_"
        "REALBOX",
        ClientSummarizedResultType::kSearch, /*expected_count=*/1);

    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.TypedSuggest.ClientSummarizedResultType",
        ClientSummarizedResultType::kSearch, /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.TypedSuggest.ClientSummarizedResultType."
        "ByPageContext.NTP_REALBOX",
        ClientSummarizedResultType::kSearch, /*expected_count=*/1);

    // Verify the UKM event.
    const char* entry_name = ukm::builders::Omnibox_SuggestionUsed::kEntryName;
    EXPECT_EQ(ukm_recorder.GetEntriesByName(entry_name).size(), 1ul);
    auto* entry = ukm_recorder.GetEntriesByName(entry_name)[0].get();
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kResultTypeGroupName,
        static_cast<uint64_t>(ClientSummarizedResultType::kSearch));
  }

  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    AutocompleteResult result;
    result.AppendMatches({BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST)});
    SessionData session;
    session.zero_prefix_suggestions_shown_in_session = true;
    session.zero_prefix_search_suggestions_shown_in_session = true;
    OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0,
                                     /*session_data=*/session);
    log.ukm_source_id = ukm::NoURLSourceId();
    RecordMetrics(log);

    // Verify the UMA histograms.
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionUsed.ClientSummarizedResultType",
        ClientSummarizedResultType::kSearch,
        /*expected_count=*/1);

    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType",
        ClientSummarizedResultType::kSearch, /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType.ByPageContext.NTP_"
        "REALBOX",
        ClientSummarizedResultType::kSearch, /*expected_count=*/1);

    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ZeroSuggest.ClientSummarizedResultType",
        ClientSummarizedResultType::kSearch, /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ZeroSuggest.ClientSummarizedResultType."
        "ByPageContext.NTP_REALBOX",
        ClientSummarizedResultType::kSearch, /*expected_count=*/1);

    // Verify the UKM event.
    const char* entry_name = ukm::builders::Omnibox_SuggestionUsed::kEntryName;
    EXPECT_EQ(ukm_recorder.GetEntriesByName(entry_name).size(), 1ul);
    auto* entry = ukm_recorder.GetEntriesByName(entry_name)[0].get();
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kResultTypeGroupName,
        static_cast<uint64_t>(ClientSummarizedResultType::kSearch));
  }
}

TEST_F(OmniboxMetricsProviderTest, RecordContextualSearchMetrics) {
  // Contextual search suggestion shown, but not selected.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    AutocompleteMatch match =
        BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST);
    match.subtypes.insert(omnibox::SUBTYPE_CONTEXTUAL_SEARCH);
    match.takeover_action =
        base::MakeRefCounted<ContextualSearchFulfillmentAction>(
            match.destination_url, match.type, true);

    AutocompleteResult result;
    result.AppendMatches(
        {BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST), match});

    SessionData session;
    session.contextual_search_suggestions_shown_in_session = true;

    OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0,
                                     /*session_data=*/session);
    log.ukm_source_id = ukm::NoURLSourceId();

    RecordContextualSearchPrecisionRecallUsage(log);

    // Verify the UMA histograms.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ContextualSearchSuggestion.Precision", /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ContextualSearchSuggestion.Precision", false,
        /*expected_count=*/1);

    histogram_tester.ExpectTotalCount(
        "Omnibox.ContextualSearchSuggestion.Recall",
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ContextualSearchSuggestion.Recall", true,
        /*expected_count=*/1);

    histogram_tester.ExpectTotalCount(
        "Omnibox.ContextualSearchSuggestion.Usage",
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ContextualSearchSuggestion.Usage", false,
        /*expected_count=*/1);

    RecordMetrics(log);

    // Verify the UKM event and the full set of metrics.
    const char* entry_name = ukm::builders::Omnibox_SuggestionUsed::kEntryName;
    EXPECT_EQ(ukm_recorder.GetEntriesByName(entry_name).size(), 1ul);
    auto* entry = ukm_recorder.GetEntriesByName(entry_name)[0].get();
    ukm_recorder.ExpectEntryMetric(entry,
                                   ukm::builders::Omnibox_SuggestionUsed::
                                       kZeroPrefixContextualSearchShownName,
                                   true);
    ukm_recorder.ExpectEntryMetric(
        entry,
        ukm::builders::Omnibox_SuggestionUsed::kZeroPrefixLensActionShownName,
        false);
  }

  // Contextual search suggestion shown and selected.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    AutocompleteMatch match =
        BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST);
    match.subtypes.insert(omnibox::SUBTYPE_CONTEXTUAL_SEARCH);
    match.takeover_action =
        base::MakeRefCounted<ContextualSearchFulfillmentAction>(
            match.destination_url, match.type, true);

    AutocompleteResult result;
    result.AppendMatches(
        {BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST), match});

    SessionData session;
    session.contextual_search_suggestions_shown_in_session = true;

    OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/1,
                                     /*session_data=*/session);
    log.ukm_source_id = ukm::NoURLSourceId();

    RecordContextualSearchPrecisionRecallUsage(log);

    // Verify the UMA histograms.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ContextualSearchSuggestion.Precision", /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ContextualSearchSuggestion.Precision", true,
        /*expected_count=*/1);

    histogram_tester.ExpectTotalCount(
        "Omnibox.ContextualSearchSuggestion.Recall",
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ContextualSearchSuggestion.Recall", true,
        /*expected_count=*/1);

    histogram_tester.ExpectTotalCount(
        "Omnibox.ContextualSearchSuggestion.Usage",
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ContextualSearchSuggestion.Usage", true, /*expected_count=*/1);

    RecordMetrics(log);

    // Verify the UKM event and the full set of metrics.
    const char* entry_name = ukm::builders::Omnibox_SuggestionUsed::kEntryName;
    EXPECT_EQ(ukm_recorder.GetEntriesByName(entry_name).size(), 1ul);
    auto* entry = ukm_recorder.GetEntriesByName(entry_name)[0].get();
    ukm_recorder.ExpectEntryMetric(entry,
                                   ukm::builders::Omnibox_SuggestionUsed::
                                       kZeroPrefixContextualSearchShownName,
                                   true);
    ukm_recorder.ExpectEntryMetric(
        entry,
        ukm::builders::Omnibox_SuggestionUsed::kZeroPrefixLensActionShownName,
        false);
  }

  // Google Lens action shown, but not selected.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    AutocompleteMatch match =
        BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST);
    match.subtypes.insert(omnibox::SUBTYPE_CONTEXTUAL_SEARCH);
    match.takeover_action =
        base::MakeRefCounted<ContextualSearchOpenLensAction>();

    AutocompleteResult result;
    result.AppendMatches(
        {BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST), match});

    SessionData session;
    session.lens_action_shown_in_session = true;

    OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/0,
                                     /*session_data=*/session);
    log.ukm_source_id = ukm::NoURLSourceId();

    RecordContextualSearchPrecisionRecallUsage(log);

    // Verify the UMA histograms.
    histogram_tester.ExpectTotalCount("Omnibox.LensAction.Precision", 1);
    histogram_tester.ExpectBucketCount("Omnibox.LensAction.Precision", false,
                                       /*expected_count=*/1);

    histogram_tester.ExpectTotalCount("Omnibox.LensAction.Recall", 1);
    histogram_tester.ExpectBucketCount("Omnibox.LensAction.Recall", true,
                                       /*expected_count=*/1);

    histogram_tester.ExpectTotalCount("Omnibox.LensAction.Usage", 1);
    histogram_tester.ExpectBucketCount("Omnibox.LensAction.Usage", false,
                                       /*expected_count=*/1);

    RecordMetrics(log);

    // Verify the UKM event and the full set of metrics.
    const char* entry_name = ukm::builders::Omnibox_SuggestionUsed::kEntryName;
    EXPECT_EQ(ukm_recorder.GetEntriesByName(entry_name).size(), 1ul);
    auto* entry = ukm_recorder.GetEntriesByName(entry_name)[0].get();
    ukm_recorder.ExpectEntryMetric(entry,
                                   ukm::builders::Omnibox_SuggestionUsed::
                                       kZeroPrefixContextualSearchShownName,
                                   false);
    ukm_recorder.ExpectEntryMetric(
        entry,
        ukm::builders::Omnibox_SuggestionUsed::kZeroPrefixLensActionShownName,
        true);
  }

  // Google Lens action shown and selected.
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    AutocompleteMatch match =
        BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST);
    match.subtypes.insert(omnibox::SUBTYPE_CONTEXTUAL_SEARCH);
    match.takeover_action =
        base::MakeRefCounted<ContextualSearchOpenLensAction>();

    AutocompleteResult result;
    result.AppendMatches(
        {BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST), match});

    SessionData session;
    session.lens_action_shown_in_session = true;

    OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/1,
                                     /*session_data=*/session);
    log.ukm_source_id = ukm::NoURLSourceId();

    RecordContextualSearchPrecisionRecallUsage(log);

    // Verify the UMA histograms.
    histogram_tester.ExpectTotalCount("Omnibox.LensAction.Precision", 1);
    histogram_tester.ExpectBucketCount("Omnibox.LensAction.Precision", true,
                                       /*expected_count=*/1);

    histogram_tester.ExpectTotalCount("Omnibox.LensAction.Recall", 1);
    histogram_tester.ExpectBucketCount("Omnibox.LensAction.Recall", true,
                                       /*expected_count=*/1);

    histogram_tester.ExpectTotalCount("Omnibox.LensAction.Usage", 1);
    histogram_tester.ExpectBucketCount("Omnibox.LensAction.Usage", true,
                                       /*expected_count=*/1);

    RecordMetrics(log);

    // Verify the UKM event and the full set of metrics.
    const char* entry_name = ukm::builders::Omnibox_SuggestionUsed::kEntryName;
    EXPECT_EQ(ukm_recorder.GetEntriesByName(entry_name).size(), 1ul);
    auto* entry = ukm_recorder.GetEntriesByName(entry_name)[0].get();
    ukm_recorder.ExpectEntryMetric(entry,
                                   ukm::builders::Omnibox_SuggestionUsed::
                                       kZeroPrefixContextualSearchShownName,
                                   false);
    ukm_recorder.ExpectEntryMetric(
        entry,
        ukm::builders::Omnibox_SuggestionUsed::kZeroPrefixLensActionShownName,
        true);
  }
}

TEST_F(OmniboxMetricsProviderTest, RecordMetrics_MultipleSearch) {
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    AutocompleteResult result;
    result.AppendMatches(
        {BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED),
         BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST),
         BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED)});
    SessionData session;
    session.typed_suggestions_shown_in_session = true;
    session.typed_search_suggestions_shown_in_session = true;
    session.typed_url_suggestions_shown_in_session = true;
    OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/1,
                                     /*session_data=*/session);
    log.ukm_source_id = ukm::NoURLSourceId();
    log.elapsed_time_since_user_focused_omnibox = base::Milliseconds(10);
    RecordMetrics(log);

    // Verify the UMA histograms.
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionUsed.ClientSummarizedResultType",
        ClientSummarizedResultType::kSearch,
        /*expected_count=*/1);

    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType",
        ClientSummarizedResultType::kSearch,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType.ByPageContext.NTP_"
        "REALBOX",
        ClientSummarizedResultType::kSearch,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.TypedSuggest.ClientSummarizedResultType",
        ClientSummarizedResultType::kSearch,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.TypedSuggest.ClientSummarizedResultType."
        "ByPageContext.NTP_REALBOX",
        ClientSummarizedResultType::kSearch,
        /*expected_count=*/1);

    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType",
        ClientSummarizedResultType::kUrl,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType.ByPageContext.NTP_"
        "REALBOX",
        ClientSummarizedResultType::kUrl,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.TypedSuggest.ClientSummarizedResultType",
        ClientSummarizedResultType::kUrl,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.TypedSuggest.ClientSummarizedResultType."
        "ByPageContext.NTP_REALBOX",
        ClientSummarizedResultType::kUrl,
        /*expected_count=*/1);

    // Verify the UKM event and the full set of metrics.
    const char* entry_name = ukm::builders::Omnibox_SuggestionUsed::kEntryName;
    EXPECT_EQ(ukm_recorder.GetEntriesByName(entry_name).size(), 1ul);
    auto* entry = ukm_recorder.GetEntriesByName(entry_name)[0].get();
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kPageClassificationName,
        static_cast<uint64_t>(
            metrics::OmniboxEventProto_PageClassification_NTP_REALBOX));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kProviderTypeName,
        static_cast<uint64_t>(metrics::OmniboxEventProto_ProviderType_SEARCH));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kResultTypeName,
        static_cast<uint64_t>(
            metrics::OmniboxEventProto_Suggestion::SEARCH_SUGGEST));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kResultTypeGroupName,
        static_cast<uint64_t>(ClientSummarizedResultType::kSearch));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kSelectedIndexName, 1ul);
    // With exponential bucketing scheme with a standard spacing of 2.0, 10
    // falls into the 8-16 bucket as the boundaries of the buckets increase
    // exponentially, e.g., 1, 2, 4, 8, 16, etc.
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kTimeSinceLastFocusMsName,
        8ul);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kTypedLengthName, 7ul);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kTypingDurationMsName,
        0ul);
    ukm_recorder.ExpectEntryMetric(
        entry,
        ukm::builders::Omnibox_SuggestionUsed::kZeroPrefixSearchShownName,
        false);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kZeroPrefixUrlShownName,
        false);
    ukm_recorder.ExpectEntryMetric(entry,
                                   ukm::builders::Omnibox_SuggestionUsed::
                                       kZeroPrefixContextualSearchShownName,
                                   false);
    ukm_recorder.ExpectEntryMetric(
        entry,
        ukm::builders::Omnibox_SuggestionUsed::kZeroPrefixLensActionShownName,
        false);
  }
  {
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder ukm_recorder;

    AutocompleteResult result;
    result.AppendMatches({BuildMatch(AutocompleteMatch::Type::HISTORY_URL),
                          BuildMatch(AutocompleteMatch::Type::SEARCH_SUGGEST),
                          BuildMatch(AutocompleteMatch::Type::HISTORY_URL)});
    SessionData session;
    session.zero_prefix_suggestions_shown_in_session = true;
    session.zero_prefix_search_suggestions_shown_in_session = true;
    session.zero_prefix_url_suggestions_shown_in_session = true;
    OmniboxLog log = BuildOmniboxLog(result, /*selected_index=*/1,
                                     /*session_data=*/session);
    log.text = u"";
    log.ukm_source_id = ukm::NoURLSourceId();
    log.elapsed_time_since_user_focused_omnibox = base::Milliseconds(10);
    RecordMetrics(log);

    // Verify the UMA histograms.
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionUsed.ClientSummarizedResultType",
        ClientSummarizedResultType::kSearch,
        /*expected_count=*/1);

    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType",
        ClientSummarizedResultType::kSearch,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType.ByPageContext.NTP_"
        "REALBOX",
        ClientSummarizedResultType::kSearch,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ZeroSuggest.ClientSummarizedResultType",
        ClientSummarizedResultType::kSearch,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ZeroSuggest.ClientSummarizedResultType."
        "ByPageContext.NTP_REALBOX",
        ClientSummarizedResultType::kSearch,
        /*expected_count=*/1);

    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType",
        ClientSummarizedResultType::kUrl,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ClientSummarizedResultType.ByPageContext.NTP_"
        "REALBOX",
        ClientSummarizedResultType::kUrl,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ZeroSuggest.ClientSummarizedResultType",
        ClientSummarizedResultType::kUrl,
        /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.SuggestionShown.ZeroSuggest.ClientSummarizedResultType."
        "ByPageContext.NTP_REALBOX",
        ClientSummarizedResultType::kUrl,
        /*expected_count=*/1);

    // Verify the UKM event and the full set of metrics.
    const char* entry_name = ukm::builders::Omnibox_SuggestionUsed::kEntryName;
    EXPECT_EQ(ukm_recorder.GetEntriesByName(entry_name).size(), 1ul);
    auto* entry = ukm_recorder.GetEntriesByName(entry_name)[0].get();
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kPageClassificationName,
        static_cast<uint64_t>(
            metrics::OmniboxEventProto_PageClassification_NTP_REALBOX));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kProviderTypeName,
        static_cast<uint64_t>(metrics::OmniboxEventProto_ProviderType_SEARCH));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kResultTypeName,
        static_cast<uint64_t>(
            metrics::OmniboxEventProto_Suggestion::SEARCH_SUGGEST));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kResultTypeGroupName,
        static_cast<uint64_t>(ClientSummarizedResultType::kSearch));
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kSelectedIndexName, 1ul);
    // With exponential bucketing scheme with a standard spacing of 2.0, 10
    // falls into the 8-16 bucket as the boundaries of the buckets increase
    // exponentially, e.g., 1, 2, 4, 8, 16, etc.
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kTimeSinceLastFocusMsName,
        8ul);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kTypedLengthName, 0ul);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kTypingDurationMsName,
        0ul);
    ukm_recorder.ExpectEntryMetric(
        entry,
        ukm::builders::Omnibox_SuggestionUsed::kZeroPrefixSearchShownName,
        true);
    ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::Omnibox_SuggestionUsed::kZeroPrefixUrlShownName,
        true);
    ukm_recorder.ExpectEntryMetric(entry,
                                   ukm::builders::Omnibox_SuggestionUsed::
                                       kZeroPrefixContextualSearchShownName,
                                   false);
    ukm_recorder.ExpectEntryMetric(
        entry,
        ukm::builders::Omnibox_SuggestionUsed::kZeroPrefixLensActionShownName,
        false);
  }
}

TEST_F(OmniboxMetricsProviderTest, RecordMetrics_InvalidUkmSourceId) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  AutocompleteResult result;
  result.AppendMatches(
      {BuildMatch(AutocompleteMatch::Type::URL_WHAT_YOU_TYPED)});
  OmniboxLog log =
      BuildOmniboxLog(result, /*selected_index=*/0, /*session_data=*/{});
  RecordMetrics(log);

  // Verify the UMA histogram.
  histogram_tester.ExpectBucketCount(
      "Omnibox.SuggestionUsed.ClientSummarizedResultType",
      ClientSummarizedResultType::kUrl, 1);

  // Verify the UKM event was not logged due to invalid ukm source id.
  EXPECT_EQ(
      ukm_recorder
          .GetEntriesByName(ukm::builders::Omnibox_SuggestionUsed::kEntryName)
          .size(),
      0ul);
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
  OmniboxLog log =
      BuildOmniboxLog(result, /*selected_index=*/1, /*session_data=*/{});
  RecordLogAndVerifyScoringSignals(log, *matches[0].scoring_signals);

  // Now, "turn on" incognito mode, scoring signals should not be logged.
  log.is_incognito = true;
  RecordLogAndVerifyScoringSignals(log, *matches[0].scoring_signals);
}
#endif  // !(BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID))
