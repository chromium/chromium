// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/history_clusters/core/memories_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

namespace history_clusters {

namespace {

// Translate a `AnnotatedVisit` to `mojom::VisitPtr`.
mojom::URLVisitPtr VisitToMojom(
    const history::ScoredAnnotatedVisit& scored_annotated_visit) {
  auto visit_mojom = mojom::URLVisit::New();
  auto& annotated_visit = scored_annotated_visit.annotated_visit;
  visit_mojom->normalized_url = annotated_visit.url_row.url();
  visit_mojom->raw_urls.push_back(annotated_visit.url_row.url());
  visit_mojom->last_visit_time = annotated_visit.visit_row.visit_time;
  visit_mojom->first_visit_time = annotated_visit.visit_row.visit_time;
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
  visit_mojom->score = scored_annotated_visit.score;

  if (base::FeatureList::IsEnabled(kDebug)) {
    visit_mojom->debug_info["score"] = base::NumberToString(visit_mojom->score);
    visit_mojom->debug_info["visit_duration"] =
        base::NumberToString(scored_annotated_visit.annotated_visit.visit_row
                                 .visit_duration.InSecondsF());
  }

  return visit_mojom;
}

absl::optional<mojom::SearchQueryPtr> SearchQueryToMojom(
    const std::string& search_query,
    const TemplateURLService* template_url_service) {
  TemplateURLRef::SearchTermsArgs search_terms_args(
      base::UTF8ToUTF16(search_query));
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();
  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_search_provider ||
      !default_search_provider->url_ref().SupportsReplacement(
          search_terms_data)) {
    return absl::nullopt;
  }

