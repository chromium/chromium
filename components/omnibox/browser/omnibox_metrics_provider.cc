// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_metrics_provider.h"

#include <string>
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
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"

using metrics::OmniboxEventProto;

namespace {

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
      });

  const auto it = kResultTypesToClientSummarizedResultTypes->find(type);
  return it == kResultTypesToClientSummarizedResultTypes->cend()
             ? ClientSummarizedResultType::kUnknown
             : it->second;
}

}  // namespace

OmniboxMetricsProvider::OmniboxMetricsProvider() {}

OmniboxMetricsProvider::~OmniboxMetricsProvider() {}

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
  std::vector<base::StringPiece16> terms =
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

    // TODO(https://crbug.com/1103056): send the entire set of subtypes.
    if (!match.subtypes.empty()) {
      suggestion->set_result_subtype_identifier(*match.subtypes.begin());
    }

    suggestion->set_has_tab_match(match.has_tab_match.value_or(false));
    suggestion->set_is_keyword_suggestion(match.from_keyword);

    // Scoring signals are not logged for search suggestions or in incognito
    // mode.
    if (OmniboxFieldTrial::IsReportingUrlScoringSignalsEnabled() &&
        !AutocompleteMatch::IsSearchType(match.type) && !log.is_incognito &&
        match.scoring_signals) {
      suggestion->mutable_scoring_signals()->CopyFrom(*match.scoring_signals);
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
  base::UmaHistogramEnumeration(
      "Omnibox.SuggestionUsed.ClientSummarizedResultType",
      client_summarized_result_type);
}
