// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memories/memories_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

#if !defined(OFFICIAL_BUILD)
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/time_formatting.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
#include "components/tab_groups/tab_group_id.h"
#endif

MemoriesHandler::MemoriesHandler(
    mojo::PendingReceiver<memories::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : profile_(profile),
      web_contents_(web_contents),
      page_handler_(this, std::move(pending_page_handler)) {
  DCHECK(profile_);
  DCHECK(web_contents_);
}

MemoriesHandler::~MemoriesHandler() = default;

void MemoriesHandler::SetPage(
    mojo::PendingRemote<memories::mojom::Page> pending_page) {
  page_.Bind(std::move(pending_page));
}

void MemoriesHandler::GetSampleMemories(const std::string& query,
                                        MemoriesResultCallback callback) {
#if defined(OFFICIAL_BUILD)
  std::move(callback).Run({});
  return;
#else
  // Query HistoryService for URLs containing |query|.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history::QueryOptions query_options;
  query_options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;
  query_options.SetRecentDayRange(90);
  history_service->QueryHistory(
      base::UTF8ToUTF16(query), query_options,
      base::BindOnce(&MemoriesHandler::OnHistoryQueryResults,
                     weak_ptr_factory_.GetWeakPtr(), query,
                     std::move(callback)),
      &history_task_tracker_);
}

void MemoriesHandler::OnHistoryQueryResults(const std::string& query,
                                            MemoriesResultCallback callback,
                                            history::QueryResults results) {
  auto memories_result_mojom = memories::mojom::MemoriesResult::New();
  memories_result_mojom->title =
      base::UTF8ToUTF16("Related to \"" + query + "\"");
  memories_result_mojom->thumbnail_url =
      GURL("https://via.placeholder.com/200");

  auto memory_mojom = memories::mojom::Memory::New();
  memory_mojom->id = base::UnguessableToken::Create();

  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  const SearchTermsData& search_terms_data =
      template_url_service->search_terms_data();

  for (const auto& result : results) {
    // Last visit time of the Memory is that of the most recently visited URL.
    if (memory_mojom->last_visit_time.is_null()) {
      memory_mojom->last_visit_time = result.visit_time();
    }

    // Keep track of all the visited URLs.
    auto webpage = memories::mojom::WebPage::New();
    webpage->url = result.url();
    webpage->title = result.title();
    webpage->thumbnail_url = GURL("https://via.placeholder.com/200");
    memory_mojom->pages[result.url()] = std::move(webpage);

    // Check if the URL is a valid search URL.
    base::string16 search_terms;
    bool is_valid_search_url =
        default_search_provider &&
        default_search_provider->ExtractSearchTermsFromURL(
            result.url(), search_terms_data, &search_terms) &&
        !search_terms.empty();
    if (is_valid_search_url) {
      // If the URL is a valid search URL, try to create a related search query.
      const base::string16& search_query =
          base::i18n::ToLower(base::CollapseWhitespace(search_terms, false));

      // Skip duplicate search queries.
      if (base::Contains(memory_mojom->related_searches, search_query,
                         [](const auto& search_query_ptr) {
                           return search_query_ptr->query;
                         })) {
        continue;
      }

      TemplateURLRef::SearchTermsArgs search_terms_args(search_query);
      const TemplateURLRef& search_url_ref = default_search_provider->url_ref();
      const std::string& search_url = search_url_ref.ReplaceSearchTerms(
          search_terms_args, search_terms_data);
      auto search_query_mojom = memories::mojom::SearchQuery::New();
      search_query_mojom->query = search_query;
      search_query_mojom->url = GURL(search_url);
      memory_mojom->related_searches.push_back(std::move(search_query_mojom));
    } else {  // !is_valid_search_url
      // If the URL is not a search URL, try to add the visit to the top visits.
      auto visit = memories::mojom::Visit::New();
      // TOOD(mahmadi): URLResult does not contain visit_id.
      visit->url = result.url();
      visit->time = result.visit_time();

      std::function<void(std::vector<memories::mojom::VisitPtr>&, bool)>
          add_visit;
      add_visit = [&visit, &add_visit](auto& visits, bool are_top_visits) {
        // Count |visit| toward duplicate visits if the same URL is seen before.
        auto duplicate_visit_it = std::find_if(
            visits.begin(), visits.end(), [&visit](const auto& visit_ptr) {
              return visit_ptr->url == visit->url;
            });
        if (duplicate_visit_it != visits.end()) {
          (*duplicate_visit_it)->num_duplicate_visits++;
          return;
        }
        // For the top visits, if the domain name is seen before, add |visit| to
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
        // Otherwise, simply add |visit| to the list of visits.
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
      std::vector<GURL> related_tab_group_urls;
      const TabGroup* tab_group = group_model->GetTabGroup(group_id);
      gfx::Range tabs = tab_group->ListTabs();
      for (uint32_t index = tabs.start(); index < tabs.end(); ++index) {
        content::WebContents* web_contents =
            tab_strip_model->GetWebContentsAt(index);
        const GURL& url = web_contents->GetLastCommittedURL();
        if (base::Contains(memory_mojom->pages, url)) {
          related_tab_group_urls.push_back(url);
        }
      }
      if (!related_tab_group_urls.empty()) {
        auto tab_group_mojom = memories::mojom::TabGroup::New();
        tab_group_mojom->id = tab_group->id().token();
        tab_group_mojom->title = tab_group->visual_data()->title();
        tab_group_mojom->urls = related_tab_group_urls;
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
      if (base::Contains(memory_mojom->pages, bookmark.url)) {
        memory_mojom->bookmarks.push_back(bookmark.url);
      }
    }
  }

  memories_result_mojom->memories.push_back(std::move(memory_mojom));
  std::move(callback).Run(std::move(memories_result_mojom));
#endif
}
