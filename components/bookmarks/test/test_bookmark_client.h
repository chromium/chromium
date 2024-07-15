// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_
#define COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/bookmark_client.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace bookmarks {

class BookmarkModel;

class TestBookmarkClient : public BookmarkClient {
 public:
  TestBookmarkClient();

  TestBookmarkClient(const TestBookmarkClient&) = delete;
  TestBookmarkClient& operator=(const TestBookmarkClient&) = delete;

  ~TestBookmarkClient() override;

  // Returns a new BookmarkModel using a TestBookmarkClient.
  static std::unique_ptr<BookmarkModel> CreateModel();

  // Returns a new BookmarkModel using `client`.
  static std::unique_ptr<BookmarkModel> CreateModelWithClient(
      std::unique_ptr<TestBookmarkClient> client);

  // Causes the the next call to CreateModel() or GetLoadManagedNodeCallback()
  // to return a node representing managed bookmarks. The raw pointer of this
  // node is returned for convenience.
  BookmarkPermanentNode* EnableManagedNode();

  // Returns true if `node` is the `managed_node_`.
  bool IsManagedNodeRoot(const BookmarkNode* node);

  // Mimics the completion of a previously-triggered GetFaviconImageForPageURL()
  // call for `page_url`, usually invoked by BookmarkModel. Returns false if no
  // such a call is pending completion. The completion returns a favicon with
  // URL `icon_url` and a single-color 16x16 image using `color`.
  bool SimulateFaviconLoaded(const GURL& page_url,
                             const GURL& icon_url,
                             const gfx::Image& color);

  // Mimics the completion of a previously-triggered GetFaviconImageForPageURL()
  // call for `page_url`, usually invoked by BookmarkModel. Returns false if no
  // such a call is pending completion. The completion returns an empty image
  // for the favicon.
  bool SimulateEmptyFaviconLoaded(const GURL& page_url);

  // Returns true if there is at least one active favicon loading task.
  bool HasFaviconLoadTasks() const;

  // Sets the value returned by
  // `IsSyncFeatureEnabledIncludingBookmarks()`.
  void SetIsSyncFeatureEnabledIncludingBookmarks(bool value);

  // Returns sync metadata for account bookmarks,  received via
  // DecodeAccountBookmarkSyncMetadata() or modified via
  // SetAccountBookmarkSyncMetadataAndScheduleWrite().
  const std::string& account_bookmark_sync_metadata() const {
    return account_bookmark_sync_metadata_;
  }

  void SetAccountBookmarkSyncMetadataAndScheduleWrite(
      const std::string& account_bookmark_sync_metadata);

  // BookmarkClient:
  LoadManagedNodeCallback GetLoadManagedNodeCallback() override;
  bool IsSyncFeatureEnabledIncludingBookmarks() override;
  bool CanSetPermanentNodeTitle(const BookmarkNode* permanent_node) override;
  bool IsNodeManaged(const BookmarkNode* node) override;
  std::string EncodeLocalOrSyncableBookmarkSyncMetadata() override;
  std::string EncodeAccountBookmarkSyncMetadata() override;
  void DecodeLocalOrSyncableBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override;
  void DecodeAccountBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override;
  base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) override;
  void OnBookmarkNodeRemovedUndoable(
      const BookmarkNode* parent,
      size_t index,
      std::unique_ptr<BookmarkNode> node) override;

 private:
  // Helpers for GetLoadManagedNodeCallback().
  static std::unique_ptr<BookmarkPermanentNode> LoadManagedNode(
      std::unique_ptr<BookmarkPermanentNode> managed_node,
      int64_t* next_id);

  // managed_node_ exists only until GetLoadManagedNodeCallback gets called, but
  // unowned_managed_node_ stays around after that.
  std::unique_ptr<BookmarkPermanentNode> managed_node_;
  raw_ptr<BookmarkPermanentNode, DanglingUntriaged> unowned_managed_node_ =
      nullptr;

  base::CancelableTaskTracker::TaskId next_task_id_ = 1;
  std::map<GURL, std::list<favicon_base::FaviconImageCallback>>
      requests_per_page_url_;

  bool is_sync_feature_enabled_including_bookmarks_for_uma = false;

  std::string account_bookmark_sync_metadata_;
  base::RepeatingClosure account_bookmark_sync_metadata_save_closure_ =
      base::DoNothing();
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_
