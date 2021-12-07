// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
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
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

namespace history_clusters {

namespace {

// Creates a `mojom::VisitPtr` from a `history_clusters::Visit`.
mojom::URLVisitPtr VisitToMojom(Profile* profile, const Visit& visit) {
  auto visit_mojom = mojom::URLVisit::New();
  visit_mojom->normalized_url = visit.normalized_url;

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

  if (base::FeatureList::IsEnabled(kUserVisibleDebug)) {
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

// Chosen fairly arbitrarily. In practice this fills many vertical viewports
// adequately. The WebUI automatically queries for more for tall monitor cases.
constexpr size_t kMaxClustersCount = 10;

}  // namespace

// Creates a `mojom::QueryResultPtr` using the original `query`, if the query
// was a continuation one, and the result of querying HistoryClustersService.
mojom::QueryResultPtr QueryClustersResultToMojom(Profile* profile,
                                                 const std::string& query,
                                                 bool is_continuation,
                                                 QueryClustersResult result) {
  std::vector<mojom::ClusterPtr> cluster_mojoms;
  for (const auto& cluster : result.clusters) {
    auto cluster_mojom = mojom::Cluster::New();
    cluster_mojom->id = cluster.cluster_id;
    std::set<std::string> related_searches;  // Keeps track of unique searches.
    for (const auto& visit : cluster.visits) {
      mojom::URLVisitPtr visit_mojom = VisitToMojom(profile, visit);
      if (!cluster_mojom->visit) {
        // The first visit is always the top visit.
        cluster_mojom->visit = std::move(visit_mojom);
      } else {
        const auto& top_visit = cluster.visits.front();
        DCHECK(visit.score <= top_visit.score);
        // After the experiment-controlled max related visits are attached to
        // the top visit, any subsequent visits scored below the
        // experiment-controlled threshold are considered below the fold.
        // 0-scored (duplicate) visits are always considered below the fold.
        const auto& top_visit_mojom = cluster_mojom->visit;
        visit_mojom->below_the_fold =
            (top_visit_mojom->related_visits.size() >=
                 static_cast<size_t>(
                     kNumVisitsToAlwaysShowAboveTheFold.Get()) &&
             visit.score < kMinScoreToAlwaysShowAboveTheFold.Get()) ||
            visit.score == 0.0;
        top_visit_mojom->related_visits.push_back(std::move(visit_mojom));
      }

      // Coalesce the related searches of this visit into the top visit, but
      // only if the top visit's related searches count is still under the cap.
      // Note we coalesce a whole visit's worth of searches at a time, so we
      // can exceed the cap, but we ignore further visits' searches after that.
      constexpr size_t kMaxRelatedSearches = 8;
      const auto& top_visit_mojom = cluster_mojom->visit;
      if (top_visit_mojom->related_searches.size() < kMaxRelatedSearches) {
        for (const auto& search_query :
             visit.annotated_visit.content_annotations.related_searches) {
          if (!related_searches.insert(search_query).second) {
            continue;
          }

          auto search_query_mojom = SearchQueryToMojom(profile, search_query);
          if (search_query_mojom) {
            top_visit_mojom->related_searches.emplace_back(
                std::move(*search_query_mojom));
          }
        }
      }
    }

    cluster_mojoms.emplace_back(std::move(cluster_mojom));
  }

  auto result_mojom = mojom::QueryResult::New();
  result_mojom->query = query;
  result_mojom->is_continuation = is_continuation;
  result_mojom->continuation_end_time = result.continuation_end_time;
  result_mojom->clusters = std::move(cluster_mojoms);
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

void HistoryClustersHandler::QueryClusters(mojom::QueryParamsPtr query_params) {
  base::TimeTicks query_start_time = base::TimeTicks::Now();

  const std::string& query = query_params->query;
  base::Time end_time;
  if (query_params->end_time.has_value()) {
    // Continuation queries have a non-null value for `end_time`.
    DCHECK(!query_params->end_time->is_null())
        << "Queried clusters with a null value for end_time.";
    end_time = *(query_params->end_time);
  }

  if (!query.empty()) {
    // If the query string is not empty, we assume that this clusters query
    // is user generated.
    HistoryClustersMetricsLogger::GetOrCreateForPage(
        web_contents_->GetPrimaryPage())
        ->increment_query_count();
  }

  // Cancel pending tasks, if any.
  query_task_tracker_.TryCancelAll();
  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile_);
  history_clusters_service->QueryClusters(
      query, /*begin_time=*/base::Time(), end_time, kMaxClustersCount,
      base::BindOnce(&QueryClustersResultToMojom, profile_, query,
                     query_params->end_time.has_value())
          .Then(base::BindOnce(&HistoryClustersHandler::OnClustersQueryResult,
                               weak_ptr_factory_.GetWeakPtr(),
                               query_start_time)),
      &query_task_tracker_);
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
  if (rfh && base::FeatureList::IsEnabled(kNonUserVisibleDebug)) {
    rfh->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kInfo, message);
  }
}

void HistoryClustersHandler::OnClustersQueryResult(
    base::TimeTicks query_start_time,
    mojom::QueryResultPtr query_result) {
  // In case no clusters came back, recursively ask for more here. We do this
  // to fulfill the mojom contract where we always return at least one cluster,
  // or we exhaust History. We don't do this in the service because of task
  // tracker lifetime difficulty. In practice, this only happens when the user
  // has a search query that doesn't match any of the clusters in the "page".
  // https://crbug.com/1263728
  if (query_result->clusters.empty() &&
      query_result->continuation_end_time.has_value()) {
    base::Time continuation_end_time = *query_result->continuation_end_time;
    DCHECK(!continuation_end_time.is_null());

    auto* history_clusters_service =
        HistoryClustersServiceFactory::GetForBrowserContext(profile_);
    history_clusters_service->QueryClusters(
        query_result->query, /*begin_time=*/base::Time(), continuation_end_time,
        kMaxClustersCount,
        base::BindOnce(&QueryClustersResultToMojom, profile_,
                       query_result->query, query_result->is_continuation)
            .Then(base::BindOnce(&HistoryClustersHandler::OnClustersQueryResult,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 query_start_time)),
        &query_task_tracker_);

    return;
  }

  page_->OnClustersQueryResult(std::move(query_result));

  // Log metrics after delivering the results to the page.
  base::TimeDelta service_latency = base::TimeTicks::Now() - query_start_time;
  base::UmaHistogramTimes("History.Clusters.ServiceLatency", service_latency);
}

void HistoryClustersHandler::OnVisitsRemoved(
    std::vector<mojom::URLVisitPtr> visits) {
  page_->OnVisitsRemoved(std::move(visits));
}

}  // namespace history_clusters
