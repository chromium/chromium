// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/favicon_cache.h"

#include <tuple>

#include "base/containers/lru_cache.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "components/favicon/core/favicon_service.h"
#include "components/omnibox/browser/autocomplete_result.h"

namespace {

size_t GetFaviconCacheSize() {
  // Set cache size to twice the number of maximum results in either the
  // on-focus or prefix-suggest mode to avoid favicon refetches as the user
  // types. Favicon fetches are uncached and can hit disk.
  return 2 * std::max(AutocompleteResult::GetMaxMatches(),
                      AutocompleteResult::GetMaxMatches(true));
}

}  // namespace

bool FaviconCache::Request::operator<(const Request& rhs) const {
  // Compare |type| first, and if equal, compare |url|.
  return std::tie(type, url) < std::tie(rhs.type, rhs.url);
}

FaviconCache::FaviconCache(favicon::FaviconService* favicon_service,
                           history::HistoryService* history_service)
    : favicon_service_(favicon_service),
      lru_cache_(GetFaviconCacheSize()),
      responses_without_favicons_(GetFaviconCacheSize()) {
  if (history_service) {
    history_observation_.Observe(history_service);

    favicons_changed_subscription_ =
        history_service->AddFaviconsChangedCallback(base::BindRepeating(
            &FaviconCache::OnFaviconsChanged, weak_factory_.GetWeakPtr()));
  }
}

FaviconCache::~FaviconCache() = default;

gfx::Image FaviconCache::GetFaviconForPageUrl(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return GetFaviconInternal({RequestType::BY_PAGE_URL, page_url},
                            std::move(on_favicon_fetched));
}

gfx::Image FaviconCache::GetLargestFaviconForPageUrl(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return GetFaviconInternal({RequestType::RAW_BY_PAGE_URL, page_url},
                            std::move(on_favicon_fetched));
}

gfx::Image FaviconCache::GetFaviconForIconUrl(
    const GURL& icon_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return GetFaviconInternal({RequestType::BY_ICON_URL, icon_url},
                            std::move(on_favicon_fetched));
}

gfx::Image FaviconCache::GetFaviconInternal(
    const Request& request,
    FaviconFetchedCallback on_favicon_fetched) {
  if (!favicon_service_)
    return gfx::Image();

  if (request.url.is_empty() || !request.url.is_valid())
    return gfx::Image();

  // Early exit if we have a cached favicon ready.
  auto cache_iterator = lru_cache_.Get(request);
  if (cache_iterator != lru_cache_.end())
    return cache_iterator->second;

  // Early exit if we've already established that we don't have the favicon.
  if (responses_without_favicons_.Peek(request) !=
      responses_without_favicons_.end()) {
    return gfx::Image();
  }

  // We have an outstanding request for this page. Add one more waiting callback
  // and return an empty gfx::Image.
  auto it = pending_requests_.find(request);
  if (it != pending_requests_.end()) {
    it->second.push_back(std::move(on_favicon_fetched));
    return gfx::Image();
  }

  if (request.type == RequestType::BY_PAGE_URL) {
    favicon_service_->GetFaviconImageForPageURL(
        request.url,
        base::BindRepeating(&FaviconCache::OnFaviconFetched,
                            weak_factory_.GetWeakPtr(), request),
        &task_tracker_);
  } else if (request.type == RequestType::RAW_BY_PAGE_URL) {
    favicon_service_->GetRawFaviconForPageURL(
        request.url, {favicon_base::IconType::kFavicon},
        /*icon_size_in_pixels=*/0, /*fallback_to_host=*/false,
        base::BindRepeating(&FaviconCache::OnFaviconRawBitmapFetched,
                            weak_factory_.GetWeakPtr(), request),
        &task_tracker_);
  } else if (request.type == RequestType::BY_ICON_URL) {
    favicon_service_->GetFaviconImage(
        request.url,
        base::BindRepeating(&FaviconCache::OnFaviconFetched,
                            weak_factory_.GetWeakPtr(), request),
        &task_tracker_);
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  pending_requests_[request].push_back(std::move(on_favicon_fetched));

  return gfx::Image();
}

void FaviconCache::OnFaviconFetched(
    const Request& request,
    const favicon_base::FaviconImageResult& result) {
  if (result.image.IsEmpty()) {
    responses_without_favicons_.Put(request, true);
    pending_requests_.erase(request);
    return;
  }

  InvokeRequestCallbackWithFavicon(request, result.image);
}

void FaviconCache::OnFaviconRawBitmapFetched(
    const Request& request,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (!bitmap_result.is_valid()) {
    responses_without_favicons_.Put(request, true);
    pending_requests_.erase(request);
    return;
  }

  InvokeRequestCallbackWithFavicon(
      request, gfx::Image::CreateFrom1xPNGBytes(bitmap_result.bitmap_data));
}

void FaviconCache::InvokeRequestCallbackWithFavicon(const Request& request,
                                                    const gfx::Image& image) {
  DCHECK(!image.IsEmpty());
  lru_cache_.Put(request, image);

  auto it = pending_requests_.find(request);
  CHECK(it != pending_requests_.end(), base::NotFatalUntil::M130);
  for (auto& callback : it->second) {
    std::move(callback).Run(image);
  }
  pending_requests_.erase(it);
}

void FaviconCache::OnURLVisited(history::HistoryService* history_service,
                                const history::URLRow& url_row,
                                const history::VisitRow& new_visit) {
  auto it = responses_without_favicons_.Peek(
      {RequestType::BY_PAGE_URL, url_row.url()});
  if (it != responses_without_favicons_.end())
    responses_without_favicons_.Erase(it);
}

void FaviconCache::InvalidateCachedRequests(const Request& request) {
  {
    auto it = lru_cache_.Peek(request);
    if (it != lru_cache_.end())
      lru_cache_.Erase(it);
  }

  {
    auto it = responses_without_favicons_.Peek(request);
    if (it != responses_without_favicons_.end())
      responses_without_favicons_.Erase(it);
  }
}

void FaviconCache::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // We only care about actual user (or sync) deletions.
  if (deletion_info.is_from_expiration())
    return;

  if (deletion_info.IsAllHistory()) {
    lru_cache_.Clear();
    responses_without_favicons_.Clear();
    return;
  }

  for (const history::URLRow& row : deletion_info.deleted_rows()) {
    InvalidateCachedRequests({RequestType::BY_PAGE_URL, row.url()});
  }
}

void FaviconCache::OnFaviconsChanged(const std::set<GURL>& page_urls,
                                     const GURL& icon_url) {
  for (GURL page_url : page_urls) {
    InvalidateCachedRequests({RequestType::BY_PAGE_URL, page_url});
  }

  InvalidateCachedRequests({RequestType::BY_ICON_URL, icon_url});
}
