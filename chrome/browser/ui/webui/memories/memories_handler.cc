// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memories/memories_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/history_clusters/core/memories_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if !defined(CHROME_BRANDED)
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/time_formatting.h"
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

namespace {

GURL GetRandomlySizedThumbnailUrl() {
  const std::vector<int> dimensions = {150, 160, 170, 180, 190, 200};
  auto random_dimension = [&dimensions]() {
    return dimensions[rand() % dimensions.size()];
  };
  return GURL(base::StringPrintf("https://via.placeholder.com/%dX%d",
                                 random_dimension(), random_dimension()));
}

}  // namespace
#endif

MemoriesHandler::MemoriesHandler(
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

MemoriesHandler::~MemoriesHandler() = default;

void MemoriesHandler::SetPage(
    mojo::PendingRemote<history_clusters::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void MemoriesHandler::QueryMemories(
    history_clusters::mojom::QueryParamsPtr query_params) {
  auto result_mojom = history_clusters::mojom::MemoriesResult::New();
  result_mojom->title = query_params->query;
  result_mojom->thumbnail_url = GetRandomlySizedThumbnailUrl();
  if (!query_params->recency_threshold.has_value()) {
    // The default value for the recency threshold should be the present time.
    query_params->recency_threshold = base::Time::Now();
  } else {
    // Continuation queries have a value for the recency threshold. Mark the
    // result as such.
    result_mojom->is_continuation = true;
  }
  auto result_callback =
      base::BindOnce(&MemoriesHandler::OnMemoriesQueryResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_mojom));
  if (history_clusters::RemoteModelEndpoint().is_valid()) {
    // Cancel pending queries, if any.
    query_task_tracker_.TryCancelAll();
    auto* history_clusters_service =
        HistoryClustersServiceFactory::GetForBrowserContext(profile_);
    history_clusters_service->QueryMemories(
        std::move(query_params),
        base::BindOnce(
            [](base::OnceCallback<void(
                   history_clusters::mojom::QueryParamsPtr,
                   std::vector<history_clusters::mojom::MemoryPtr>)> callback,
               history_clusters::HistoryClustersService::QueryMemoriesResponse
                   response) {
              std::move(callback).Run(std::move(response.query_params),
                                      std::move(response.clusters));
            },
            std::move(result_callback)),
        &query_task_tracker_);
  } else {
#if defined(CHROME_BRANDED)
    page_->OnMemoriesQueryResult(
        history_clusters::mojom::MemoriesResult::New());
#else
    // Cancel pending queries, if any.
    query_task_tracker_.TryCancelAll();
    QueryHistoryService(std::move(query_params), {},
                        std::move(result_callback));
#endif
  }
}

void MemoriesHandler::RemoveVisits(
    std::vector<history_clusters::mojom::VisitPtr> visits,
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
    // ExpireHistoryArgs::end_time is not inclusive. Make sure all visits in the
    // given timespan are removed by adding 1 second to it.
    expire_args.end_time = visit_ptr->time + base::TimeDelta::FromSeconds(1);
    expire_args.begin_time = visit_ptr->first_visit_time;
  }
  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile_);
  history_clusters_service->RemoveVisits(
      expire_list,
      base::BindOnce(&MemoriesHandler::OnVisitsRemoved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(visits)),
      &remove_task_tracker_);
  std::move(callback).Run(true);
}

void MemoriesHandler::OnMemoriesDebugMessage(const std::string& message) {
  if (content::RenderFrameHost* rfh = web_contents_->GetMainFrame()) {
    rfh->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kInfo, message);
  }
}

void MemoriesHandler::OnMemoriesQueryResult(
    history_clusters::mojom::MemoriesResultPtr result_mojom,
    history_clusters::mojom::QueryParamsPtr continuation_query_params,
    std::vector<history_clusters::mojom::MemoryPtr> memory_mojoms) {
  result_mojom->continuation_query_params =
      std::move(continuation_query_params);
  result_mojom->memories = std::move(memory_mojoms);
  page_->OnMemoriesQueryResult(std::move(result_mojom));
}

void MemoriesHandler::OnVisitsRemoved(
    std::vector<history_clusters::mojom::VisitPtr> visits) {
  page_->OnVisitsRemoved(std::move(visits));
}

