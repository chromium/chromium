// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/url_index.h"

#include <iterator>

#include "base/containers/adapters.h"
#include "base/guid.h"
#include "components/bookmarks/browser/url_and_title.h"

namespace bookmarks {

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
  return parent->Remove(size_t{parent->GetIndexOf(node)});
}

void UrlIndex::SetUrl(BookmarkNode* node, const GURL& url) {
  base::AutoLock url_lock(url_lock_);
  RemoveImpl(node, nullptr);
  node->set_url(url);
  AddImpl(node);
}

void UrlIndex::SetTitle(BookmarkNode* node, const base::string16& title) {
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
  BookmarkNode tmp_node(/*id=*/0, base::GenerateGUID(), url);
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

UrlIndex::Stats UrlIndex::ComputeStats() const {
  base::AutoLock url_lock(url_lock_);
  UrlIndex::Stats stats;
  stats.total_url_bookmark_count = nodes_ordered_by_url_set_.size();
  if (stats.total_url_bookmark_count > 1) {
    auto prev_i = nodes_ordered_by_url_set_.begin();
    for (auto i = std::next(prev_i); i != nodes_ordered_by_url_set_.end();
         ++i, ++prev_i) {
      if ((*prev_i)->url() == (*i)->url())
        ++stats.duplicate_url_bookmark_count;
    }
  }
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
  BookmarkNode tmp_node(/*id=*/0, base::GenerateGUID(), url);
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
