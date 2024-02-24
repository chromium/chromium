// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "history_clusters_debug_jsons.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_util.h"

namespace history_clusters {

namespace {

base::Value::Dict GetDebugJSONDictForAnnotatedVisit(
    const history::AnnotatedVisit& visit) {
  base::Value::Dict debug_visit;
  debug_visit.Set("visitId", base::NumberToString(visit.visit_row.visit_id));
  debug_visit.Set("url",
                  visit.content_annotations.search_normalized_url.is_empty()
                      ? visit.url_row.url().spec()
                      : visit.content_annotations.search_normalized_url.spec());
  debug_visit.Set("title", visit.url_row.title());
  debug_visit.Set(
      "foregroundTimeSecs",
      base::NumberToString(
          visit.context_annotations.total_foreground_duration.InSeconds()));
  debug_visit.Set(
      "visitDurationSecs",
      base::NumberToString(visit.visit_row.visit_duration.InSeconds()));
  debug_visit.Set(
      "navigationTimeMs",
      base::NumberToString(visit.visit_row.visit_time.ToDeltaSinceWindowsEpoch()
                               .InMilliseconds()));
  debug_visit.Set("pageEndReason", visit.context_annotations.page_end_reason);
  debug_visit.Set("pageTransition",
                  base::NumberToString(visit.visit_row.transition));
  debug_visit.Set(
      "referringVisitId",
      base::NumberToString(visit.referring_visit_of_redirect_chain_start));
  debug_visit.Set(
      "openerVisitId",
      base::NumberToString(visit.opener_visit_of_redirect_chain_start));
  debug_visit.Set("originatorCacheGuid", visit.visit_row.originator_cache_guid);
  debug_visit.Set(
      "originatorReferringVisitId",
      base::NumberToString(visit.visit_row.originator_referring_visit));
  debug_visit.Set(
      "originatorOpenerVisitId",
      base::NumberToString(visit.visit_row.originator_opener_visit));
  debug_visit.Set("urlForDeduping",
                  visit.content_annotations.search_normalized_url.is_empty()
                      ? ComputeURLForDeduping(visit.url_row.url()).spec()
                      : visit.content_annotations.search_normalized_url.spec());
  debug_visit.Set("visitSource", base::NumberToString(visit.source));
  debug_visit.Set("isKnownToSync", visit.visit_row.is_known_to_sync);
  debug_visit.Set("normalized_url",
                  visit.content_annotations.search_normalized_url.is_empty()
                      ? visit.url_row.url().spec()
                      : visit.content_annotations.search_normalized_url.spec());

  // Content annotations.
  base::Value::List debug_categories;
  for (const auto& category :
       visit.content_annotations.model_annotations.categories) {
    base::Value::Dict debug_category;
    debug_category.Set("name", category.id);
    debug_category.Set("value", category.weight);
    debug_categories.Append(std::move(debug_category));
  }
  debug_visit.Set("categories", std::move(debug_categories));
  base::Value::List debug_entities;
  for (const auto& entity :
       visit.content_annotations.model_annotations.entities) {
    base::Value::Dict debug_entity;
    debug_entity.Set("name", entity.id);
    debug_entity.Set("value", entity.weight);
    debug_entities.Append(std::move(debug_entity));
  }
  debug_visit.Set("entities", std::move(debug_entities));
  debug_visit.Set("visibility",
                  visit.content_annotations.model_annotations.visibility_score);
  debug_visit.Set("searchTerms", visit.content_annotations.search_terms);
  if (!visit.content_annotations.search_terms.empty()) {
    debug_visit.Set("hasRelatedSearches",
                    !visit.content_annotations.related_searches.empty());
  }
  debug_visit.Set("hasUrlKeyedImage",
                  visit.content_annotations.has_url_keyed_image);
  return debug_visit;
}

}  // namespace

std::string GetDebugTime(const base::Time time) {
  return time.is_null() ? "null" : base::TimeFormatAsIso8601(time);
}

// Gets a loggable JSON representation of `visits`.
std::string GetDebugJSONForVisits(
    const std::vector<history::AnnotatedVisit>& visits) {
  base::Value::List debug_visits_list;
  for (auto& visit : visits) {
    debug_visits_list.Append(GetDebugJSONDictForAnnotatedVisit(visit));
  }

  base::Value::Dict debug_value;
  debug_value.Set("visits", std::move(debug_visits_list));
  std::string debug_string;
  if (!base::JSONWriter::WriteWithOptions(
          debug_value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &debug_string)) {
    debug_string = "Error: Could not write visits to JSON.";
  }
  return debug_string;
}

// Gets a loggable JSON representation of `clusters`.
std::string GetDebugJSONForClusters(
    const std::vector<history::Cluster>& clusters) {
  base::Value::List debug_clusters_list;
  for (const auto& cluster : clusters) {
    base::Value::Dict debug_cluster;
    debug_cluster.Set("id", static_cast<int>(cluster.cluster_id));
    debug_cluster.Set("label", cluster.label.value_or(u""));
    base::Value::List debug_keywords;
    for (const auto& keyword_data_p : cluster.keyword_to_data_map) {
      debug_keywords.Append(base::UTF16ToUTF8(keyword_data_p.first));
    }
    debug_cluster.Set("keywords", std::move(debug_keywords));
    debug_cluster.Set("should_show_on_prominent_ui_surfaces",
                      cluster.should_show_on_prominent_ui_surfaces);
    debug_cluster.Set("triggerability_calculated",
                      cluster.triggerability_calculated);

    base::Value::List debug_visits;
    for (const auto& visit : cluster.visits) {
      base::Value::Dict debug_visit =
          GetDebugJSONDictForAnnotatedVisit(visit.annotated_visit);
      debug_visit.Set("score", visit.score);
      debug_visit.Set("interaction_state",
                      history::ClusterVisit::InteractionStateToInt(
                          visit.interaction_state));
      debug_visit.Set("site_engagement_score", visit.engagement_score);

      base::Value::List debug_duplicate_visits;
      for (const auto& duplicate_visit : visit.duplicate_visits)
        debug_duplicate_visits.Append(duplicate_visit.url.spec());
      debug_visit.Set("duplicate_visits", std::move(debug_duplicate_visits));

      debug_visits.Append(std::move(debug_visit));
    }
    debug_cluster.Set("visits", std::move(debug_visits));

    debug_clusters_list.Append(std::move(debug_cluster));
  }

  base::Value::Dict debug_value;
  debug_value.Set("clusters", std::move(debug_clusters_list));
  std::string debug_string;
  if (!base::JSONWriter::WriteWithOptions(
          debug_value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &debug_string)) {
    debug_string = "Error: Could not write clusters to JSON.";
  }
  return debug_string;
}

template <typename T>
std::string GetDebugJSONForUrlKeywordSet(
    const std::unordered_set<T>& keyword_set) {
  base::Value::List keyword_list;
  for (const auto& keyword : keyword_set) {
    keyword_list.Append(keyword);
  }

  std::string debug_string;
  if (!base::JSONWriter::WriteWithOptions(
          keyword_list, base::JSONWriter::OPTIONS_PRETTY_PRINT,
          &debug_string)) {
    debug_string = "Error: Could not write keywords list to JSON.";
  }
  return debug_string;
}

template std::string GetDebugJSONForUrlKeywordSet<std::u16string>(
    const std::unordered_set<std::u16string>&);
template std::string GetDebugJSONForUrlKeywordSet<std::string>(
    const std::unordered_set<std::string>&);

std::string GetDebugJSONForKeywordMap(
    const std::unordered_map<std::u16string, history::ClusterKeywordData>&
        keyword_to_data_map) {
  base::Value::List debug_keywords;
  for (const auto& keyword_data_p : keyword_to_data_map) {
    debug_keywords.Append(base::UTF16ToUTF8(keyword_data_p.first));
  }
  std::string debug_string;
  if (!base::JSONWriter::WriteWithOptions(
          debug_keywords, base::JSONWriter::OPTIONS_PRETTY_PRINT,
          &debug_string)) {
    debug_string = "Error: Could not write keywords list to JSON.";
  }
  return debug_string;
}

}  // namespace history_clusters