#if !defined(CHROME_BRANDED)
void MemoriesHandler::QueryHistoryService(
    history_clusters::mojom::QueryParamsPtr query_params,
    std::vector<history_clusters::mojom::MemoryPtr> memory_mojoms,
    MemoriesQueryResultsCallback callback) {
  const size_t max_count =
      query_params->max_count ? query_params->max_count : -1;
  if (memory_mojoms.size() == max_count) {
    // Enough Memories have been created. Run the callback with those Memories
    // along with the continuation query params.
    std::move(callback).Run(std::move(query_params), std::move(memory_mojoms));
    return;
  }

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history::QueryOptions query_options;
  query_options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;
  query_options.end_time =
      query_params->recency_threshold.value_or(base::Time::Now());
  // Make sure to look back far enough to find some visits.
  query_options.begin_time =
      query_options.end_time.LocalMidnight() - base::TimeDelta::FromDays(30);
  std::u16string query = base::UTF8ToUTF16(query_params->query);
  history_service->QueryHistory(
      query, query_options,
      base::BindOnce(&MemoriesHandler::OnHistoryQueryResults,
                     weak_ptr_factory_.GetWeakPtr(), std::move(query_params),
                     std::move(memory_mojoms), std::move(callback)),
      &query_task_tracker_);
}

