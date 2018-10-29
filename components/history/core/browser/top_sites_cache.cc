// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/top_sites_cache.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"

namespace history {

TopSitesCache::CanonicalURLQuery::CanonicalURLQuery(const GURL& url) {
  most_visited_url_.redirects.push_back(url);
  entry_.first = &most_visited_url_;
  entry_.second = 0u;
}

TopSitesCache::CanonicalURLQuery::~CanonicalURLQuery() {
}

TopSitesCache::TopSitesCache() : num_forced_urls_(0) {
  clear_query_ref_.ClearQuery();
  clear_query_ref_.ClearRef();
  clear_path_query_ref_.ClearQuery();
  clear_path_query_ref_.ClearRef();
  clear_path_query_ref_.ClearPath();
}

TopSitesCache::~TopSitesCache() {
}

void TopSitesCache::SetTopSites(const MostVisitedURLList& top_sites) {
  top_sites_ = top_sites;
  CountForcedURLs();
  GenerateCanonicalURLs();
}

void TopSitesCache::SetThumbnails(const URLToImagesMap& images) {
  images_ = images;
}

void TopSitesCache::ClearUnreferencedThumbnails() {
  URLToImagesMap images_to_keep;
  for (const std::pair<GURL, Images>& entry : images_) {
    if (IsKnownURL(entry.first)) {
      images_to_keep.insert(entry);
    }
  }
  images_ = std::move(images_to_keep);
}

Images* TopSitesCache::GetImage(const GURL& url) {
  return &images_[GetCanonicalURL(url)];
}

bool TopSitesCache::GetPageThumbnail(
    const GURL& url,
    scoped_refptr<base::RefCountedMemory>* bytes) const {
  auto found = images_.find(GetCanonicalURL(url));
  if (found != images_.end()) {
    base::RefCountedMemory* data = found->second.thumbnail.get();
    if (data) {
      *bytes = data;
      return true;
    }
  }
  return false;
}

bool TopSitesCache::GetPageThumbnailScore(const GURL& url,
                                          ThumbnailScore* score) const {
  auto found = images_.find(GetCanonicalURL(url));
  if (found != images_.end()) {
    *score = found->second.thumbnail_score;
    return true;
  }
  return false;
}

const GURL& TopSitesCache::GetCanonicalURL(const GURL& url) const {
  auto it = GetCanonicalURLsIterator(url);
  return it == canonical_urls_.end() ? url : it->first.first->url;
}

GURL TopSitesCache::GetGeneralizedCanonicalURL(const GURL& url) const {
  auto it_hi = canonical_urls_.lower_bound(CanonicalURLQuery(url).entry());
  if (it_hi != canonical_urls_.end()) {
    // Test match ignoring "?query#ref". This also handles exact match.
    if (url.ReplaceComponents(clear_query_ref_) ==
        GetURLFromIterator(it_hi).ReplaceComponents(clear_query_ref_)) {
      return it_hi->first.first->url;
    }
  }
  // Everything on or after |it_hi| is irrelevant.

  GURL base_url(url.ReplaceComponents(clear_path_query_ref_));
  auto it_lo = canonical_urls_.lower_bound(CanonicalURLQuery(base_url).entry());
  if (it_lo == canonical_urls_.end())
    return GURL::EmptyGURL();
  GURL compare_url_lo(GetURLFromIterator(it_lo));
  if (!HaveSameSchemeHostAndPort(base_url, compare_url_lo) ||
      !IsPathPrefix(base_url.path(), compare_url_lo.path())) {
    return GURL::EmptyGURL();
  }
  // Everything before |it_lo| is irrelevant.

  // Search in [|it_lo|, |it_hi|) in reversed order. The first URL found that's
  // a prefix of |url| (ignoring "?query#ref") would be returned.
  for (auto it = it_hi; it != it_lo;) {
    --it;
    GURL compare_url(GetURLFromIterator(it));
    DCHECK(HaveSameSchemeHostAndPort(compare_url, url));
    if (IsPathPrefix(compare_url.path(), url.path()))
      return it->first.first->url;
  }

  return GURL::EmptyGURL();
}

bool TopSitesCache::IsKnownURL(const GURL& url) const {
  return GetCanonicalURLsIterator(url) != canonical_urls_.end();
}

size_t TopSitesCache::GetURLIndex(const GURL& url) const {
  DCHECK(IsKnownURL(url));
  return GetCanonicalURLsIterator(url)->second;
}

size_t TopSitesCache::GetNumNonForcedURLs() const {
  return top_sites_.size() - num_forced_urls_;
}

size_t TopSitesCache::GetNumForcedURLs() const {
  return num_forced_urls_;
}

void TopSitesCache::CountForcedURLs() {
  num_forced_urls_ = 0;
  while (num_forced_urls_ < top_sites_.size()) {
    // Forced sites are all at the beginning.
    if (top_sites_[num_forced_urls_].last_forced_time.is_null())
      break;
    num_forced_urls_++;
  }
#if DCHECK_IS_ON()
  // In debug, ensure the cache user has no forced URLs pass that point.
  for (size_t i = num_forced_urls_; i < top_sites_.size(); ++i) {
    DCHECK(top_sites_[i].last_forced_time.is_null())
        << "All the forced URLs must appear before non-forced URLs.";
  }
#endif
}

void TopSitesCache::GenerateCanonicalURLs() {
  canonical_urls_.clear();
  for (size_t i = 0; i < top_sites_.size(); i++)
    StoreRedirectChain(top_sites_[i].redirects, i);
}

void TopSitesCache::StoreRedirectChain(const RedirectList& redirects,
                                       size_t destination) {
  // |redirects| is empty if the user pinned a site and there are not enough top
  // sites before the pinned site.

  // Map all the redirected URLs to the destination.
  for (size_t i = 0; i < redirects.size(); i++) {
    // If this redirect is already known, don't replace it with a new one.
    if (!IsKnownURL(redirects[i])) {
      CanonicalURLEntry entry;
      entry.first = &(top_sites_[destination]);
      entry.second = i;
      canonical_urls_[entry] = destination;
    }
  }
}

TopSitesCache::CanonicalURLs::const_iterator
    TopSitesCache::GetCanonicalURLsIterator(const GURL& url) const {
  return canonical_urls_.find(CanonicalURLQuery(url).entry());
}

const GURL& TopSitesCache::GetURLFromIterator(
    CanonicalURLs::const_iterator it) const {
  DCHECK(it != canonical_urls_.end());
  return it->first.first->redirects[it->first.second];
}

}  // namespace history
