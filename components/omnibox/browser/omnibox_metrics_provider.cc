// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_metrics_provider.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/metrics/metrics_log.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"

using metrics::OmniboxEventProto;

namespace {

using ScoringSignals = ::metrics::OmniboxEventProto::Suggestion::ScoringSignals;
using OmniboxScoringSignals = ::metrics::OmniboxScoringSignals;

// Keep up to date with ClientSummarizedResultType in
// //tools/metrics/histograms/enums.xml.
enum class ClientSummarizedResultType : int {
  kUrl = 0,
  kSearch = 1,
  kApp = 2,
  kContact = 3,
  kOnDevice = 4,
  kUnknown = 5,
  kMaxValue = kUnknown
};

ClientSummarizedResultType GetClientSummarizedResultType(
    const OmniboxEventProto::Suggestion::ResultType type) {
  static const base::NoDestructor<base::flat_map<
      OmniboxEventProto::Suggestion::ResultType, ClientSummarizedResultType>>
      kResultTypesToClientSummarizedResultTypes({
          {OmniboxEventProto::Suggestion::URL_WHAT_YOU_TYPED,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::HISTORY_URL,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::HISTORY_TITLE,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::HISTORY_BODY,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::HISTORY_KEYWORD,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::NAVSUGGEST,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::BOOKMARK_TITLE,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::NAVSUGGEST_PERSONALIZED,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::CLIPBOARD_URL,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::PHYSICAL_WEB,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::PHYSICAL_WEB_OVERFLOW,
           ClientSummarizedResultType::kUrl},  // More like a URL than a search.
          {OmniboxEventProto::Suggestion::DOCUMENT,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::SEARCH_WHAT_YOU_TYPED,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::SEARCH_HISTORY,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::SEARCH_SUGGEST,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::SEARCH_OTHER_ENGINE,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::SEARCH_SUGGEST_ENTITY,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::SEARCH_SUGGEST_TAIL,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::SEARCH_SUGGEST_PERSONALIZED,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::SEARCH_SUGGEST_PROFILE,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::SEARCH_SUGGEST_ANSWER,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::CALCULATOR,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::CLIPBOARD_TEXT,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::CLIPBOARD_IMAGE,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::EXTENSION_APP,
           ClientSummarizedResultType::kApp},
          {OmniboxEventProto::Suggestion::APP,
           ClientSummarizedResultType::kApp},
          {OmniboxEventProto::Suggestion::CONTACT,
           ClientSummarizedResultType::kContact},
          {OmniboxEventProto::Suggestion::APP_RESULT,
           ClientSummarizedResultType::kOnDevice},
          {OmniboxEventProto::Suggestion::LEGACY_ON_DEVICE,
           ClientSummarizedResultType::kOnDevice},
          {OmniboxEventProto::Suggestion::TILE_SUGGESTION,
           ClientSummarizedResultType::kSearch},
          {OmniboxEventProto::Suggestion::HISTORY_CLUSTER,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::OPEN_TAB,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::STARTER_PACK,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::TAB_SWITCH,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::PEDAL,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::HISTORY_EMBEDDINGS,
           ClientSummarizedResultType::kUrl},
          {OmniboxEventProto::Suggestion::FEATURED_ENTERPRISE_SEARCH,
           ClientSummarizedResultType::kSearch},
      });

  const auto it = kResultTypesToClientSummarizedResultTypes->find(type);
  return it == kResultTypesToClientSummarizedResultTypes->cend()
             ? ClientSummarizedResultType::kUnknown
             : it->second;
}

// Extracts the subset of signals which must be logged by the client in order to
// train the Omnibox ML Scoring model using server-side training logic.
void GetScoringSignalsForLogging(const OmniboxScoringSignals& scoring_signals,
                                 ScoringSignals& scoring_signals_for_logging) {
  // Keep consistent:
  // - omnibox_event.proto `ScoringSignals`
  // - omnibox_scoring_signals.proto `OmniboxScoringSignals`
  // - autocomplete_scoring_model_handler.cc
  //   `AutocompleteScoringModelHandler::ExtractInputFromScoringSignals()`
  // - autocomplete_match.cc `AutocompleteMatch::MergeScoringSignals()`
  // - autocomplete_controller.cc `RecordScoringSignalCoverageForProvider()`
  // - omnibox_metrics_provider.cc `GetScoringSignalsForLogging()`
  // - omnibox.mojom `struct Signals`
  // - omnibox_page_handler.cc
  //   `TypeConverter<AutocompleteMatch::ScoringSignals, mojom::SignalsPtr>`
  // - omnibox_page_handler.cc `TypeConverter<mojom::SignalsPtr,
  //   AutocompleteMatch::ScoringSignals>`
  // - omnibox_util.ts `signalNames`
  // - omnibox/histograms.xml
  //   `Omnibox.URLScoringModelExecuted.ScoringSignalCoverage`

  if (scoring_signals.has_typed_count()) {
    scoring_signals_for_logging.set_typed_count(scoring_signals.typed_count());
  }
  if (scoring_signals.has_visit_count()) {
    scoring_signals_for_logging.set_visit_count(scoring_signals.visit_count());
  }
  if (scoring_signals.has_elapsed_time_last_visit_secs()) {
    scoring_signals_for_logging.set_elapsed_time_last_visit_secs(
        scoring_signals.elapsed_time_last_visit_secs());
  }
  if (scoring_signals.has_shortcut_visit_count()) {
    scoring_signals_for_logging.set_shortcut_visit_count(
        scoring_signals.shortcut_visit_count());
  }
  if (scoring_signals.has_shortest_shortcut_len()) {
    scoring_signals_for_logging.set_shortest_shortcut_len(
        scoring_signals.shortest_shortcut_len());
  }
  if (scoring_signals.has_elapsed_time_last_shortcut_visit_sec()) {
    scoring_signals_for_logging.set_elapsed_time_last_shortcut_visit_sec(
        scoring_signals.elapsed_time_last_shortcut_visit_sec());
  }
  if (scoring_signals.has_is_host_only()) {
    scoring_signals_for_logging.set_is_host_only(
        scoring_signals.is_host_only());
  }
  if (scoring_signals.has_num_bookmarks_of_url()) {
    scoring_signals_for_logging.set_num_bookmarks_of_url(
        scoring_signals.num_bookmarks_of_url());
  }
  if (scoring_signals.has_first_bookmark_title_match_position()) {
    scoring_signals_for_logging.set_first_bookmark_title_match_position(
        scoring_signals.first_bookmark_title_match_position());
  }
  if (scoring_signals.has_total_bookmark_title_match_length()) {
    scoring_signals_for_logging.set_total_bookmark_title_match_length(
        scoring_signals.total_bookmark_title_match_length());
  }
  if (scoring_signals.has_num_input_terms_matched_by_bookmark_title()) {
    scoring_signals_for_logging.set_num_input_terms_matched_by_bookmark_title(
        scoring_signals.num_input_terms_matched_by_bookmark_title());
  }
  if (scoring_signals.has_first_url_match_position()) {
    scoring_signals_for_logging.set_first_url_match_position(
        scoring_signals.first_url_match_position());
  }
  if (scoring_signals.has_total_url_match_length()) {
    scoring_signals_for_logging.set_total_url_match_length(
        scoring_signals.total_url_match_length());
  }
  if (scoring_signals.has_host_match_at_word_boundary()) {
    scoring_signals_for_logging.set_host_match_at_word_boundary(
        scoring_signals.host_match_at_word_boundary());
  }
  if (scoring_signals.has_total_host_match_length()) {
    scoring_signals_for_logging.set_total_host_match_length(
        scoring_signals.total_host_match_length());
  }
  if (scoring_signals.has_total_path_match_length()) {
    scoring_signals_for_logging.set_total_path_match_length(
        scoring_signals.total_path_match_length());
  }
  if (scoring_signals.has_total_query_or_ref_match_length()) {
    scoring_signals_for_logging.set_total_query_or_ref_match_length(
        scoring_signals.total_query_or_ref_match_length());
  }
  if (scoring_signals.has_total_title_match_length()) {
    scoring_signals_for_logging.set_total_title_match_length(
        scoring_signals.total_title_match_length());
  }
  if (scoring_signals.has_has_non_scheme_www_match()) {
    scoring_signals_for_logging.set_has_non_scheme_www_match(
        scoring_signals.has_non_scheme_www_match());
  }
  if (scoring_signals.has_num_input_terms_matched_by_title()) {
    scoring_signals_for_logging.set_num_input_terms_matched_by_title(
        scoring_signals.num_input_terms_matched_by_title());
  }
  if (scoring_signals.has_num_input_terms_matched_by_url()) {
    scoring_signals_for_logging.set_num_input_terms_matched_by_url(
        scoring_signals.num_input_terms_matched_by_url());
  }
  if (scoring_signals.has_length_of_url()) {
    scoring_signals_for_logging.set_length_of_url(
        scoring_signals.length_of_url());
  }
  if (scoring_signals.has_site_engagement()) {
    scoring_signals_for_logging.set_site_engagement(
        scoring_signals.site_engagement());
  }
  if (scoring_signals.has_allowed_to_be_default_match()) {
    scoring_signals_for_logging.set_allowed_to_be_default_match(
        scoring_signals.allowed_to_be_default_match());
  }
  if (scoring_signals.has_search_suggest_relevance()) {
    scoring_signals_for_logging.set_search_suggest_relevance(
        scoring_signals.search_suggest_relevance());
  }
}

}  // namespace

