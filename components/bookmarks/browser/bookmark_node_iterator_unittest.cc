// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node_iterator.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace bookmarks {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

std::unique_ptr<BookmarkNode> CreateFolder(int64_t id) {
  return std::make_unique<BookmarkNode>(id, base::Uuid::GenerateRandomV4(),
                                        GURL());
}

std::unique_ptr<BookmarkNode> CreateUrlNode(int64_t id) {
  return std::make_unique<BookmarkNode>(id, base::Uuid::GenerateRandomV4(),
                                        GURL("https://example.com"));
}

std::vector<int64_t> CollectIds(BookmarkNodeIterator& iterator) {
  std::vector<int64_t> ids;
  while (iterator.has_next()) {
    ids.push_back(iterator.Next()->id());
  }
  return ids;
}

TEST(BookmarkNodeIteratorTest, EmptyRoot) {
  auto root = CreateFolder(0);
  BookmarkNodeIterator iterator(root.get());
  EXPECT_FALSE(iterator.has_next());
  EXPECT_THAT(CollectIds(iterator), IsEmpty());
}

TEST(BookmarkNodeIteratorTest, PreOrderTraversal) {
  // 0 -> [1 -> [2, 3], 4 -> [], 5]
  auto root = CreateFolder(0);
  BookmarkNode* f1 = root->Add(CreateFolder(1));
  f1->Add(CreateUrlNode(2));
  f1->Add(CreateUrlNode(3));
  root->Add(CreateFolder(4));
  root->Add(CreateUrlNode(5));

  BookmarkNodeIterator iterator(root.get());
  EXPECT_THAT(CollectIds(iterator), ElementsAre(1, 2, 3, 4, 5));
}

TEST(BookmarkNodeIteratorTest, PrunesMatchingSubtrees) {
  // 0 -> [1 (pruned) -> [2], 3 -> [4 (pruned), 5], 6 (pruned)]
  auto root = CreateFolder(0);
  BookmarkNode* f1 = root->Add(CreateFolder(1));
  f1->Add(CreateUrlNode(2));
  BookmarkNode* f3 = root->Add(CreateFolder(3));
  f3->Add(CreateUrlNode(4));
  f3->Add(CreateUrlNode(5));
  root->Add(CreateUrlNode(6));

  BookmarkNodeIterator iterator(
      root.get(), base::BindRepeating([](const BookmarkNode* node) {
        return node->id() == 1 || node->id() == 4 || node->id() == 6;
      }));
  EXPECT_THAT(CollectIds(iterator), ElementsAre(3, 5));
}

TEST(BookmarkNodeIteratorTest, PrunesAllChildren) {
  auto root = CreateFolder(0);
  root->Add(CreateUrlNode(1));
  root->Add(CreateFolder(2));

  BookmarkNodeIterator iterator(
      root.get(),
      base::BindRepeating([](const BookmarkNode*) { return true; }));
  EXPECT_FALSE(iterator.has_next());
}

}  // namespace
}  // namespace bookmarks
