// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_CONTENT_STORE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_CONTENT_STORE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/bind.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"

namespace dom_distiller {

// The maximum number of items to keep in the cache before deleting some.
const int kDefaultMaxNumCachedEntries = 32;

// This is a simple interface for saving and loading of distilled content for an
// ArticleEntry.
class DistilledContentStore {
 public:
  typedef base::Callback<void(bool /* success */,
                              std::unique_ptr<DistilledArticleProto>)>
      LoadCallback;
  typedef base::Callback<void(bool /* success */)> SaveCallback;

  virtual void SaveContent(const ArticleEntry& entry,
                           const DistilledArticleProto& proto,
                           SaveCallback callback) = 0;
  virtual void LoadContent(const ArticleEntry& entry,
                           LoadCallback callback) = 0;

  DistilledContentStore() {}
  virtual ~DistilledContentStore() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DistilledContentStore);
};

// This content store keeps up to |max_num_entries| of the last accessed items
// in its cache. Both loading and saving content is counted as access.
// Lookup can be done based on entry ID or URL.
class InMemoryContentStore : public DistilledContentStore {
 public:
  explicit InMemoryContentStore(const int max_num_entries);
  ~InMemoryContentStore() override;

  // DistilledContentStore implementation
  void SaveContent(const ArticleEntry& entry,
                   const DistilledArticleProto& proto,
                   SaveCallback callback) override;
  void LoadContent(const ArticleEntry& entry, LoadCallback callback) override;

  // Synchronously saves the content.
  void InjectContent(const ArticleEntry& entry,
                     const DistilledArticleProto& proto);

 private:
  // The CacheDeletor gets called when anything is removed from the ContentMap.
  class CacheDeletor {
   public:
    explicit CacheDeletor(InMemoryContentStore* store);
    ~CacheDeletor();
    void operator()(DistilledArticleProto* proto);

   private:
    InMemoryContentStore* store_;
  };

  void AddUrlToIdMapping(const ArticleEntry& entry,
                         const DistilledArticleProto& proto);

  void EraseUrlToIdMapping(const DistilledArticleProto& proto);

  typedef base::MRUCache<std::string,
                         std::unique_ptr<DistilledArticleProto, CacheDeletor>>
      ContentMap;
  typedef std::unordered_map<std::string, std::string> UrlMap;

  ContentMap cache_;
  UrlMap url_to_id_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_CONTENT_CACHE_H_
