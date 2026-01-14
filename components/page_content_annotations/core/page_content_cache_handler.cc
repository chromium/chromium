// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_cache_handler.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/page_content_annotations/core/page_content_cache.h"
#include "components/page_content_annotations/core/web_state_wrapper.h"

namespace page_content_annotations {

namespace {

void RecordExtractionAndCachingStatus(
    PageContentExtractionAndCachingStatus status) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PageContentCache.ExtractionAndCachingStatus", status);
}

}  // namespace

void PageContentCacheHandler::OnPageContentRetrievedForClosedTab(
    int64_t tab_id,
    std::optional<optimization_guide::PageContentResult> result) {
  if (!result.has_value()) {
    return;
  }

  // The closed tab may have been reopened or committed to closure by the time
  // we get to update the cache. In either case, ignore the cached contents.
  auto it = closed_tabs_.find(tab_id);
  if (it == closed_tabs_.end()) {
    return;
  }

  std::unique_ptr<optimization_guide::PageContentResult> data =
      std::make_unique<optimization_guide::PageContentResult>();
  data->url = result->url;
  data->navigation_timestamp = result->navigation_timestamp;
  data->extraction_time = result->extraction_time;
  data->page_context = std::move(result->page_context);
  closed_tabs_[tab_id] = std::move(data);
}

PageContentCacheHandler::PageContentCacheHandler(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    const base::FilePath& profile_path,
    base::TimeDelta max_context_age)
    : page_content_cache_(std::make_unique<PageContentCache>(os_crypt_async,
                                                             profile_path,
                                                             max_context_age)) {
}

PageContentCacheHandler::~PageContentCacheHandler() = default;

void PageContentCacheHandler::OnTabClosed(int64_t tab_id) {
  // Asynchronously fetch content to avoid blocking the UI thread. Stores
  // tab ID in the map so that we can track and prevent double-caching
  // (cached content associated with the tab ID in both the closed tabs map and
  // the non-closed tabs database) as well as avoid potential race conditions
  // where the tab closure is committed before we finish fetching the content.
  closed_tabs_[tab_id] = nullptr;
  page_content_cache_->GetPageContentForTab(
      tab_id, base::BindOnce(
                  &PageContentCacheHandler::OnPageContentRetrievedForClosedTab,
                  weak_ptr_factory_.GetWeakPtr(), tab_id));
  RecordExtractionAndCachingStatus(
      PageContentExtractionAndCachingStatus::kContentsDeletedOnTabClose);
  page_content_cache_->RemovePageContentForTab(tab_id);
}

void PageContentCacheHandler::OnTabCloseUndone(int64_t tab_id) {
  auto it = closed_tabs_.find(tab_id);
  if (it == closed_tabs_.end()) {
    return;
  }
  const std::unique_ptr<optimization_guide::PageContentResult> data =
      std::move(it->second);
  closed_tabs_.erase(it);
  if (data) {
    UpdateCache(tab_id, data->url, data->navigation_timestamp,
                data->extraction_time, std::move(data->page_context));
    RecordExtractionAndCachingStatus(PageContentExtractionAndCachingStatus::
                                         kContentsAvailableOnTabCloseUndone);
  }
}

void PageContentCacheHandler::OnVisibilityChanged(
    std::optional<int64_t> tab_id,
    const WebStateWrapper& web_state,
    std::optional<optimization_guide::proto::PageContext> page_context,
    const base::Time& extraction_time) {
  if (!tab_id || web_state.is_off_the_record || IsTabClosed(tab_id.value())) {
    return;
  }
  if (web_state.visibility != PageContentVisibility::kHidden) {
    return;
  }
  if (!page_context) {
    RecordExtractionAndCachingStatus(PageContentExtractionAndCachingStatus::
                                         kContentsNotAvailableWhenBackgrounded);
    return;
  }
  // Even if background trigger is enabled, update the cache with available page
  // contents. This is to avoid losing context if tab was killed as soon as it
  // was hidden. If extraction succeeds, then cache would be updated again in
  // ProcessPageContentExtraction().
  UpdateCache(*tab_id, web_state.last_committed_url,
              web_state.navigation_timestamp, extraction_time,
              std::move(*page_context));
  RecordExtractionAndCachingStatus(PageContentExtractionAndCachingStatus::
                                       kContentsAvailableWhenBackgrounded);
}

void PageContentCacheHandler::OnNewNavigation(
    std::optional<int64_t> tab_id,
    const WebStateWrapper& web_state) {
  if (!tab_id || web_state.is_off_the_record) {
    return;
  }
  RecordExtractionAndCachingStatus(
      PageContentExtractionAndCachingStatus::kContentsDeletedOnTabUpdate);
  // Delete cached contents for the tab_id when page is updated.
  page_content_cache_->RemovePageContentForTab(*tab_id);
}

void PageContentCacheHandler::TabClosureCommitted(int64_t tab_id) {
  // We only remove the value of the cache here, and not the tab id itself
  // in case events such as OnVisibilityChanged are received after the closure
  // is committed. Keeping the tab id here allows us to keep tracking the tab
  // as closed, and avoid trying to extract its web contents and caching it.
  closed_tabs_.erase(tab_id);
  committed_closed_tabs_.insert(tab_id);
}

void PageContentCacheHandler::ProcessPageContentExtraction(
    std::optional<int64_t> tab_id,
    const WebStateWrapper& web_state,
    const optimization_guide::proto::PageContext& page_context,
    const base::Time& extraction_time) {
  if (!tab_id || web_state.is_off_the_record || IsTabClosed(tab_id.value())) {
    return;
  }

  // This method only handles the case when extraction finishes when tab is
  // already backgrounded. We do not cache contents for active tab since it can
  // be extracted on demand.
  if (web_state.visibility == PageContentVisibility::kHidden) {
    RecordExtractionAndCachingStatus(
        PageContentExtractionAndCachingStatus::kExtractionObservedInBackground);
    UpdateCache(*tab_id, web_state.last_committed_url,
                web_state.navigation_timestamp, extraction_time,
                std::move(page_context));
  } else {
    RecordExtractionAndCachingStatus(
        PageContentExtractionAndCachingStatus::kExtractionObservedInForeground);
  }
}

bool PageContentCacheHandler::IsTabClosed(int64_t tab_id) const {
  return closed_tabs_.contains(tab_id) ||
         committed_closed_tabs_.contains(tab_id);
}

void PageContentCacheHandler::UpdateCache(
    int64_t tab_id,
    const GURL& url,
    const base::Time& navigation_timestamp,
    const base::Time& extraction_time,
    const optimization_guide::proto::PageContext& page_context) {
  if (IsTabClosed(tab_id)) {
    return;
  }
  page_content_cache_->CachePageContent(tab_id, url, navigation_timestamp,
                                        extraction_time,
                                        std::move(page_context));
}

}  // namespace page_content_annotations
