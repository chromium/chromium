// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_CORE_BOOKMARK_MODEL_H_
#define COMPONENTS_BOOKMARKS_BROWSER_CORE_BOOKMARK_MODEL_H_

#include <string>
#include <string_view>
#include <vector>

#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace query_parser {
enum class MatchingAlgorithm;
}  // namespace query_parser

namespace bookmarks {

struct TitledUrlMatch;
struct UrlAndTitle;

// A minimal subset of BookmarkModel API, intended to allow migrating the ios/
// codebase from having two BookmarkModel instances to having one. One
// important property of all APIs in this class is that they have obvious
// semantics if an instance represented a merged view of two underlying
// BookmarkModel instances. Beyond that, the precise APIs included here is
// arbitrary and influenced by actual need in code.
// TODO(crbug.com/326185948): Remove this base class one the migration is
// complete.
class CoreBookmarkModel : public KeyedService {
 public:
  CoreBookmarkModel();
  CoreBookmarkModel(const CoreBookmarkModel&) = delete;
  ~CoreBookmarkModel() override;

  CoreBookmarkModel& operator=(const CoreBookmarkModel&) = delete;

  // Returns true if the model finished loading.
  virtual bool loaded() const = 0;

  // Returns true if the specified URL is bookmarked.
  virtual bool IsBookmarked(const GURL& url) const = 0;

  // Returns the set of nodes with the `url`.
  virtual size_t GetNodeCountByURL(const GURL& url) const = 0;

  // Returns the titles of the nodes with the specified URL.
  virtual std::vector<std::u16string_view> GetNodeTitlesByURL(
      const GURL& url) const = 0;

  // Return the set of bookmarked urls and their titles. This returns the unique
  // set of URLs. For example, if two bookmarks reference the same URL only one
  // entry is added not matter the titles are same or not.
  [[nodiscard]] virtual std::vector<UrlAndTitle> GetUniqueUrls() const = 0;

  // Returns up bookmarks containing each term from `query` in either the title,
  // URL, or the titles of ancestors. `matching_algorithm` determines the
  // algorithm used by QueryParser internally to parse `query`. `max_count_hint`
  // is used as approximate maximum count, but implementations are allowed to
  // exceed this maximum.
  [[nodiscard]] virtual std::vector<TitledUrlMatch> GetBookmarksMatching(
      const std::u16string& query,
      size_t max_count_hint,
      query_parser::MatchingAlgorithm matching_algorithm) const = 0;

  // Removes all the non-permanent bookmark nodes that are editable by the user.
  virtual void RemoveAllUserBookmarks() = 0;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_CORE_BOOKMARK_MODEL_H_