  auto search_query_mojom = mojom::SearchQuery::New();
  search_query_mojom->query = search_query;
  search_query_mojom->url =
      GURL(default_search_provider->url_ref().ReplaceSearchTerms(
          search_terms_args, search_terms_data));
  return search_query_mojom;
}

void ServiceResultToMojom(
    Profile* profile,
    base::OnceCallback<
        void(const absl::optional<base::Time>& continuation_max_time,
             std::vector<mojom::ClusterPtr> cluster_mojoms)> callback,
    HistoryClustersService::QueryClustersResult result) {
  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  std::vector<mojom::ClusterPtr> clusters_mojoms;
  for (const auto& cluster : result.clusters) {
    auto cluster_mojom = mojom::Cluster::New();
    cluster_mojom->id = cluster.cluster_id;
    std::set<std::string> related_searches;  // Keeps track of unique searches.
    for (const auto& visit : cluster.scored_annotated_visits) {
      if (cluster_mojom->visits.empty()) {
        // First visit is always the top visit.
        cluster_mojom->visits.push_back(VisitToMojom(visit));
      } else {
        auto& top_visit = cluster_mojom->visits.front();
        DCHECK(visit.score <= top_visit->score);

        // After 3 related visits are attached, any subsequent visits scored
        // below 0.5 are considered below the fold. 0-scored "ghost" visits are
        // always considered below the fold. They are always hidden in
        // production, but shown when the kDebug flag is enabled for debugging.
        mojom::URLVisitPtr visit_mojom = VisitToMojom(visit);
        visit_mojom->below_the_fold = (top_visit->related_visits.size() > 3 &&
                                       visit_mojom->score < 0.5) ||
                                      visit_mojom->score == 0.0;

        // We leave any 0-scored visits (most likely duplicates) still in the
        // cluster, so that deleting the whole cluster deletes these related
        // duplicates too.
        top_visit->related_visits.push_back(std::move(visit_mojom));
      }

      auto& top_visit = cluster_mojom->visits.front();
      // The top visit's related searches are the set of related searches across
      // all the visits in the order they are encountered.
      for (const auto& search_query :
           visit.annotated_visit.content_annotations.related_searches) {
        if (!related_searches.insert(search_query).second) {
          continue;
        }

        auto search_query_mojom =
            SearchQueryToMojom(search_query, template_url_service);
        if (search_query_mojom) {
          top_visit->related_searches.emplace_back(
              std::move(*search_query_mojom));
        }
      }
    }

    clusters_mojoms.emplace_back(std::move(cluster_mojom));
  }

  // TODO(tommycli): Resolve the semantics mismatch where the C++ handler uses
  // `continuation_end_time` == base::Time() to represent exhausted history,
  // while the mojom uses an explicit absl::optional value.
  absl::optional<base::Time> continuation_end_time;
  if (!result.continuation_end_time.is_null()) {
    continuation_end_time = result.continuation_end_time;
  }

  std::move(callback).Run(continuation_end_time, std::move(clusters_mojoms));
}

// Exists temporarily only for developer usage. Never enabled via variations.
// TODO(mahmadi): Remove once on-device clustering backend is more mature.
const base::Feature kUIDevelopmentMakeFakeHistoryClusters{
    "UIDevelopmentMakeFakeHistoryClusters", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

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

void HistoryClustersHandler::QueryClusters(mojom::QueryParamsPtr query_params) {
  const std::string& query = query_params->query;
  const size_t max_count = query_params->max_count;
  base::Time end_time;
  if (query_params->end_time) {
    DCHECK(!query_params->end_time->is_null())
        << "Page called handler with non-null but invalid end_time.";
    end_time = *(query_params->end_time);
  }
  auto result_callback =
      base::BindOnce(&HistoryClustersHandler::OnClustersQueryResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(query_params));

  if (!base::FeatureList::IsEnabled(kUIDevelopmentMakeFakeHistoryClusters)) {
    // Cancel pending tasks, if any.
    query_task_tracker_.TryCancelAll();
    auto* history_clusters_service =
        HistoryClustersServiceFactory::GetForBrowserContext(profile_);
    history_clusters_service->QueryClusters(
        query, end_time, max_count,
        base::BindOnce(&ServiceResultToMojom, profile_,
                       std::move(result_callback)),
        &query_task_tracker_);
  } else {
#if defined(CHROME_BRANDED)
    OnMemoriesDebugMessage(
        "HistoryClustersHandler Error: No UI Mocks on Official Build.");
    page_->OnClustersQueryResult(mojom::QueryResult::New());
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
    mojom::QueryParamsPtr original_query_params,
    const absl::optional<base::Time>& continuation_end_time,
    std::vector<mojom::ClusterPtr> cluster_mojoms) {
  auto result_mojom = mojom::QueryResult::New();
  result_mojom->query = original_query_params->query;
  // Continuation queries have a value for `end_time`. Mark the result as such.
  result_mojom->is_continuation = original_query_params->end_time.has_value();
  result_mojom->continuation_end_time = continuation_end_time;
  result_mojom->clusters = std::move(cluster_mojoms);
  page_->OnClustersQueryResult(std::move(result_mojom));
}

void HistoryClustersHandler::OnVisitsRemoved(
    std::vector<mojom::URLVisitPtr> visits) {
  page_->OnVisitsRemoved(std::move(visits));
}

#if !defined(CHROME_BRANDED)
void HistoryClustersHandler::QueryHistoryService(
    const std::string& query,
    base::Time end_time,
    size_t max_count,
    std::vector<mojom::ClusterPtr> cluster_mojoms,
    QueryResultsCallback callback) {
  if (max_count > 0 && cluster_mojoms.size() == max_count) {
    // Enough clusters have been created. Run the callback with those Clusters
    // along with the continuation max time threshold.
    std::move(callback).Run(end_time, std::move(cluster_mojoms));
    return;
  }

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history::QueryOptions query_options;
  query_options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;
  query_options.end_time = end_time;
  history_service->QueryHistory(
      base::UTF8ToUTF16(query), query_options,
      base::BindOnce(&HistoryClustersHandler::OnHistoryQueryResults,
                     weak_ptr_factory_.GetWeakPtr(), query, end_time, max_count,
                     std::move(cluster_mojoms), std::move(callback)),
      &query_task_tracker_);
}

void HistoryClustersHandler::OnHistoryQueryResults(
    const std::string& query,
    base::Time end_time,
    size_t max_count,
    std::vector<mojom::ClusterPtr> cluster_mojoms,
    QueryResultsCallback callback,
    history::QueryResults results) {
  if (results.empty()) {
    // No more results to create Clusters from. Run the callback with the
    // Clusters created so far.
    std::move(callback).Run(end_time, std::move(cluster_mojoms));
    return;
  }

  auto cluster_mojom = mojom::Cluster::New();
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
  std::set<std::string> related_searches;

  // Keep track of visits in this cluster.
  std::vector<mojom::URLVisitPtr> visits;

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

    auto visit = mojom::URLVisit::New();
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
      visit->annotations.push_back(mojom::Annotation::kSearchResultsPage);

      // Also offer this as a related search query.
      related_searches.insert(base::UTF16ToUTF8(normalized_search_query));
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
            visit->annotations.push_back(mojom::Annotation::kTabGrouped);
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
          visit->annotations.push_back(mojom::Annotation::kBookmarked);
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
        auto search_query_mojom =
            SearchQueryToMojom(search_query, template_url_service);
        if (search_query_mojom) {
          cluster_mojom->visits[0]->related_searches.push_back(
              std::move(*search_query_mojom));
        }
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
  end_time = cluster_mojom->last_visit_time.LocalMidnight() -
             base::TimeDelta::FromSeconds(1);
  cluster_mojoms.push_back(std::move(cluster_mojom));
  QueryHistoryService(query, end_time, max_count, std::move(cluster_mojoms),
                      std::move(callback));
}
#endif

}  // namespace history_clusters
