// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/history_clusters/history_clusters_metrics_logger.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_clusters/core/query_clusters_state.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/time_format.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

// Creates a `mojom::VisitPtr` from a `history_clusters::Visit`.
mojom::URLVisitPtr VisitToMojom(Profile* profile,
                                const history::ClusterVisit& visit) {
  auto visit_mojom = mojom::URLVisit::New();
  visit_mojom->normalized_url = visit.normalized_url;
  visit_mojom->url_for_display = base::UTF16ToUTF8(visit.url_for_display);

  auto& annotated_visit = visit.annotated_visit;
  visit_mojom->raw_urls.push_back(annotated_visit.url_row.url());
  visit_mojom->last_visit_time = annotated_visit.visit_row.visit_time;
  visit_mojom->first_visit_time = annotated_visit.visit_row.visit_time;

  // Update the fields to reflect data held in the duplicate visits too.
  for (const auto& duplicate : visit.duplicate_visits) {
    visit_mojom->raw_urls.push_back(duplicate.annotated_visit.url_row.url());
    visit_mojom->last_visit_time =
        std::max(visit_mojom->last_visit_time,
                 duplicate.annotated_visit.visit_row.visit_time);
    visit_mojom->first_visit_time =
        std::min(visit_mojom->first_visit_time,
                 duplicate.annotated_visit.visit_row.visit_time);
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
  if (annotated_visit.context_annotations.is_existing_part_of_tab_group ||
      annotated_visit.context_annotations.is_placed_in_tab_group) {
    visit_mojom->annotations.push_back(mojom::Annotation::kTabGrouped);
  }
  if (annotated_visit.context_annotations.is_existing_bookmark ||
      annotated_visit.context_annotations.is_new_bookmark) {
    visit_mojom->annotations.push_back(mojom::Annotation::kBookmarked);
  }

  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  const TemplateURL* default_search_provider =
      template_url_service ? template_url_service->GetDefaultSearchProvider()
                           : nullptr;
  if (default_search_provider &&
      default_search_provider->IsSearchURL(
          visit.normalized_url, template_url_service->search_terms_data())) {
    visit_mojom->annotations.push_back(mojom::Annotation::kSearchResultsPage);
  }

  visit_mojom->hidden = visit.hidden;

  if (GetConfig().user_visible_debug) {
    visit_mojom->debug_info["visit_id"] =
        base::NumberToString(annotated_visit.visit_row.visit_id);
    visit_mojom->debug_info["score"] = base::NumberToString(visit.score);
    visit_mojom->debug_info["visit_duration"] = base::NumberToString(
        annotated_visit.visit_row.visit_duration.InSecondsF());
  }

  return visit_mojom;
}

// Creates a `mojom::SearchQueryPtr` from the given search query, if possible.
absl::optional<mojom::SearchQueryPtr> SearchQueryToMojom(
    Profile* profile,
    const std::string& search_query) {
  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  const TemplateURL* default_search_provider =
      template_url_service ? template_url_service->GetDefaultSearchProvider()
                           : nullptr;
  if (!default_search_provider) {
    return absl::nullopt;
  }

  const std::string url = default_search_provider->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(base::UTF8ToUTF16(search_query)),
      template_url_service->search_terms_data());
  if (url.empty()) {
    return absl::nullopt;
  }

  auto search_query_mojom = mojom::SearchQuery::New();
  search_query_mojom->query = search_query;
  search_query_mojom->url = GURL(url);
  return search_query_mojom;
}

}  // namespace

// Creates a `mojom::QueryResultPtr` using the original `query`, if the query
// was a continuation one, and the result of querying HistoryClustersService.
mojom::QueryResultPtr QueryClustersResultToMojom(
    Profile* profile,
    const std::string& query,
    const std::vector<history::Cluster> clusters_batch,
    bool can_load_more,
    bool is_continuation) {
  std::vector<mojom::ClusterPtr> cluster_mojoms;
  for (const auto& cluster : clusters_batch) {
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
    }

    for (const auto& visit : cluster.visits) {
      cluster_mojom->visits.push_back(VisitToMojom(profile, visit));
    }

    for (const auto& related_search : cluster.related_searches) {
      auto search_query_mojom = SearchQueryToMojom(profile, related_search);
      if (search_query_mojom) {
        cluster_mojom->related_searches.emplace_back(
            std::move(*search_query_mojom));
      }
    }

    cluster_mojoms.emplace_back(std::move(cluster_mojom));
  }

  auto result_mojom = mojom::QueryResult::New();
  result_mojom->query = query;
  result_mojom->clusters = std::move(cluster_mojoms);
  result_mojom->can_load_more = can_load_more;
  result_mojom->is_continuation = is_continuation;
  return result_mojom;
}

HistoryClustersHandler::HistoryClustersHandler(
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      page_handler_(this, std::move(pending_page_handler)) {
  DCHECK(profile_);
  DCHECK(web_contents_);

  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile_);
  DCHECK(history_clusters_service);
  service_observation_.Observe(history_clusters_service);
}

