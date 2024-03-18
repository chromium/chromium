// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_LOAD_DETAILS_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_LOAD_DETAILS_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/uuid_index.h"

namespace base {
class TimeTicks;
}

namespace bookmarks {

class BookmarkPermanentNode;
class TitledUrlIndex;
class UrlIndex;

// BookmarkLoadDetails represents the outcome of loading and parsing the JSON
// file containing bookmarks. It is produced by ModelLoader in the backend task
// runner, including the generation of indices, and posted to the UI thread to
// finalize the loading of BookmarkModel.
class BookmarkLoadDetails {
 public:
  BookmarkLoadDetails();
  ~BookmarkLoadDetails();

  BookmarkLoadDetails(const BookmarkLoadDetails&) = delete;
  BookmarkLoadDetails& operator=(const BookmarkLoadDetails&) = delete;

  // Local-or-syncable permanent nodes (never null).
  BookmarkPermanentNode* bb_node() { return bb_node_; }
  BookmarkPermanentNode* mobile_folder_node() { return mobile_folder_node_; }
  BookmarkPermanentNode* other_folder_node() { return other_folder_node_; }

  // Account permanent nodes (null unless `AddAccountPermanentNodes()` is
  // called).
  BookmarkPermanentNode* account_bb_node() { return account_bb_node_; }
  BookmarkPermanentNode* account_mobile_folder_node() {
    return account_mobile_folder_node_;
  }
  BookmarkPermanentNode* account_other_folder_node() {
    return account_other_folder_node_;
  }

  std::unique_ptr<TitledUrlIndex> owned_titled_url_index() {
    return std::move(titled_url_index_);
  }

  UuidIndex owned_local_or_syncable_uuid_index() {
    return std::move(local_or_syncable_uuid_index_);
  }

  UuidIndex owned_account_uuid_index() {
    return std::move(account_uuid_index_);
  }

  // Max id of the nodes.
  void set_max_id(int64_t max_id) { max_id_ = max_id; }
  int64_t max_id() const { return max_id_; }

  // The required-recovery bit represents whether the on-disk state was corrupt
  // and had to be recovered. Scenarios include ID or UUID collisions and
  // checksum mismatches.
  void set_required_recovery(bool value) { required_recovery_ = value; }
  bool required_recovery() const { return required_recovery_; }

  // Whether ids were reassigned. IDs are reassigned during decoding if the
  // checksum of the file doesn't match, some IDs are missing or not
  // unique. Basically, if the user modified the bookmarks directly we'll
  // reassign the ids to ensure they are unique.
  void set_ids_reassigned(bool value) { ids_reassigned_ = value; }
  bool ids_reassigned() const { return ids_reassigned_; }

  // If IDs are reassigned during decoding, this represents the mapping from old
  // (i.e. on-disk) ID to the newly-assigned ones.
  const std::multimap<int64_t, int64_t>&
  local_or_syncable_reassigned_ids_per_old_id() const {
    return local_or_syncable_reassigned_ids_per_old_id_;
  }
  void set_local_or_syncable_reassigned_ids_per_old_id(
      std::multimap<int64_t, int64_t> value) {
    local_or_syncable_reassigned_ids_per_old_id_ = std::move(value);
  }

  // Returns the string blob representing the sync metadata in the json file.
  // The string blob is set during decode time upon the call to Bookmark::Load.
  void set_local_or_syncable_sync_metadata_str(std::string sync_metadata_str) {
    local_or_syncable_sync_metadata_str_ = std::move(sync_metadata_str);
  }

  const std::string& local_or_syncable_sync_metadata_str() const {
    return local_or_syncable_sync_metadata_str_;
  }

  // Same as above but for account bookmarks.
  void set_account_sync_metadata_str(std::string sync_metadata_str) {
    account_sync_metadata_str_ = std::move(sync_metadata_str);
  }
  const std::string& account_sync_metadata_str() const {
    return account_sync_metadata_str_;
  }

  // Adds account bookmarks. May be called at most once.
  void AddAccountPermanentNodes(
      std::unique_ptr<BookmarkPermanentNode> account_bb_node,
      std::unique_ptr<BookmarkPermanentNode> account_other_folder_node,
      std::unique_ptr<BookmarkPermanentNode> account_mobile_folder_node);

  // Assigns node IDs for local-or-syncable permanent nodes if not previously
  // assigned/decoded.
  void PopulateNodeIdsForLocalOrSyncablePermanentNodes();

  // Adds managed nodes. May be called at most once.
  // PopulateNodeIdsForLocalOrSyncablePermanentNodes() must have been invoked
  // before this function.
  void AddManagedNode(std::unique_ptr<BookmarkPermanentNode> managed_node);

  void CreateIndices();

  void ResetPermanentNodePointers();

  const scoped_refptr<UrlIndex>& url_index() { return url_index_; }
  const UrlIndex* url_index() const { return url_index_.get(); }

  base::TimeTicks load_start() { return load_start_; }

  const BookmarkNode* RootNodeForTest() const;

 private:
  // Adds node to the various indices, recursing through all children as well.
  void AddNodeToIndexRecursive(BookmarkNode* node, UuidIndex& uuid_index);

  std::unique_ptr<BookmarkNode> root_node_;
  std::unique_ptr<TitledUrlIndex> titled_url_index_;
  UuidIndex local_or_syncable_uuid_index_;
  UuidIndex account_uuid_index_;
  int64_t max_id_ = 1;
  bool ids_reassigned_ = false;
  std::multimap<int64_t, int64_t> local_or_syncable_reassigned_ids_per_old_id_;
  bool required_recovery_ = false;
  scoped_refptr<UrlIndex> url_index_;
  raw_ptr<BookmarkPermanentNode> bb_node_;
  raw_ptr<BookmarkPermanentNode> other_folder_node_;
  raw_ptr<BookmarkPermanentNode> mobile_folder_node_;
  raw_ptr<BookmarkPermanentNode> account_bb_node_;
  raw_ptr<BookmarkPermanentNode> account_other_folder_node_;
  raw_ptr<BookmarkPermanentNode> account_mobile_folder_node_;
  bool has_managed_node_ = false;
  // String blob representing the sync metadata stored in the json file, one
  // per storage type (local-or-syncable or account bookmarks). In normal
  // circumstances, only one of them may be non-empty.
  std::string local_or_syncable_sync_metadata_str_;
  std::string account_sync_metadata_str_;
  base::TimeTicks load_start_;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_LOAD_DETAILS_H_
