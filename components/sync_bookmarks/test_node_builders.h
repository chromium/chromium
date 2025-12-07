// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_TEST_NODE_BUILDERS_H_
#define COMPONENTS_SYNC_BOOKMARKS_TEST_NODE_BUILDERS_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/uuid.h"
#include "url/gurl.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace sync_bookmarks::test {

class FolderBuilder;

// Test class to build bookmark URLs conveniently and compactly in tests.
class UrlBuilder {
 public:
  UrlBuilder(const std::u16string& title, const GURL& url);
  UrlBuilder(const UrlBuilder&);
  ~UrlBuilder();

  UrlBuilder& SetUuid(const base::Uuid& uuid);

  void Build(bookmarks::BookmarkModel* model,
             const bookmarks::BookmarkNode* parent) const;

 private:
  const std::u16string title_;
  const GURL url_;
  std::optional<base::Uuid> uuid_;
};

// Test class to build bookmark folders conveniently and compactly in tests.
class FolderBuilder {
 public:
  using FolderOrUrl = std::variant<FolderBuilder, UrlBuilder>;

  static void AddChildTo(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         const FolderOrUrl& folder_or_url);

  static void AddChildrenTo(bookmarks::BookmarkModel* model,
                            const bookmarks::BookmarkNode* parent,
                            const std::vector<FolderOrUrl>& children);

  explicit FolderBuilder(const std::u16string& title);
  FolderBuilder(const FolderBuilder&);
  ~FolderBuilder();

  FolderBuilder& SetChildren(std::vector<FolderOrUrl> children);
  FolderBuilder& SetUuid(const base::Uuid& uuid);

  void Build(bookmarks::BookmarkModel* model,
             const bookmarks::BookmarkNode* parent) const;

 private:
  const std::u16string title_;
  std::vector<FolderOrUrl> children_;
  std::optional<base::Uuid> uuid_;
};

}  // namespace sync_bookmarks::test

#endif  // COMPONENTS_SYNC_BOOKMARKS_TEST_NODE_BUILDERS_H_
