// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/eoc_internals/eoc_internals_page_handler.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_cache.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_features.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_fetch.h"
#include "components/ntp_snippets/contextual/contextual_suggestions_result.h"

using contextual_suggestions::ContextualContentSuggestionsService;
using contextual_suggestions::ContextualSuggestionsCache;
using contextual_suggestions::ContextualSuggestionsDebuggingReporter;
using contextual_suggestions::ContextualSuggestionsFetch;
using contextual_suggestions::ContextualSuggestionsResult;

namespace {
bool AreChromeFlagsSetup() {
  return base::FeatureList::IsEnabled(
             contextual_suggestions::kContextualSuggestionsButton);
}

std::string GetAreChromeFlagsSetupString() {
  return AreChromeFlagsSetup() ? "true" : "false";
}
}  // namespace

EocInternalsPageHandler::EocInternalsPageHandler(
    eoc_internals::mojom::PageHandlerRequest request,
    ContextualContentSuggestionsService* contextual_content_suggestions_service)
    : binding_(this, std::move(request)),
      contextual_content_suggestions_service_(
          contextual_content_suggestions_service) {}

EocInternalsPageHandler::~EocInternalsPageHandler() {}

void EocInternalsPageHandler::GetProperties(GetPropertiesCallback callback) {
  base::flat_map<std::string, std::string> properties;
  // TODO(wylieb): Find the actual time when it's moved to c++ and configurable,
  // see b/838748 for more details.
  // TODO(wylieb): Instead of having a map<string, string>, move this to a mojo
  // struct and populate the fields to html in javascript.
  properties["time-to-trigger"] = "2";
  properties["chrome-flags-setup"] = GetAreChromeFlagsSetupString();
  properties["fetch-endpoint-url"] =
      ContextualSuggestionsFetch::GetFetchEndpoint();
  std::move(callback).Run(properties);
}

void EocInternalsPageHandler::SetTriggerTime(int64_t seconds) {
  // TODO(wylieb): Implement this when updating triggering time manually is
  // supported.
}

void EocInternalsPageHandler::GetCachedMetricEvents(
    GetCachedMetricEventsCallback callback) {
  std::vector<eoc_internals::mojom::MetricEventPtr> metric_events;
  if (contextual_content_suggestions_service_ == nullptr) {
    std::move(callback).Run(std::move(metric_events));
    return;
  }

  ContextualSuggestionsDebuggingReporter* debugging_reporter =
      contextual_content_suggestions_service_->GetDebuggingReporter();
  // Events will be ordered from oldest -> newest.
  // TODO(wylieb): Consider storing a timestamp along with metric events so that
  // clear ordering and sorting can be used.
  for (auto debug_event : debugging_reporter->GetEvents()) {
    auto metric_event = eoc_internals::mojom::MetricEvent::New();
    metric_event->url = debug_event.url;
    metric_event->sheet_peeked = debug_event.sheet_peeked;
    metric_event->button_shown = debug_event.button_shown;
    metric_event->sheet_opened = debug_event.sheet_opened;
    metric_event->sheet_closed = debug_event.sheet_closed;
    metric_event->any_suggestion_taken = debug_event.any_suggestion_taken;
    metric_event->any_suggestion_downloaded =
        debug_event.any_suggestion_downloaded;
    metric_events.push_back(std::move(metric_event));
  }
  std::move(callback).Run(std::move(metric_events));
}

void EocInternalsPageHandler::ClearCachedMetricEvents(
    ClearCachedMetricEventsCallback callback) {
  if (!AreChromeFlagsSetup()) {
    std::move(callback).Run();
    return;
  }

  contextual_content_suggestions_service_->GetDebuggingReporter()
      ->ClearEvents();
  std::move(callback).Run();
}

void EocInternalsPageHandler::GetCachedSuggestionResults(
    GetCachedSuggestionResultsCallback callback) {
  std::vector<eoc_internals::mojom::SuggestionResultPtr> suggestion_results;
  if (contextual_content_suggestions_service_ == nullptr) {
    std::move(callback).Run(std::move(suggestion_results));
    return;
  }

  base::flat_map<GURL, ContextualSuggestionsResult> result_map =
      contextual_content_suggestions_service_
          ->GetAllCachedResultsForDebugging();

  for (auto iter = result_map.begin(); iter != result_map.end(); iter++) {
    auto suggestion_result = eoc_internals::mojom::SuggestionResult::New();
    suggestion_result->url = iter->first.spec();

    auto peek_conditions = eoc_internals::mojom::PeekConditions::New();
    peek_conditions->confidence = iter->second.peek_conditions.confidence;
    peek_conditions->page_scroll_percentage =
        iter->second.peek_conditions.page_scroll_percentage;
    peek_conditions->minimum_seconds_on_page =
        iter->second.peek_conditions.minimum_seconds_on_page;
    peek_conditions->maximum_number_of_peeks =
        iter->second.peek_conditions.maximum_number_of_peeks;
    suggestion_result->peek_conditions = std::move(peek_conditions);

    for (const auto& cluster : iter->second.clusters) {
      for (const auto& contextual_suggestion : cluster.suggestions) {
        auto suggestion = eoc_internals::mojom::Suggestion::New();
        suggestion->url = contextual_suggestion.url.spec();
        suggestion->title = contextual_suggestion.title;
        suggestion->publisher_name = contextual_suggestion.publisher_name;
        suggestion->snippet = contextual_suggestion.snippet;
        suggestion->image_id = contextual_suggestion.image_id;
        suggestion->favicon_image_id = contextual_suggestion.favicon_image_id;
        suggestion_result->suggestions.push_back(std::move(suggestion));
      }
    }

    suggestion_results.push_back(std::move(suggestion_result));
  }

  std::move(callback).Run(std::move(suggestion_results));
}

void EocInternalsPageHandler::ClearCachedSuggestionResults(
    ClearCachedSuggestionResultsCallback callback) {
  if (!AreChromeFlagsSetup()) {
    std::move(callback).Run();
    return;
  }

  contextual_content_suggestions_service_->ClearCachedResultsForDebugging();
  std::move(callback).Run();
}