void MemoriesHandler::OnHistoryQueryResults(
    history_clusters::mojom::QueryParamsPtr query_params,
    std::vector<history_clusters::mojom::MemoryPtr> memory_mojoms,
    MemoriesQueryResultsCallback callback,
    history::QueryResults results) {
  if (results.empty()) {
    // No more results to create Memories from. Run the callback with the
    // Memories created so far along with the continuation query params.
    std::move(callback).Run(std::move(query_params), std::move(memory_mojoms));
    return;
  }

  auto memory_mojom = history_clusters::mojom::Memory::New();
  memory_mojom->id = base::UnguessableToken::Create();

  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();

  // Keep track of the visited URLs and their titles in this memory.
  std::set<std::pair<GURL, std::u16string>> visited_urls;

  for (const auto& result : results) {
    // Last visit time of the Memory is the visit time of most recently visited
    // URL in the Memory. Collect all the visits in that day into the Memory.
    if (memory_mojom->last_visit_time.is_null()) {
      memory_mojom->last_visit_time = result.visit_time();
    } else if (memory_mojom->last_visit_time.LocalMidnight() >
               result.visit_time()) {
      break;
    }

    visited_urls.insert(std::make_pair(result.url(), result.title()));

    // Check if the URL is a valid search URL.
    std::u16string search_terms;
    bool is_valid_search_url =
        default_search_provider &&
        default_search_provider->ExtractSearchTermsFromURL(
            result.url(), search_terms_data, &search_terms) &&
        !search_terms.empty();
    if (is_valid_search_url) {
      // If the URL is a valid search URL, try to create a related search query.
      const std::u16string& search_query =
          base::i18n::ToLower(base::CollapseWhitespace(search_terms, false));
      const std::string search_query_utf8 = base::UTF16ToUTF8(search_query);

      // Skip duplicate search queries.
      if (base::Contains(memory_mojom->related_searches, search_query_utf8,
                         [](const auto& search_query_ptr) {
                           return search_query_ptr->query;
                         })) {
        continue;
      }

      TemplateURLRef::SearchTermsArgs search_terms_args(search_query);
      const TemplateURLRef& search_url_ref = default_search_provider->url_ref();
      const std::string& search_url = search_url_ref.ReplaceSearchTerms(
          search_terms_args, search_terms_data);
      auto search_query_mojom = history_clusters::mojom::SearchQuery::New();
      search_query_mojom->query = search_query_utf8;
      search_query_mojom->url = GURL(search_url);
      memory_mojom->related_searches.push_back(std::move(search_query_mojom));
    } else {  // !is_valid_search_url
      // If the URL is not a search URL, try to add the visit to the top visits.
      auto visit = history_clusters::mojom::Visit::New();
      // TODO(mahmadi): URLResult does not contain visit_id.
      visit->url = result.url();
      visit->page_title = base::UTF16ToUTF8(result.title());
      visit->thumbnail_url = GetRandomlySizedThumbnailUrl();
      visit->time = result.visit_time();
      visit->first_visit_time = result.visit_time();
      visit->relative_date = base::UTF16ToUTF8(ui::TimeFormat::Simple(
          ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
          base::Time::Now() - visit->time));
      visit->time_of_day =
          base::UTF16ToUTF8(base::TimeFormatTimeOfDay(visit->time));

      std::function<void(std::vector<history_clusters::mojom::VisitPtr>&, bool)>
          add_visit;
      add_visit = [&visit, &add_visit](auto& visits, bool are_top_visits) {
        // Count `visit` toward duplicate visits if the same URL is seen before.
        auto duplicate_visit_it = std::find_if(
            visits.begin(), visits.end(), [&visit](const auto& visit_ptr) {
              return visit_ptr->url == visit->url;
            });
        if (duplicate_visit_it != visits.end()) {
          (*duplicate_visit_it)->num_duplicate_visits++;
          (*duplicate_visit_it)->first_visit_time = visit->time;
          return;
        }
        // For the top visits, if the domain name is seen before, add `visit` to
        // the related visits of the respective top visit recursively.
        if (are_top_visits) {
          auto related_visit_it = std::find_if(
              visits.begin(), visits.end(), [&visit](const auto& visit_ptr) {
                return visit_ptr->url.host() == visit->url.host();
              });
          if (related_visit_it != visits.end()) {
            add_visit((*related_visit_it)->related_visits, false);
            return;
          }
        }
        // Otherwise, simply add `visit` to the list of visits.
        visits.push_back(std::move(visit));
      };
      add_visit(memory_mojom->top_visits, true);
    }
  }

  // Add related tab groups (tab groups containing any of the visited URLs).
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (browser) {
    const TabStripModel* tab_strip_model = browser->tab_strip_model();
    const TabGroupModel* group_model = tab_strip_model->group_model();
    for (const auto& group_id : group_model->ListTabGroups()) {
      const TabGroup* tab_group = group_model->GetTabGroup(group_id);
      bool tab_group_has_url_in_memory = false;
      gfx::Range tabs = tab_group->ListTabs();
      for (uint32_t index = tabs.start(); index < tabs.end(); ++index) {
        content::WebContents* web_contents =
            tab_strip_model->GetWebContentsAt(index);
        const GURL& url = web_contents->GetLastCommittedURL();
        if (base::Contains(visited_urls, url,
                           [](const auto& pair) { return pair.first; })) {
          tab_group_has_url_in_memory = true;
          break;
        }
      }
      if (tab_group_has_url_in_memory) {
        auto tab_group_mojom = history_clusters::mojom::TabGroup::New();
        tab_group_mojom->id = tab_group->id().token();
        tab_group_mojom->title =
            base::UTF16ToUTF8(tab_group->visual_data()->title());
        for (uint32_t index = tabs.start(); index < tabs.end(); ++index) {
          content::WebContents* web_contents =
              tab_strip_model->GetWebContentsAt(index);
          auto webpage = history_clusters::mojom::WebPage::New();
          webpage->url = web_contents->GetLastCommittedURL();
          webpage->title = base::UTF16ToUTF8(web_contents->GetTitle());
          webpage->thumbnail_url = GetRandomlySizedThumbnailUrl();
          tab_group_mojom->pages.push_back(std::move(webpage));
        }
        memory_mojom->related_tab_groups.push_back(std::move(tab_group_mojom));
      }
    }
  }

  // Add related bookmarks (bookmarked URLs among the visited URLs).
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  if (model && model->loaded()) {
    std::vector<bookmarks::UrlAndTitle> bookmarks;
    model->GetBookmarks(&bookmarks);
    for (const auto& bookmark : bookmarks) {
      auto matching_visited_url_it = std::find_if(
          visited_urls.begin(), visited_urls.end(),
          [&bookmark](const auto& pair) { return pair.first == bookmark.url; });
      if (matching_visited_url_it != visited_urls.end()) {
        auto webpage = history_clusters::mojom::WebPage::New();
        webpage->url = matching_visited_url_it->first;
        webpage->title = base::UTF16ToUTF8(matching_visited_url_it->second);
        webpage->thumbnail_url = GetRandomlySizedThumbnailUrl();
        memory_mojom->bookmarks.push_back(std::move(webpage));
      }
    }
  }

  // Continue to extract Memories. Set the recency threshold to 11:59:59pm of
  // the day before the Memory's `last_visit_time`.
  query_params->recency_threshold =
      memory_mojom->last_visit_time.LocalMidnight() -
      base::TimeDelta::FromSeconds(1);
  memory_mojoms.push_back(std::move(memory_mojom));
  QueryHistoryService(std::move(query_params), std::move(memory_mojoms),
                      std::move(callback));
}
#endif
