// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_STORAGE_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_STORAGE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/titled_url_index.h"

namespace base {
class SequencedTaskRunner;
}

namespace bookmarks {

class BookmarkClient;
class BookmarkModel;
class BookmarkNode;
class UrlIndex;

// A callback that generates a std::unique_ptr<BookmarkPermanentNode>, given a
// max ID to use. The max ID argument will be updated after if a new node has
// been created and assigned an ID.
using LoadManagedNodeCallback =
    base::OnceCallback<std::unique_ptr<BookmarkPermanentNode>(int64_t*)>;

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

  // Loads the managed node and adds it to |root_|. Returns true if the added
  // node has children.
  bool LoadManagedNode();

  BookmarkNode* root_node() { return root_node_ptr_; }
  BookmarkPermanentNode* bb_node() { return bb_node_; }
  BookmarkPermanentNode* mobile_folder_node() { return mobile_folder_node_; }
  BookmarkPermanentNode* other_folder_node() { return other_folder_node_; }

  TitledUrlIndex* index() { return index_.get(); }
  std::unique_ptr<TitledUrlIndex> owned_index() { return std::move(index_); }

  const BookmarkNode::MetaInfoMap& model_meta_info_map() const {
    return model_meta_info_map_;
  }
  void set_model_meta_info_map(const BookmarkNode::MetaInfoMap& meta_info_map) {
    model_meta_info_map_ = meta_info_map;
  }

  int64_t model_sync_transaction_version() const {
    return model_sync_transaction_version_;
  }
  void set_model_sync_transaction_version(int64_t sync_transaction_version) {
    model_sync_transaction_version_ = sync_transaction_version;
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

  // Whether new GUIDs were assigned to Bookmarks that lacked them.
  void set_guids_reassigned(bool value) { guids_reassigned_ = value; }
  bool guids_reassigned() const { return guids_reassigned_; }

  // Returns the string blob representing the sync metadata in the json file.
  // The string blob is set during decode time upon the call to Bookmark::Load.
  void set_sync_metadata_str(std::string sync_metadata_str) {
    sync_metadata_str_ = std::move(sync_metadata_str);
  }
  const std::string& sync_metadata_str() const { return sync_metadata_str_; }

  void CreateUrlIndex();
  UrlIndex* url_index() { return url_index_.get(); }

 private:
  // Creates one of the possible permanent nodes (bookmark bar node, other node
  // and mobile node) from |type|. The node is added to (and owned by) |root_|.
  BookmarkPermanentNode* CreatePermanentNode(BookmarkClient* client,
                                             BookmarkNode::Type type);

  std::unique_ptr<BookmarkNode> root_node_;
  BookmarkNode* root_node_ptr_;
  BookmarkPermanentNode* bb_node_ = nullptr;
  BookmarkPermanentNode* other_folder_node_ = nullptr;
  BookmarkPermanentNode* mobile_folder_node_ = nullptr;
  LoadManagedNodeCallback load_managed_node_callback_;
  std::unique_ptr<TitledUrlIndex> index_;
  BookmarkNode::MetaInfoMap model_meta_info_map_;
  int64_t model_sync_transaction_version_;
  int64_t max_id_ = 1;
  std::string computed_checksum_;
  std::string stored_checksum_;
  bool ids_reassigned_ = false;
  bool guids_reassigned_ = false;
  scoped_refptr<UrlIndex> url_index_;
  // A string blob represetning the sync metadata stored in the json file.
  std::string sync_metadata_str_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkLoadDetails);
};

// Loads the bookmarks. This is intended to be called on the background thread.
// Updates state in |details| based on the load. |emit_experimental_uma|
// determines whether a few newly introduced and experimental UMA metrics should
// be logged.
void LoadBookmarks(const base::FilePath& profile_path,
                   bool emit_experimental_uma,
                   BookmarkLoadDetails* details);

// BookmarkStorage handles reading/write the bookmark bar model. The
// BookmarkModel uses the BookmarkStorage to load bookmarks from disk, as well
// as notifying the BookmarkStorage every time the model changes.
//
// Internally BookmarkStorage uses BookmarkCodec to do the actual read/write.
class BookmarkStorage : public base::ImportantFileWriter::DataSerializer {
 public:
  // Creates a BookmarkStorage for the specified model. The data will be loaded
  // from and saved to a location derived from |profile_path|. The IO code will
  // be executed as a task in |sequenced_task_runner|.
  BookmarkStorage(BookmarkModel* model,
                  const base::FilePath& profile_path,
                  base::SequencedTaskRunner* sequenced_task_runner);
  ~BookmarkStorage() override;

  // Schedules saving the bookmark bar model to disk.
  void ScheduleSave();

  // Notification the bookmark bar model is going to be deleted. If there is
  // a pending save, it is saved immediately.
  void BookmarkModelDeleted();

  // ImportantFileWriter::DataSerializer implementation.
  bool SerializeData(std::string* output) override;

 private:
  // The state of the bookmark file backup. We lazily backup this file in order
  // to reduce disk writes until absolutely necessary. Will also leave the
  // backup unchanged if the browser starts & quits w/o changing bookmarks.
  enum BackupState {
    // No attempt has yet been made to backup the bookmarks file.
    BACKUP_NONE,
    // A request to backup the bookmarks file has been posted, but not yet
    // fulfilled.
    BACKUP_DISPATCHED,
    // The bookmarks file has been backed up (or at least attempted).
    BACKUP_ATTEMPTED
  };

  // Serializes the data and schedules save using ImportantFileWriter.
  // Returns true on successful serialization.
  bool SaveNow();

  // Callback from backend after creation of backup file.
  void OnBackupFinished();

  // The model. The model is NULL once BookmarkModelDeleted has been invoked.
  BookmarkModel* model_;

  // Helper to write bookmark data safely.
  base::ImportantFileWriter writer_;

  // The state of the backup file creation which is created lazily just before
  // the first scheduled save.
  BackupState backup_state_ = BACKUP_NONE;

  // Sequenced task runner where file I/O operations will be performed at.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  base::WeakPtrFactory<BookmarkStorage> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkStorage);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_STORAGE_H_