HistoryClustersHandler::~HistoryClustersHandler() = default;

void HistoryClustersHandler::SetPage(
    mojo::PendingRemote<mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void HistoryClustersHandler::ToggleVisibility(
    bool visible,
    ToggleVisibilityCallback callback) {
  profile_->GetPrefs()->SetBoolean(prefs::kVisible, visible);
  std::move(callback).Run(visible);
}

void HistoryClustersHandler::StartQueryClusters(const std::string& query) {
  if (!query.empty()) {
    // If the query string is not empty, we assume that this clusters query
    // is user generated.
    HistoryClustersMetricsLogger::GetOrCreateForPage(
        web_contents_->GetPrimaryPage())
        ->increment_query_count();
  }

  // Since the query has changed, initialize a new QueryClustersState and
  // request the first batch of clusters.
  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile_);
  query_clusters_state_ = std::make_unique<QueryClustersState>(
      history_clusters_service->GetWeakPtr(), query);
  LoadMoreClusters(query);
}

void HistoryClustersHandler::LoadMoreClusters(const std::string& query) {
  if (query_clusters_state_) {
    DCHECK_EQ(query, query_clusters_state_->query());
    query_clusters_state_->LoadNextBatchOfClusters(
        base::BindOnce(&QueryClustersResultToMojom, profile_)
            .Then(base::BindOnce(&HistoryClustersHandler::OnClustersQueryResult,
                                 weak_ptr_factory_.GetWeakPtr())));
  }
}

void HistoryClustersHandler::RemoveVisits(
    std::vector<mojom::URLVisitPtr> visits,
    RemoveVisitsCallback callback) {
  // Reject the request if a pending task exists or the set of visits is empty.
  if (remove_task_tracker_.HasTrackedTasks() || visits.empty()) {
    std::move(callback).Run(false);
    return;
  }

  std::vector<history::ExpireHistoryArgs> expire_list;
  expire_list.reserve(visits.size());
  for (const auto& visit_ptr : visits) {
    expire_list.resize(expire_list.size() + 1);
    auto& expire_args = expire_list.back();
    expire_args.urls =
        std::set<GURL>(visit_ptr->raw_urls.begin(), visit_ptr->raw_urls.end());
    // ExpireHistoryArgs::end_time is not inclusive. Make sure all visits in the
    // given timespan are removed by adding 1 second to it.
    expire_args.end_time = visit_ptr->last_visit_time + base::Seconds(1);
    expire_args.begin_time = visit_ptr->first_visit_time;
  }
  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile_);
  history_clusters_service->RemoveVisits(
      expire_list,
      base::BindOnce(&HistoryClustersHandler::OnVisitsRemoved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(visits)),
      &remove_task_tracker_);
  std::move(callback).Run(true);
}

void HistoryClustersHandler::OpenVisitUrlsInTabGroup(
    std::vector<mojom::URLVisitPtr> visits) {
  const auto* browser = chrome::FindTabbedBrowser(profile_, false);
  if (!browser) {
    return;
  }

  // Hard cap the number of opened visits in a tab group to 32. It's a
  // relatively high cap chosen fairly arbitrarily, because the user took an
  // affirmative action to open this many tabs. And hidden visits aren't opened.
  constexpr size_t kMaxVisitsToOpenInTabGroup = 32;
  if (visits.size() > kMaxVisitsToOpenInTabGroup) {
    visits.resize(kMaxVisitsToOpenInTabGroup);
  }

  auto* model = browser->tab_strip_model();
  std::vector<int> tab_indices;
  tab_indices.reserve(visits.size());
  auto* opener = web_contents_.get();
  for (const auto& visit_ptr : visits) {
    auto* opened_web_contents = opener->OpenURL(
        content::OpenURLParams(visit_ptr->normalized_url, content::Referrer(),
                               WindowOpenDisposition::NEW_BACKGROUND_TAB,
                               ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));

    // The url may have opened a new window or clobbered the current one.
    // Replace `opener` to be sure. `opened_web_contents` may be null in tests.
    if (opened_web_contents) {
      opener = opened_web_contents;
    }

    // Only add those tabs to a new group that actually opened in this browser.
    const int tab_index = model->GetIndexOfWebContents(opened_web_contents);
    if (tab_index != TabStripModel::kNoTab) {
      tab_indices.push_back(tab_index);
    }
  }
  model->AddToNewGroup(tab_indices);
}

void HistoryClustersHandler::OnDebugMessage(const std::string& message) {
  content::RenderFrameHost* rfh = web_contents_->GetMainFrame();
  if (rfh && GetConfig().non_user_visible_debug) {
    rfh->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kInfo, message);
  }
}

void HistoryClustersHandler::OnClustersQueryResult(
    mojom::QueryResultPtr query_result) {
  page_->OnClustersQueryResult(std::move(query_result));
}

void HistoryClustersHandler::OnVisitsRemoved(
    std::vector<mojom::URLVisitPtr> visits) {
  page_->OnVisitsRemoved(std::move(visits));
}

}  // namespace history_clusters
