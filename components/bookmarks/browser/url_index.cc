// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/url_index.h"

#include <iterator>

#include "base/containers/adapters.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/common/url_load_stats.h"

namespace bookmarks {

namespace {

// Computes and aggregates into |*stats| metrics corresponding to a particular
// group of bookmarks with the same URL, listed in |*bookmarks_with_same_url|.
// The caller is responsible to guarantee that these provided bookmarks all
// share the same URL. Upon completion of this function, the state of
// |*bookmarks_with_same_url| is unspecified, because the implementation is free
// to modify its state desirable to avoid additional memory allocations on the
// calling site.
void AddStatsForBookmarksWithSameUrl(
    std::vector<const BookmarkNode*>* bookmarks_with_same_url,
    UrlLoadStats* stats) {
  if (bookmarks_with_same_url->size() <= 1)
    return;

  stats->duplicate_url_bookmark_count += bookmarks_with_same_url->size() - 1;

  // Sort only if there are 3 or more bookmarks. With exactly two (which is
  // believed to be a common case) the precise ordering is irrelevant for the
  // logic that follows.
  if (bookmarks_with_same_url->size() > 2) {
    std::sort(bookmarks_with_same_url->begin(), bookmarks_with_same_url->end(),
              [](const BookmarkNode* a, const BookmarkNode* b) {
                DCHECK_EQ(a->url(), b->url());
                if (a->GetTitle() != b->GetTitle())
                  return a->GetTitle() < b->GetTitle();
                return a->parent() < b->parent();
              });
  }

  size_t duplicate_title_count = 0;
  size_t duplicate_title_and_parent_count = 0;

  auto prev_i = bookmarks_with_same_url->begin();
  for (auto i = std::next(prev_i); i != bookmarks_with_same_url->end();
       ++i, ++prev_i) {
    DCHECK_EQ((*prev_i)->url(), (*i)->url());
    if ((*prev_i)->GetTitle() == (*i)->GetTitle()) {
      ++duplicate_title_count;
      if ((*prev_i)->parent() == (*i)->parent())
        ++duplicate_title_and_parent_count;
    }
  }

  DCHECK_LT(duplicate_title_count, bookmarks_with_same_url->size());
  DCHECK_LE(duplicate_title_and_parent_count, duplicate_title_count);

  stats->duplicate_url_and_title_bookmark_count += duplicate_title_count;
  stats->duplicate_url_and_title_and_parent_bookmark_count +=
      duplicate_title_and_parent_count;
}

void AddTimeStatsForBookmark(BookmarkNode* node, UrlLoadStats* stats) {
  stats->avg_num_days_since_added +=
      (base::Time::Now() - node->date_added()).InDays();

  if (node->date_last_used() != base::Time()) {
    stats->used_url_bookmark_count += 1;
  }
}

}  // namespace

UrlIndex::UrlIndex(std::unique_ptr<BookmarkNode> root)
    : root_(std::move(root)) {
  base::AutoLock url_lock(url_lock_);
  AddImpl(root_.get());
}

void UrlIndex::Add(BookmarkNode* parent,
                   size_t index,
                   std::unique_ptr<BookmarkNode> node) {
  base::AutoLock url_lock(url_lock_);
  AddImpl(parent->Add(std::move(node), index));
}

std::unique_ptr<BookmarkNode> UrlIndex::Remove(BookmarkNode* node,
                                               std::set<GURL>* removed_urls) {
  base::AutoLock url_lock(url_lock_);
  RemoveImpl(node, removed_urls);
  BookmarkNode* parent = node->parent();
  return parent->Remove(parent->GetIndexOf(node).value());
}

void UrlIndex::SetUrl(BookmarkNode* node, const GURL& url) {
  base::AutoLock url_lock(url_lock_);
  RemoveImpl(node, nullptr);
  node->set_url(url);
  AddImpl(node);
}

void UrlIndex::SetTitle(BookmarkNode* node, const std::u16string& title) {
  // Acquiring the lock is necessary to avoid races with
  // UrlIndex::GetBookmarks().
  base::AutoLock url_lock(url_lock_);
  node->SetTitle(title);
}

void UrlIndex::GetNodesWithIconUrl(const GURL& icon_url,
                                   std::set<const BookmarkNode*>* nodes) {
  base::AutoLock url_lock(url_lock_);
  for (const BookmarkNode* node : nodes_ordered_by_url_set_) {
    if (node->icon_url() && icon_url == *node->icon_url())
      nodes->insert(node);
  }
}

void UrlIndex::GetNodesByUrl(const GURL& url,
                             std::vector<const BookmarkNode*>* nodes) {
  base::AutoLock url_lock(url_lock_);
  BookmarkNode tmp_node(/*id=*/0, base::Uuid::GenerateRandomV4(), url);
  auto i = nodes_ordered_by_url_set_.find(&tmp_node);
  while (i != nodes_ordered_by_url_set_.end() && (*i)->url() == url) {
    nodes->push_back(*i);
    ++i;
  }
}

bool UrlIndex::HasBookmarks() const {
  base::AutoLock url_lock(url_lock_);
  return !nodes_ordered_by_url_set_.empty();
}

UrlLoadStats UrlIndex::ComputeStats() const {
  base::AutoLock url_lock(url_lock_);
  UrlLoadStats stats;
  stats.total_url_bookmark_count = nodes_ordered_by_url_set_.size();
  if (nodes_ordered_by_url_set_.begin() != nodes_ordered_by_url_set_.end()) {
    AddTimeStatsForBookmark(*nodes_ordered_by_url_set_.begin(), &stats);
  }

  if (stats.total_url_bookmark_count <= 1)
    return stats;

  std::vector<const BookmarkNode*> bookmarks_with_same_url;
  auto prev_i = nodes_ordered_by_url_set_.begin();
  for (auto i = std::next(prev_i); i != nodes_ordered_by_url_set_.end();
       ++i, ++prev_i) {
    // Handle duplicate URL stats.
    if ((*prev_i)->url() != (*i)->url()) {
      AddStatsForBookmarksWithSameUrl(&bookmarks_with_same_url, &stats);
      bookmarks_with_same_url.clear();
    }
    bookmarks_with_same_url.push_back(*i);

    AddTimeStatsForBookmark(*i, &stats);
  }

  stats.avg_num_days_since_added /= nodes_ordered_by_url_set_.size();
  AddStatsForBookmarksWithSameUrl(&bookmarks_with_same_url, &stats);
  return stats;
}

bool UrlIndex::IsBookmarked(const GURL& url) {
  base::AutoLock url_lock(url_lock_);
  return IsBookmarkedNoLock(url);
}

void UrlIndex::GetBookmarks(std::vector<UrlAndTitle>* bookmarks) {
  base::AutoLock url_lock(url_lock_);
  const GURL* last_url = nullptr;
  for (auto i = nodes_ordered_by_url_set_.begin();
       i != nodes_ordered_by_url_set_.end(); ++i) {
    const GURL* url = &((*i)->url());
    // Only add unique URLs.
    if (!last_url || *url != *last_url) {
      UrlAndTitle bookmark;
      bookmark.url = *url;
      bookmark.title = (*i)->GetTitle();
      bookmarks->push_back(bookmark);
    }
    last_url = url;
  }
}

UrlIndex::~UrlIndex() = default;

bool UrlIndex::IsBookmarkedNoLock(const GURL& url) {
  url_lock_.AssertAcquired();
  BookmarkNode tmp_node(/*id=*/0, base::Uuid::GenerateRandomV4(), url);
  return (nodes_ordered_by_url_set_.find(&tmp_node) !=
          nodes_ordered_by_url_set_.end());
}

void UrlIndex::AddImpl(BookmarkNode* node) {
  url_lock_.AssertAcquired();
  if (node->is_url())
    nodes_ordered_by_url_set_.insert(node);
  for (const auto& child : node->children())
    AddImpl(child.get());
}

void UrlIndex::RemoveImpl(BookmarkNode* node, std::set<GURL>* removed_urls) {
  url_lock_.AssertAcquired();
  if (node->is_url()) {
    auto i = nodes_ordered_by_url_set_.find(node);
    DCHECK(i != nodes_ordered_by_url_set_.end());
    // i points to the first node with the URL, advance until we find the
    // node we're removing.
    while (*i != node)
      ++i;
    nodes_ordered_by_url_set_.erase(i);
    if (removed_urls && !IsBookmarkedNoLock(node->url()))
      removed_urls->insert(node->url());
  }
  for (const auto& child : base::Reversed(node->children()))
    RemoveImpl(child.get(), removed_urls);
}

}  // namespace bookmarks
