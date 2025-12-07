// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/test_node_builders.h"

#include <utility>

#include "base/check.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace sync_bookmarks::test {

UrlBuilder::UrlBuilder(const std::u16string& title, const GURL& url)
    : title_(title), url_(url) {}
UrlBuilder::UrlBuilder(const UrlBuilder&) = default;
UrlBuilder::~UrlBuilder() = default;

UrlBuilder& UrlBuilder::SetUuid(const base::Uuid& uuid) {
  uuid_ = uuid;
  return *this;
}

void UrlBuilder::Build(bookmarks::BookmarkModel* model,
                       const bookmarks::BookmarkNode* parent) const {
  model->AddURL(parent, parent->children().size(), title_, url_,
                /*meta_info=*/nullptr,
                /*creation_time=*/std::nullopt, uuid_);
}

void FolderBuilder::AddChildTo(bookmarks::BookmarkModel* model,
                               const bookmarks::BookmarkNode* parent,
                               const FolderOrUrl& folder_or_url) {
  if (std::holds_alternative<UrlBuilder>(folder_or_url)) {
    std::get<UrlBuilder>(folder_or_url).Build(model, parent);
  } else {
    CHECK(std::holds_alternative<FolderBuilder>(folder_or_url));
    std::get<FolderBuilder>(folder_or_url).Build(model, parent);
  }
}

void FolderBuilder::AddChildrenTo(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* parent,
                                  const std::vector<FolderOrUrl>& children) {
  for (const FolderOrUrl& folder_or_url : children) {
    AddChildTo(model, parent, folder_or_url);
  }
}

FolderBuilder::FolderBuilder(const std::u16string& title) : title_(title) {}
FolderBuilder::FolderBuilder(const FolderBuilder&) = default;
FolderBuilder::~FolderBuilder() = default;

FolderBuilder& FolderBuilder::SetChildren(std::vector<FolderOrUrl> children) {
  children_ = std::move(children);
  return *this;
}

FolderBuilder& FolderBuilder::SetUuid(const base::Uuid& uuid) {
  uuid_ = uuid;
  return *this;
}

void FolderBuilder::Build(bookmarks::BookmarkModel* model,
                          const bookmarks::BookmarkNode* parent) const {
  const bookmarks::BookmarkNode* folder = model->AddFolder(
      parent, parent->children().size(), title_,
      /*meta_info=*/nullptr, /*creation_time=*/std::nullopt, uuid_);
  AddChildrenTo(model, folder, children_);
}

}  // namespace sync_bookmarks::test