OmniboxMetricsProvider::OmniboxMetricsProvider() = default;

OmniboxMetricsProvider::~OmniboxMetricsProvider() = default;

void OmniboxMetricsProvider::OnRecordingEnabled() {
  subscription_ = OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
      base::BindRepeating(&OmniboxMetricsProvider::OnURLOpenedFromOmnibox,
                          base::Unretained(this)));
}

void OmniboxMetricsProvider::OnRecordingDisabled() {
  subscription_ = {};
}

void OmniboxMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  uma_proto->mutable_omnibox_event()->Swap(
      omnibox_events_cache.mutable_omnibox_event());
}

void OmniboxMetricsProvider::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  RecordOmniboxOpenedURL(*log);
  RecordOmniboxOpenedURLClientSummarizedResultType(*log);
}

void OmniboxMetricsProvider::RecordOmniboxOpenedURL(const OmniboxLog& log) {
  std::vector<std::u16string_view> terms =
      base::SplitStringPiece(log.text, base::kWhitespaceUTF16,
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  OmniboxEventProto* omnibox_event = omnibox_events_cache.add_omnibox_event();
  omnibox_event->set_time_sec(metrics::MetricsLog::GetCurrentTime());
  if (log.tab_id.is_valid()) {
    // If we know what tab the autocomplete URL was opened in, log it.
    omnibox_event->set_tab_id(log.tab_id.id());
  }
  omnibox_event->set_typed_length(log.text.length());
  omnibox_event->set_just_deleted_text(log.just_deleted_text);
  omnibox_event->set_num_typed_terms(static_cast<int>(terms.size()));
  omnibox_event->set_selected_index(log.selection.line);
  omnibox_event->set_selected_tab_match(log.disposition ==
                                        WindowOpenDisposition::SWITCH_TO_TAB);
  if (log.completed_length != std::u16string::npos)
    omnibox_event->set_completed_length(log.completed_length);
  const base::TimeDelta default_time_delta = base::Milliseconds(-1);
  if (log.elapsed_time_since_user_first_modified_omnibox !=
      default_time_delta) {
    // Only upload the typing duration if it is set/valid.
    omnibox_event->set_typing_duration_ms(
        log.elapsed_time_since_user_first_modified_omnibox.InMilliseconds());
  }
  if (log.elapsed_time_since_last_change_to_default_match !=
      default_time_delta) {
    omnibox_event->set_duration_since_last_default_match_update_ms(
        log.elapsed_time_since_last_change_to_default_match.InMilliseconds());
  }
  omnibox_event->set_current_page_classification(
      log.current_page_classification);
  omnibox_event->set_input_type(log.input_type);
  // We consider a paste-and-search/paste-and-go action to have a closed popup
  // (as explained in omnibox_event.proto) even if it was not, because such
  // actions ignore the contents of the popup so it doesn't matter that it was
  // open.
  omnibox_event->set_is_popup_open(log.is_popup_open && !log.is_paste_and_go);
  omnibox_event->set_is_paste_and_go(log.is_paste_and_go);

  if (log.steady_state_omnibox_position !=
      metrics::OmniboxEventProto::UNKNOWN_POSITION) {
    omnibox_event->set_steady_state_omnibox_position(
        log.steady_state_omnibox_position);
  }

  for (size_t i = 0; i < log.result->size(); i++) {
    const AutocompleteMatch& match = log.result->match_at(i);
    OmniboxEventProto::Suggestion* suggestion = omnibox_event->add_suggestion();

    const int action_index = log.selection.line == i && log.selection.IsAction()
                                 ? log.selection.action_index
                                 : -1;
    suggestion->set_provider(match.GetOmniboxEventProviderType(action_index));
    suggestion->set_result_type(match.GetOmniboxEventResultType(action_index));
    suggestion->set_relevance(match.relevance);
    if (match.typed_count != -1) {
      suggestion->set_typed_count(match.typed_count);
    }

    // TODO(crbug.com/40139076): send the entire set of subtypes.
    if (!match.subtypes.empty()) {
      suggestion->set_result_subtype_identifier(*match.subtypes.begin());
    }

    suggestion->set_has_tab_match(match.has_tab_match.value_or(false));
    suggestion->set_is_keyword_suggestion(match.from_keyword);

    // Scoring signals are not logged when the client is in incognito mode or
    // when the particular suggestion type is considered ineligible for signal
    // logging.
    if (OmniboxFieldTrial::IsReportingUrlScoringSignalsEnabled() &&
        !log.is_incognito && match.IsMlSignalLoggingEligible() &&
        match.scoring_signals) {
      ScoringSignals scoring_signals_for_logging;
      GetScoringSignalsForLogging(*match.scoring_signals,
                                  scoring_signals_for_logging);
      suggestion->mutable_scoring_signals()->CopyFrom(
          scoring_signals_for_logging);
    }
  }
  for (const auto& info : log.providers_info) {
    OmniboxEventProto::ProviderInfo* provider_info =
        omnibox_event->add_provider_info();
    provider_info->CopyFrom(info);
  }
  omnibox_event->set_in_keyword_mode(log.in_keyword_mode);
  if (log.in_keyword_mode)
    omnibox_event->set_keyword_mode_entry_method(log.keyword_mode_entry_method);
  if (log.is_query_started_from_tile)
    omnibox_event->set_is_query_started_from_tile(true);
  for (auto feature : log.features_triggered)
    omnibox_event->add_feature_triggered(feature);
  for (auto feature : log.features_triggered_in_session) {
    omnibox_event->add_feature_triggered_in_session(feature);
  }
}

void OmniboxMetricsProvider::RecordOmniboxOpenedURLClientSummarizedResultType(
    const OmniboxLog& log) {
  if (log.selection.line < 0 || log.selection.line >= log.result->size()) {
    return;
  }

  auto autocomplete_match = log.result->match_at(log.selection.line);
  auto omnibox_event_result_type = autocomplete_match.GetOmniboxEventResultType(
      log.selection.IsAction() ? log.selection.action_index : -1);
  auto client_summarized_result_type =
      GetClientSummarizedResultType(omnibox_event_result_type);
  // Log UMA histogram.
  base::UmaHistogramEnumeration(
      "Omnibox.SuggestionUsed.ClientSummarizedResultType",
      client_summarized_result_type);
  // Log UKM event.
  if (log.ukm_source_id != ukm::kInvalidSourceId) {
    ukm::builders::Omnibox_SuggestionUsed(log.ukm_source_id)
        .SetResultType(static_cast<int64_t>(client_summarized_result_type))
        .Record(ukm::UkmRecorder::Get());
  }
}
