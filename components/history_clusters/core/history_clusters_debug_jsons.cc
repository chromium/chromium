// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "history_clusters_debug_jsons.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace history_clusters {

// Gets a loggable JSON representation of `visits`.
std::string GetDebugJSONForVisits(
    const std::vector<history::AnnotatedVisit>& visits) {
  base::ListValue debug_visits_list;
  for (auto& visit : visits) {
    base::DictionaryValue debug_visit;
    debug_visit.SetIntKey("visitId", visit.visit_row.visit_id);
    debug_visit.SetStringKey("url", visit.url_row.url().spec());
    debug_visit.SetStringKey("title", visit.url_row.title());
    debug_visit.SetIntKey("foreground_time_secs",
                          visit.visit_row.visit_duration.InSeconds());
    debug_visit.SetIntKey(
        "navigationTimeMs",
        visit.visit_row.visit_time.ToDeltaSinceWindowsEpoch().InMilliseconds());
    debug_visit.SetIntKey("pageEndReason",
                          visit.context_annotations.page_end_reason);
    debug_visit.SetIntKey("pageTransition",
                          static_cast<int>(visit.visit_row.transition));
    debug_visit.SetIntKey("referringVisitId",
                          visit.referring_visit_of_redirect_chain_start);
    debug_visit.SetIntKey("openerVisitId",
                          visit.opener_visit_of_redirect_chain_start);
    debug_visits_list.Append(std::move(debug_visit));
  }

  base::DictionaryValue debug_value;
  debug_value.SetKey("visits", std::move(debug_visits_list));
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
  // TODO(manukh): `ListValue` is deprecated; replace with `std::vector`.
  base::ListValue debug_clusters_list;
  for (const auto& cluster : clusters) {
    base::DictionaryValue debug_cluster;

    debug_cluster.SetStringKey("label", cluster.label.value_or(u""));
    base::DictionaryValue debug_keyword_to_data_map;
    for (const auto& keyword_data_p : cluster.keyword_to_data_map) {
      base::ListValue debug_collection;
      for (const auto& collection : keyword_data_p.second.entity_collections) {
        debug_collection.Append(collection);
      }
      base::DictionaryValue debug_keyword_data;
      debug_keyword_data.SetKey("collections", std::move(debug_collection));
      debug_keyword_to_data_map.SetKey(base::UTF16ToUTF8(keyword_data_p.first),
                                       std::move(debug_keyword_data));
    }
    debug_cluster.SetKey("keyword_to_data_map",
                         std::move(debug_keyword_to_data_map));
    debug_cluster.SetBoolKey("should_show_on_prominent_ui_surfaces",
                             cluster.should_show_on_prominent_ui_surfaces);

    base::ListValue debug_visits;
    for (const auto& visit : cluster.visits) {
      base::DictionaryValue debug_visit;
      debug_visit.SetIntKey("visit_id",
                            visit.annotated_visit.visit_row.visit_id);
      debug_visit.SetDoubleKey("score", visit.score);
      base::ListValue debug_categories;
      for (const auto& category : visit.annotated_visit.content_annotations
                                      .model_annotations.categories) {
        base::DictionaryValue debug_category;
        debug_category.SetStringKey("name", category.id);
        debug_category.SetIntKey("value", category.weight);
        debug_categories.Append(std::move(debug_category));
      }
      debug_visit.SetKey("categories", std::move(debug_categories));
      base::ListValue debug_entities;
      for (const auto& entity : visit.annotated_visit.content_annotations
                                    .model_annotations.entities) {
        base::DictionaryValue debug_entity;
        debug_entity.SetStringKey("name", entity.id);
        debug_entity.SetIntKey("value", entity.weight);
        debug_entities.Append(std::move(debug_entity));
      }
      debug_visit.SetKey("entities", std::move(debug_entities));
      if (!visit.annotated_visit.content_annotations.search_terms.empty()) {
        debug_visit.SetStringKey(
            "search_terms",
            visit.annotated_visit.content_annotations.search_terms);
      }
      debug_visit.SetDoubleKey("site_engagement_score", visit.engagement_score);

      base::ListValue debug_duplicate_visits;
      for (const auto& duplicate_visit : visit.duplicate_visits) {
        debug_duplicate_visits.Append(static_cast<int>(
            duplicate_visit.annotated_visit.visit_row.visit_id));
      }
      debug_visit.SetKey("duplicate_visits", std::move(debug_duplicate_visits));

      debug_visits.Append(std::move(debug_visit));
    }
    debug_cluster.SetKey("visits", std::move(debug_visits));

    debug_clusters_list.Append(std::move(debug_cluster));
  }

  std::string debug_string;
  if (!base::JSONWriter::WriteWithOptions(
          debug_clusters_list, base::JSONWriter::OPTIONS_PRETTY_PRINT,
          &debug_string)) {
    debug_string = "Error: Could not write clusters to JSON.";
  }
  return debug_string;
}

template <typename T>
std::string GetDebugJSONForUrlKeywordSet(
    const std::unordered_set<T>& keyword_set) {
  std::vector<base::Value> keyword_list;
  for (const auto& keyword : keyword_set) {
    keyword_list.emplace_back(keyword);
  }

  std::string debug_string;
  if (!base::JSONWriter::WriteWithOptions(
          base::Value(keyword_list), base::JSONWriter::OPTIONS_PRETTY_PRINT,
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
  base::DictionaryValue debug_keyword_to_data_map;
  for (const auto& keyword_data_p : keyword_to_data_map) {
    base::ListValue debug_collection;
    for (const auto& collection : keyword_data_p.second.entity_collections) {
      debug_collection.Append(collection);
    }
    base::DictionaryValue debug_keyword_data;
    debug_keyword_data.SetKey("collections", std::move(debug_collection));
    debug_keyword_to_data_map.SetKey(base::UTF16ToUTF8(keyword_data_p.first),
                                     std::move(debug_keyword_data));
  }
  std::string debug_string;
  if (!base::JSONWriter::WriteWithOptions(
          debug_keyword_to_data_map, base::JSONWriter::OPTIONS_PRETTY_PRINT,
          &debug_string)) {
    debug_string = "Error: Could not write keywords list to JSON.";
  }
  return debug_string;
}

}  // namespace history_clusters
