// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distilled_content_store.h"
#include "base/task/single_thread_task_runner.h"

#include <memory>
#include <utility>

namespace dom_distiller {

InMemoryContentStore::InMemoryContentStore(const int max_num_entries)
    : cache_(max_num_entries) {}

InMemoryContentStore::~InMemoryContentStore() {
  // Clear the cache before destruction to ensure the CacheDeletor is not called
  // after InMemoryContentStore has been destroyed.
  cache_.Clear();
}

void InMemoryContentStore::SaveContent(
    const ArticleEntry& entry,
    const DistilledArticleProto& proto,
    InMemoryContentStore::SaveCallback callback) {
  InjectContent(entry, proto);
  if (!callback.is_null()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }
}

void InMemoryContentStore::LoadContent(
    const ArticleEntry& entry,
    InMemoryContentStore::LoadCallback callback) {
  if (callback.is_null())
    return;

  ContentMap::const_iterator it = cache_.Get(entry.entry_id);
  bool success = it != cache_.end();
  if (!success) {
    // Could not find article by entry ID, so try looking it up by URL.
    for (const GURL& page : entry.pages) {
      UrlMap::const_iterator url_it = url_to_id_.find(page.spec());
      if (url_it != url_to_id_.end()) {
        it = cache_.Get(url_it->second);
        success = it != cache_.end();
        if (success) {
          break;
        }
      }
    }
  }
  std::unique_ptr<DistilledArticleProto> distilled_article;
  if (success) {
    distilled_article = std::make_unique<DistilledArticleProto>(*it->second);
  } else {
    distilled_article = std::make_unique<DistilledArticleProto>();
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success,
                                std::move(distilled_article)));
}

void InMemoryContentStore::InjectContent(const ArticleEntry& entry,
                                         const DistilledArticleProto& proto) {
  cache_.Put(entry.entry_id,
             std::unique_ptr<DistilledArticleProto, CacheDeletor>(
                 new DistilledArticleProto(proto), CacheDeletor(this)));
  AddUrlToIdMapping(entry, proto);
}

void InMemoryContentStore::AddUrlToIdMapping(
    const ArticleEntry& entry,
    const DistilledArticleProto& proto) {
  for (int i = 0; i < proto.pages_size(); i++) {
    const DistilledPageProto& page = proto.pages(i);
    if (page.has_url()) {
      url_to_id_[page.url()] = entry.entry_id;
    }
  }
}

void InMemoryContentStore::EraseUrlToIdMapping(
    const DistilledArticleProto& proto) {
  for (int i = 0; i < proto.pages_size(); i++) {
    const DistilledPageProto& page = proto.pages(i);
    if (page.has_url()) {
      url_to_id_.erase(page.url());
    }
  }
}

InMemoryContentStore::CacheDeletor::CacheDeletor(InMemoryContentStore* store)
    : store_(store) {}

InMemoryContentStore::CacheDeletor::~CacheDeletor() = default;

void InMemoryContentStore::CacheDeletor::operator()(
    DistilledArticleProto* proto) {
  // When InMemoryContentStore is deleted, the |store_| pointer becomes invalid,
  // but since the ContentMap is cleared in the InMemoryContentStore destructor,
  // this should never be called after the destructor.
  store_->EraseUrlToIdMapping(*proto);
  delete proto;
}

}  // namespace dom_distiller
