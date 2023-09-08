// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_LOAD_DETAILS_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_LOAD_DETAILS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/uuid_index.h"

namespace base {
class TimeTicks;
}

namespace bookmarks {

class BookmarkPermanentNode;
class TitledUrlIndex;
class UrlIndex;

// BookmarkLoadDetails is used by BookmarkStorage when loading bookmarks.
// BookmarkModel creates a BookmarkLoadDetails and passes it (including
// ownership) to BookmarkStorage. BookmarkStorage loads the bookmarks (and
// index) in the background thread, then calls back to the BookmarkModel (on
// the main thread) when loading is done, passing ownership back to the
// BookmarkModel. While loading BookmarkModel does not maintain references to
// the contents of the BookmarkLoadDetails, this ensures we don't have any
// threading problems.
class BookmarkLoadDetails {
 public:
  explicit BookmarkLoadDetails(BookmarkClient* client);
  ~BookmarkLoadDetails();

  BookmarkLoadDetails(const BookmarkLoadDetails&) = delete;
  BookmarkLoadDetails& operator=(const BookmarkLoadDetails&) = delete;

  // Loads the managed node and adds it to |root_|.
  void LoadManagedNode();

  BookmarkNode* root_node() { return root_node_ptr_; }
  BookmarkPermanentNode* bb_node() { return bb_node_; }
  BookmarkPermanentNode* mobile_folder_node() { return mobile_folder_node_; }
  BookmarkPermanentNode* other_folder_node() { return other_folder_node_; }

  std::unique_ptr<TitledUrlIndex> owned_titled_url_index() {
    return std::move(titled_url_index_);
  }

  UuidIndex owned_uuid_index() { return std::move(uuid_index_); }

  const BookmarkNode::MetaInfoMap& model_meta_info_map() const {
    return model_meta_info_map_;
  }
  void set_model_meta_info_map(const BookmarkNode::MetaInfoMap& meta_info_map) {
    model_meta_info_map_ = meta_info_map;
  }
  const BookmarkNode::MetaInfoMap& model_unsynced_meta_info_map() const {
    return model_unsynced_meta_info_map_;
  }
  void set_model_unsynced_meta_info_map(
      const BookmarkNode::MetaInfoMap& model_unsynced_meta_info_map) {
    model_unsynced_meta_info_map_ = model_unsynced_meta_info_map;
  }

  // Max id of the nodes.
  void set_max_id(int64_t max_id) { max_id_ = max_id; }
  int64_t max_id() const { return max_id_; }

  // Computed checksum.
  void set_computed_checksum(const std::string& value) {
    computed_checksum_ = value;
  }
  const std::string& computed_checksum() const { return computed_checksum_; }

  // Stored checksum.
  void set_stored_checksum(const std::string& value) {
    stored_checksum_ = value;
  }
  const std::string& stored_checksum() const { return stored_checksum_; }

  // Whether ids were reassigned. IDs are reassigned during decoding if the
  // checksum of the file doesn't match, some IDs are missing or not
  // unique. Basically, if the user modified the bookmarks directly we'll
  // reassign the ids to ensure they are unique.
  void set_ids_reassigned(bool value) { ids_reassigned_ = value; }
  bool ids_reassigned() const { return ids_reassigned_; }

  // Whether new UUIDs were assigned to Bookmarks that lacked them.
  void set_uuids_reassigned(bool value) { uuids_reassigned_ = value; }
  bool uuids_reassigned() const { return uuids_reassigned_; }

  // Returns the string blob representing the sync metadata in the json file.
  // The string blob is set during decode time upon the call to Bookmark::Load.
  void set_sync_metadata_str(std::string sync_metadata_str) {
    sync_metadata_str_ = std::move(sync_metadata_str);
  }
  const std::string& sync_metadata_str() const { return sync_metadata_str_; }

  void CreateIndices();

  const scoped_refptr<UrlIndex>& url_index() { return url_index_; }

  base::TimeTicks load_start() { return load_start_; }

 private:
  // Adds node to the various indices, recursing through all children as well.
  void AddNodeToIndexRecursive(BookmarkNode* node);

  std::unique_ptr<BookmarkNode> root_node_;
  raw_ptr<BookmarkNode, DanglingUntriaged> root_node_ptr_;
  raw_ptr<BookmarkPermanentNode, DanglingUntriaged> bb_node_ = nullptr;
  raw_ptr<BookmarkPermanentNode, DanglingUntriaged> other_folder_node_ =
      nullptr;
  raw_ptr<BookmarkPermanentNode, DanglingUntriaged> mobile_folder_node_ =
      nullptr;
  LoadManagedNodeCallback load_managed_node_callback_;
  std::unique_ptr<TitledUrlIndex> titled_url_index_;
  UuidIndex uuid_index_;
  BookmarkNode::MetaInfoMap model_meta_info_map_;
  BookmarkNode::MetaInfoMap model_unsynced_meta_info_map_;
  int64_t max_id_ = 1;
  std::string computed_checksum_;
  std::string stored_checksum_;
  bool ids_reassigned_ = false;
  bool uuids_reassigned_ = false;
  scoped_refptr<UrlIndex> url_index_;
  // A string blob represetning the sync metadata stored in the json file.
  std::string sync_metadata_str_;
  base::TimeTicks load_start_;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_LOAD_DETAILS_H_
