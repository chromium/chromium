// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/url_index.h"

#include <iterator>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/common/url_load_stats.h"

namespace bookmarks {

namespace {

void AddTimeStatsForBookmark(BookmarkNode* node,
                             base::Time now,
                             UrlLoadStats* stats) {
  stats->avg_num_days_since_added += (now - node->date_added()).InDays();

  // It's possible that date_last_used hasn't been populated for this node.
  if (node->date_last_used() != base::Time()) {
    stats->used_url_bookmark_count += 1;
    int mru_days = std::max(0, (now - node->date_last_used()).InDays());
    stats->most_recently_used_bookmark_days =
        std::min<size_t>(mru_days, stats->most_recently_used_bookmark_days);
    stats->per_bookmark_num_days_since_used.push_back(mru_days);
  } else if (node->is_url()) {
    int mra_days = std::max(0, (now - node->date_added()).InDays());
    stats->per_bookmark_num_days_since_used.push_back(mra_days);
  }

  stats->most_recently_saved_bookmark_days =
      std::min<size_t>(std::max(0, (now - node->date_added()).InDays()),
                       stats->most_recently_saved_bookmark_days);

  stats->most_recently_saved_folder_days = std::min<size_t>(
      std::max(0, (now - node->parent()->date_added()).InDays()),
      stats->most_recently_saved_folder_days);
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
    if (node->icon_url() && icon_url == *node->icon_url()) {
      nodes->insert(node);
    }
  }
}

void UrlIndex::GetNodesByUrl(
    const GURL& url,
    std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>* nodes) {
  base::AutoLock url_lock(url_lock_);
  auto i = nodes_ordered_by_url_set_.find<GURL>(url);
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

  base::Time now = base::Time::Now();
  UrlLoadStats stats;
  if (!nodes_ordered_by_url_set_.empty()) {
    stats.total_url_bookmark_count = nodes_ordered_by_url_set_.size();
    for (BookmarkNode* node : nodes_ordered_by_url_set_) {
      AddTimeStatsForBookmark(node, now, &stats);
    }
    stats.avg_num_days_since_added /= nodes_ordered_by_url_set_.size();
  }
  return stats;
}

bool UrlIndex::IsBookmarked(const GURL& url) {
  base::AutoLock url_lock(url_lock_);
  return IsBookmarkedNoLock(url);
}

std::vector<UrlAndTitle> UrlIndex::GetUniqueUrls() {
  std::vector<UrlAndTitle> bookmarks;
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
      bookmarks.push_back(bookmark);
    }
    last_url = url;
  }
  return bookmarks;
}

UrlIndex::~UrlIndex() = default;

bool UrlIndex::IsBookmarkedNoLock(const GURL& url) {
  url_lock_.AssertAcquired();
  return (nodes_ordered_by_url_set_.find(url) !=
          nodes_ordered_by_url_set_.end());
}

void UrlIndex::AddImpl(BookmarkNode* node) {
  url_lock_.AssertAcquired();
  if (node->is_url()) {
    nodes_ordered_by_url_set_.insert(node);
  }
  for (const auto& child : node->children()) {
    AddImpl(child.get());
  }
}

void UrlIndex::RemoveImpl(BookmarkNode* node, std::set<GURL>* removed_urls) {
  url_lock_.AssertAcquired();
  if (node->is_url()) {
    auto i = nodes_ordered_by_url_set_.find(node);
    CHECK(i != nodes_ordered_by_url_set_.end(), base::NotFatalUntil::M130);
    // i points to the first node with the URL, advance until we find the
    // node we're removing.
    while (*i != node) {
      ++i;
    }
    nodes_ordered_by_url_set_.erase(i);
    if (removed_urls && !IsBookmarkedNoLock(node->url())) {
      removed_urls->insert(node->url());
    }
  }
  for (const auto& child : base::Reversed(node->children())) {
    RemoveImpl(child.get(), removed_urls);
  }
}

}  // namespace bookmarks
