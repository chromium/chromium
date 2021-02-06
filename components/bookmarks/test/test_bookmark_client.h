// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_
#define COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "components/bookmarks/browser/bookmark_client.h"

namespace bookmarks {

class BookmarkModel;

class TestBookmarkClient : public BookmarkClient {
 public:
  TestBookmarkClient();
  ~TestBookmarkClient() override;

  // Returns a new BookmarkModel using a TestBookmarkClient.
  static std::unique_ptr<BookmarkModel> CreateModel();

  // Returns a new BookmarkModel using |client|.
  static std::unique_ptr<BookmarkModel> CreateModelWithClient(
      std::unique_ptr<BookmarkClient> client);

  // Causes the the next call to CreateModel() or GetLoadManagedNodeCallback()
  // to return a node representing managed bookmarks. The raw pointer of this
  // node is returned for convenience.
  BookmarkPermanentNode* EnableManagedNode();

  // Returns true if |node| is the |managed_node_|.
  bool IsManagedNodeRoot(const BookmarkNode* node);

  // Returns true if |node| belongs to the tree of the |managed_node_|.
  bool IsAManagedNode(const BookmarkNode* node);

 private:
  // BookmarkClient:
  bool IsPermanentNodeVisibleWhenEmpty(BookmarkNode::Type type) override;
  void RecordAction(const base::UserMetricsAction& action) override;
  LoadManagedNodeCallback GetLoadManagedNodeCallback() override;
  bool CanSetPermanentNodeTitle(const BookmarkNode* permanent_node) override;
  bool CanSyncNode(const BookmarkNode* node) override;
  bool CanBeEditedByUser(const BookmarkNode* node) override;
  std::string EncodeBookmarkSyncMetadata() override;
  void DecodeBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override;

  // Helpers for GetLoadManagedNodeCallback().
  static std::unique_ptr<BookmarkPermanentNode> LoadManagedNode(
      std::unique_ptr<BookmarkPermanentNode> managed_node,
      int64_t* next_id);

  // managed_node_ exists only until GetLoadManagedNodeCallback gets called, but
  // unowned_managed_node_ stays around after that.
  std::unique_ptr<BookmarkPermanentNode> managed_node_;
  BookmarkPermanentNode* unowned_managed_node_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkClient);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_
