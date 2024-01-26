// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_cluster_type_utils.h"

#include <optional>

#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "ui/base/l10n/time_format.h"

namespace history_clusters {

// Creates a `mojom::VisitPtr` from a `history_clusters::Visit`.
mojom::URLVisitPtr VisitToMojom(const TemplateURLService* template_url_service,
                                const history::ClusterVisit& visit) {
  auto visit_mojom = mojom::URLVisit::New();
  visit_mojom->visit_id = visit.annotated_visit.visit_row.visit_id;
  visit_mojom->normalized_url = visit.normalized_url;
  visit_mojom->url_for_display = base::UTF16ToUTF8(visit.url_for_display);
  visit_mojom->is_known_to_sync =
      visit.annotated_visit.visit_row.is_known_to_sync;

  // Add the raw URLs and visit times so the UI can perform deletion.
  auto& annotated_visit = visit.annotated_visit;
  visit_mojom->has_url_keyed_image =
      annotated_visit.content_annotations.has_url_keyed_image;
  visit_mojom->raw_visit_data = mojom::RawVisitData::New(
      annotated_visit.url_row.url(), annotated_visit.visit_row.visit_time);
  for (const auto& duplicate : visit.duplicate_visits) {
    visit_mojom->duplicates.push_back(
        mojom::RawVisitData::New(duplicate.url, duplicate.visit_time));
  }

  visit_mojom->page_title = base::UTF16ToUTF8(annotated_visit.url_row.title());

  for (const auto& match : visit.title_match_positions) {
    auto match_mojom = mojom::MatchPosition::New();
    match_mojom->begin = match.first;
    match_mojom->end = match.second;
    visit_mojom->title_match_positions.push_back(std::move(match_mojom));
  }
  for (const auto& match : visit.url_for_display_match_positions) {
    auto match_mojom = mojom::MatchPosition::New();
    match_mojom->begin = match.first;
    match_mojom->end = match.second;
    visit_mojom->url_for_display_match_positions.push_back(
        std::move(match_mojom));
  }

  visit_mojom->relative_date = base::UTF16ToUTF8(ui::TimeFormat::Simple(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
      base::Time::Now() - annotated_visit.visit_row.visit_time));
  if (annotated_visit.context_annotations.is_existing_bookmark ||
      annotated_visit.context_annotations.is_new_bookmark) {
    visit_mojom->annotations.push_back(mojom::Annotation::kBookmarked);
  }

  const TemplateURL* default_search_provider =
      template_url_service ? template_url_service->GetDefaultSearchProvider()
                           : nullptr;
  if (default_search_provider &&
      default_search_provider->IsSearchURL(
          visit.normalized_url, template_url_service->search_terms_data())) {
    visit_mojom->annotations.push_back(mojom::Annotation::kSearchResultsPage);
  }

  if (GetConfig().user_visible_debug) {
    visit_mojom->debug_info["visit_id"] =
        base::NumberToString(annotated_visit.visit_row.visit_id);
    visit_mojom->debug_info["score"] = base::NumberToString(visit.score);
    visit_mojom->debug_info["interaction_state"] = base::NumberToString(
        history::ClusterVisit::InteractionStateToInt(visit.interaction_state));
    visit_mojom->debug_info["visit_time"] =
        base::TimeFormatAsIso8601(visit.annotated_visit.visit_row.visit_time);
    visit_mojom->debug_info["foreground_duration"] =
        base::NumberToString(annotated_visit.context_annotations
                                 .total_foreground_duration.InSecondsF());
    visit_mojom->debug_info["visit_source"] =
        base::NumberToString(annotated_visit.source);
  }

  return visit_mojom;
}

// Creates a `mojom::SearchQueryPtr` from the given search query, if possible.
std::optional<mojom::SearchQueryPtr> SearchQueryToMojom(
    const TemplateURLService* template_url_service,
    const std::string& search_query) {
  const TemplateURL* default_search_provider =
      template_url_service ? template_url_service->GetDefaultSearchProvider()
                           : nullptr;
  if (!default_search_provider) {
    return std::nullopt;
  }

  const std::string url = default_search_provider->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(base::UTF8ToUTF16(search_query)),
      template_url_service->search_terms_data());
  if (url.empty()) {
    return std::nullopt;
  }

  auto search_query_mojom = mojom::SearchQuery::New();
  search_query_mojom->query = search_query;
  search_query_mojom->url = GURL(url);
  return search_query_mojom;
}

mojom::ClusterPtr ClusterToMojom(const TemplateURLService* template_url_service,
                                 const history::Cluster cluster) {
  auto cluster_mojom = mojom::Cluster::New();
  cluster_mojom->id = cluster.cluster_id;
  if (cluster.label) {
    cluster_mojom->label = base::UTF16ToUTF8(*cluster.label);
    for (const auto& match : cluster.label_match_positions) {
      auto match_mojom = mojom::MatchPosition::New();
      match_mojom->begin = match.first;
      match_mojom->end = match.second;
      cluster_mojom->label_match_positions.push_back(std::move(match_mojom));
    }

    if (GetConfig().named_new_tab_groups && cluster.raw_label &&
        (cluster.label_source == history::Cluster::LabelSource::kSearch ||
         cluster.label_source ==
             history::Cluster::LabelSource::kContentDerivedEntity)) {
      cluster_mojom->tab_group_name = base::UTF16ToUTF8(*cluster.raw_label);
    }
  }

  cluster_mojom->from_persistence = cluster.from_persistence;

  if (GetConfig().user_visible_debug && cluster.from_persistence) {
    cluster_mojom->debug_info =
        "persisted, id = " + base::NumberToString(cluster.cluster_id);
  }

  for (const auto& visit : cluster.visits) {
    cluster_mojom->visits.push_back(VisitToMojom(template_url_service, visit));
  }

  for (const auto& related_search : cluster.related_searches) {
    auto search_query_mojom =
        SearchQueryToMojom(template_url_service, related_search);
    if (search_query_mojom) {
      cluster_mojom->related_searches.emplace_back(
          std::move(*search_query_mojom));
    }
  }

  return cluster_mojom;
}

}  // namespace history_clusters
