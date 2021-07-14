// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if !defined(CHROME_BRANDED)
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/search_engines/template_url_service.h"
#include "ui/base/l10n/time_format.h"
#endif

namespace {

// Exists temporarily only for developer usage. Never enabled via variations.
// TODO(mahmadi): Remove once on-device clustering backend is more mature.
const base::Feature kUIDevelopmentMakeFakeHistoryClusters{
    "UIDevelopmentMakeFakeHistoryClusters", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

HistoryClustersHandler::HistoryClustersHandler(
    mojo::PendingReceiver<history_clusters::mojom::PageHandler>
        pending_page_handler,
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
    mojo::PendingRemote<history_clusters::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void HistoryClustersHandler::QueryClusters(
    history_clusters::mojom::QueryParamsPtr query_params) {
  const std::string& query = query_params->query;
  const size_t max_count = query_params->max_count;
  base::Time end_time = query_params->max_time.value_or(base::Time());
  auto result_callback =
      base::BindOnce(&HistoryClustersHandler::OnClustersQueryResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(query_params));

  if (!base::FeatureList::IsEnabled(kUIDevelopmentMakeFakeHistoryClusters)) {
    // Cancel pending tasks, if any.
    query_task_tracker_.TryCancelAll();
    auto* history_clusters_service =
        HistoryClustersServiceFactory::GetForBrowserContext(profile_);
    // TODO(crbug.com/1220765): Supply `continuation_max_time` in
    //  `result_callback` once the service supports paging.
    history_clusters_service->QueryClusters(
        query, end_time, max_count,
        base::BindOnce(std::move(result_callback), absl::nullopt),
        &query_task_tracker_);
  } else {
#if defined(CHROME_BRANDED)
    OnMemoriesDebugMessage(
        "HistoryClustersHandler Error: No UI Mocks on Official Build.");
    page_->OnClustersQueryResult(history_clusters::mojom::QueryResult::New());
#else
    OnMemoriesDebugMessage("HistoryClustersHandler: Loading UI Mock clusters.");
    // Cancel pending tasks, if any.
    query_task_tracker_.TryCancelAll();
    QueryHistoryService(query, end_time, max_count, {},
                        std::move(result_callback));
#endif
  }
}

void HistoryClustersHandler::RemoveVisits(
    std::vector<history_clusters::mojom::URLVisitPtr> visits,
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
    expire_args.end_time =
        visit_ptr->last_visit_time + base::TimeDelta::FromSeconds(1);
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

void HistoryClustersHandler::OnMemoriesDebugMessage(
    const std::string& message) {
  if (content::RenderFrameHost* rfh = web_contents_->GetMainFrame()) {
    rfh->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kInfo, message);
  }
}

void HistoryClustersHandler::OnClustersQueryResult(
    history_clusters::mojom::QueryParamsPtr original_query_params,
    const absl::optional<base::Time>& continuation_max_time,
    std::vector<history_clusters::mojom::ClusterPtr> cluster_mojoms) {
  auto result_mojom = history_clusters::mojom::QueryResult::New();
  result_mojom->query = original_query_params->query;
  // Continuation queries have a value for `max_time`. Mark the result as such.
  result_mojom->is_continuation = original_query_params->max_time.has_value();
  result_mojom->continuation_max_time = continuation_max_time;
  result_mojom->clusters = std::move(cluster_mojoms);
  page_->OnClustersQueryResult(std::move(result_mojom));
}

void HistoryClustersHandler::OnVisitsRemoved(
    std::vector<history_clusters::mojom::URLVisitPtr> visits) {
  page_->OnVisitsRemoved(std::move(visits));
}

#if !defined(CHROME_BRANDED)
void HistoryClustersHandler::QueryHistoryService(
    const std::string& query,
    base::Time max_time,
    size_t max_count,
    std::vector<history_clusters::mojom::ClusterPtr> cluster_mojoms,
    QueryResultsCallback callback) {
  if (max_count > 0 && cluster_mojoms.size() == max_count) {
    // Enough clusters have been created. Run the callback with those Clusters
    // along with the continuation max time threshold.
    std::move(callback).Run(max_time, std::move(cluster_mojoms));
    return;
  }

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history::QueryOptions query_options;
  query_options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;
  query_options.end_time = max_time;
  // Make sure to look back far enough to find some visits.
  query_options.begin_time =
      query_options.end_time.LocalMidnight() - base::TimeDelta::FromDays(90);
  history_service->QueryHistory(
      base::UTF8ToUTF16(query), query_options,
      base::BindOnce(&HistoryClustersHandler::OnHistoryQueryResults,
                     weak_ptr_factory_.GetWeakPtr(), query, max_time, max_count,
                     std::move(cluster_mojoms), std::move(callback)),
      &query_task_tracker_);
}

void HistoryClustersHandler::OnHistoryQueryResults(
    const std::string& query,
    base::Time max_time,
    size_t max_count,
    std::vector<history_clusters::mojom::ClusterPtr> cluster_mojoms,
    QueryResultsCallback callback,
    history::QueryResults results) {
  if (results.empty()) {
    // No more results to create Clusters from. Run the callback with the
    // Clusters created so far.
    std::move(callback).Run(max_time, std::move(cluster_mojoms));
    return;
  }

  auto cluster_mojom = history_clusters::mojom::Cluster::New();
  cluster_mojom->id = rand();

  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile_);

  // Keep track of related searches in this cluster.
  std::set<std::u16string> related_searches;

  // Keep track of visits in this cluster.
  std::vector<history_clusters::mojom::URLVisitPtr> visits;

  // Keep track of the randomly generated scores between 0 and 1.
  std::vector<double> scores;

  for (const auto& result : results) {
    // Last visit time of the Cluster is the visit time of most recently visited
    // URL in the Cluster. Collect all the visits in that day into the Cluster.
    if (cluster_mojom->last_visit_time.is_null()) {
      cluster_mojom->last_visit_time = result.visit_time();
    } else if (cluster_mojom->last_visit_time.LocalMidnight() >
               result.visit_time()) {
      break;
    }

    auto visit = history_clusters::mojom::URLVisit::New();
    visit->raw_urls.push_back(result.url());
    visit->normalized_url = result.url();
    visit->page_title = base::UTF16ToUTF8(result.title());
    visit->last_visit_time = result.visit_time();
    visit->first_visit_time = result.visit_time();
    visit->relative_date = base::UTF16ToUTF8(ui::TimeFormat::Simple(
        ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
        base::Time::Now() - visit->last_visit_time));

    // Check if the URL is a valid search URL.
    std::u16string search_terms;
    bool is_valid_search_url =
        default_search_provider &&
        default_search_provider->ExtractSearchTermsFromURL(
            result.url(), search_terms_data, &search_terms) &&
        !search_terms.empty();
    // If the URL is a valid search URL, try to normalize it.
    if (is_valid_search_url) {
      const std::u16string& normalized_search_query =
          base::i18n::ToLower(base::CollapseWhitespace(search_terms, false));
      TemplateURLRef::SearchTermsArgs search_terms_args(
          normalized_search_query);
      const TemplateURLRef& search_url_ref = default_search_provider->url_ref();
      if (!search_url_ref.SupportsReplacement(search_terms_data)) {
        continue;
      }
      visit->normalized_url = GURL(search_url_ref.ReplaceSearchTerms(
          search_terms_args, search_terms_data));

      // Annotate the visit accordingly.
      visit->annotations.push_back(
          history_clusters::mojom::Annotation::kSearchResultsPage);

      // Also offer this as a related search query.
      related_searches.insert(normalized_search_query);
    }

    // Check if the URL is in a tab group.
    if (browser) {
      const TabStripModel* tab_strip_model = browser->tab_strip_model();
      const TabGroupModel* group_model = tab_strip_model->group_model();
      for (const auto& group_id : group_model->ListTabGroups()) {
        const TabGroup* tab_group = group_model->GetTabGroup(group_id);
        gfx::Range tabs = tab_group->ListTabs();
        for (uint32_t index = tabs.start(); index < tabs.end(); ++index) {
          content::WebContents* web_contents =
              tab_strip_model->GetWebContentsAt(index);
          if (result.url() == web_contents->GetLastCommittedURL()) {
            visit->annotations.push_back(
                history_clusters::mojom::Annotation::kTabGrouped);
            break;
          }
        }
      }
    }

    // Check if the URL is bookmarked.
    if (model && model->loaded()) {
      std::vector<bookmarks::UrlAndTitle> bookmarks;
      model->GetBookmarks(&bookmarks);
      for (const auto& bookmark : bookmarks) {
        if (result.url() == bookmark.url) {
          visit->annotations.push_back(
              history_clusters::mojom::Annotation::kBookmarked);
          break;
        }
      }
    }

    // Count `visit` toward duplicate visits if the same URL is seen before.
    auto duplicate_visit_it =
        std::find_if(visits.begin(), visits.end(), [&](const auto& visit_ptr) {
          return visit_ptr->normalized_url == visit->normalized_url;
        });
    if (duplicate_visit_it != visits.end()) {
      (*duplicate_visit_it)->raw_urls.push_back(result.url());
      (*duplicate_visit_it)->first_visit_time = result.visit_time();
    } else {
      visits.push_back(std::move(visit));
      scores.push_back(rand() / static_cast<double>(RAND_MAX));
    }
  }

  // Sort the randomly generated scores.
  std::sort(scores.begin(), scores.end());

  // Add the visits and the related searches to the cluster.
  for (auto& visit : visits) {
    // Visits get a score between 0 and 1 in descending order.
    visit->score = scores.back();
    scores.pop_back();

    if (cluster_mojom->visits.empty()) {
      // The first visit will be featured prominently and have related searches.
      cluster_mojom->visits.push_back(std::move(visit));

      for (auto& search_query : related_searches) {
        const std::string search_query_utf8 = base::UTF16ToUTF8(search_query);
        TemplateURLRef::SearchTermsArgs search_terms_args(search_query);
        const TemplateURLRef& search_url_ref =
            default_search_provider->url_ref();
        const std::string& search_url = search_url_ref.ReplaceSearchTerms(
            search_terms_args, search_terms_data);
        auto search_query_mojom = history_clusters::mojom::SearchQuery::New();
        search_query_mojom->query = search_query_utf8;
        search_query_mojom->url = GURL(search_url);
        cluster_mojom->visits[0]->related_searches.push_back(
            std::move(search_query_mojom));
      }
    } else {
      // The rest of the visits will related visits of the first one. Only the
      // first three related visits are visible by default.
      if (cluster_mojom->visits[0]->related_visits.size() >= 3) {
        visit->below_the_fold = true;
      }
      cluster_mojom->visits[0]->related_visits.push_back(std::move(visit));
    }
  }

  // Continue to extract Clusters from 11:59:59pm of the day before the
  // Cluster's `last_visit_time`.
  max_time = cluster_mojom->last_visit_time.LocalMidnight() -
             base::TimeDelta::FromSeconds(1);
  cluster_mojoms.push_back(std::move(cluster_mojom));
  QueryHistoryService(query, max_time, max_count, std::move(cluster_mojoms),
                      std::move(callback));
}
#endif
