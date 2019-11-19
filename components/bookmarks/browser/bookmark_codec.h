// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_CODEC_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_CODEC_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/hash/md5.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace bookmarks {

class BookmarkModel;

// BookmarkCodec is responsible for encoding and decoding the BookmarkModel
// into JSON values. The encoded values are written to disk via the
// BookmarkStorage.
class BookmarkCodec {
 public:
  // Creates an instance of the codec. During decoding, if the IDs in the file
  // are not unique, we will reassign IDs to make them unique. There are no
  // guarantees on how the IDs are reassigned or about doing minimal
  // reassignments to achieve uniqueness.
  BookmarkCodec();
  ~BookmarkCodec();

  // Encodes the model to a JSON value. It's up to the caller to delete the
  // returned object. This is invoked to encode the contents of the bookmark bar
  // model and is currently a convenience to invoking Encode that takes the
  // bookmark bar node and other folder node.
  std::unique_ptr<base::Value> Encode(BookmarkModel* model,
                                      const std::string& sync_metadata_str);

  // Encodes the bookmark bar and other folders returning the JSON value.
  std::unique_ptr<base::Value> Encode(
      const BookmarkNode* bookmark_bar_node,
      const BookmarkNode* other_folder_node,
      const BookmarkNode* mobile_folder_node,
      const BookmarkNode::MetaInfoMap* model_meta_info_map,
      int64_t sync_transaction_version,
      const std::string& sync_metadata_str);

  // Decodes the previously encoded value to the specified nodes as well as
  // setting |max_node_id| to the greatest node id. Returns true on success,
  // false otherwise. If there is an error (such as unexpected version) all
  // children are removed from the bookmark bar and other folder nodes. On exit
  // |max_node_id| is set to the max id of the nodes.
  bool Decode(const base::Value& value,
              BookmarkNode* bb_node,
              BookmarkNode* other_folder_node,
              BookmarkNode* mobile_folder_node,
              int64_t* max_node_id,
              std::string* sync_metadata_str);

  // Returns the checksum computed during last encoding/decoding call.
  const std::string& computed_checksum() const { return computed_checksum_; }

  // Returns the checksum that's stored in the file. After a call to Encode,
  // the computed and stored checksums are the same since the computed checksum
  // is stored to the file. After a call to decode, the computed checksum can
  // differ from the stored checksum if the file contents were changed by the
  // user.
  const std::string& stored_checksum() const { return stored_checksum_; }

  // Return meta info of bookmark model root.
  const BookmarkNode::MetaInfoMap& model_meta_info_map() const {
    return model_meta_info_map_;
  }

  // Return the sync transaction version of the bookmark model root.
  int64_t model_sync_transaction_version() const {
    return model_sync_transaction_version_;
  }

  // Returns whether the IDs were reassigned during decoding. Always returns
  // false after encoding.
  bool ids_reassigned() const { return ids_reassigned_; }

  // Returns whether the GUIDs were reassigned during decoding. Always returns
  // false after encoding.
  bool guids_reassigned() const { return guids_reassigned_; }

  // Names of the various keys written to the Value.
  static const char kRootsKey[];
  static const char kRootFolderNameKey[];
  static const char kOtherBookmarkFolderNameKey[];
  static const char kMobileBookmarkFolderNameKey[];
  static const char kVersionKey[];
  static const char kChecksumKey[];
  static const char kIdKey[];
  static const char kTypeKey[];
  static const char kNameKey[];
  static const char kGuidKey[];
  static const char kDateAddedKey[];
  static const char kURLKey[];
  static const char kDateModifiedKey[];
  static const char kChildrenKey[];
  static const char kMetaInfo[];
  static const char kSyncTransactionVersion[];
  // Allows the BookmarkClient to read and a write a string blob from the JSON
  // file. That string captures the bookmarks sync metadata.
  static const char kSyncMetadata[];

  // Possible values for kTypeKey.
  static const char kTypeURL[];
  static const char kTypeFolder[];

 private:
  // Encodes node and all its children into a Value object and returns it.
  std::unique_ptr<base::Value> EncodeNode(const BookmarkNode* node);

  // Encodes the given meta info into a Value object and returns it.
  std::unique_ptr<base::Value> EncodeMetaInfo(
      const BookmarkNode::MetaInfoMap& meta_info_map);

  // Helper to perform decoding.
  bool DecodeHelper(BookmarkNode* bb_node,
                    BookmarkNode* other_folder_node,
                    BookmarkNode* mobile_folder_node,
                    const base::Value& value,
                    std::string* sync_metadata_str);

  // Decodes the children of the specified node. Returns true on success.
  bool DecodeChildren(const base::ListValue& child_value_list,
                      BookmarkNode* parent);

  // Reassigns bookmark IDs for all nodes.
  void ReassignIDs(BookmarkNode* bb_node,
                   BookmarkNode* other_node,
                   BookmarkNode* mobile_node);

  // Helper to recursively reassign IDs.
  void ReassignIDsHelper(BookmarkNode* node);

  // Decodes the supplied node from the supplied value. Child nodes are
  // created appropriately by way of DecodeChildren. If node is NULL a new
  // node is created and added to parent (parent must then be non-NULL),
  // otherwise node is used.
  bool DecodeNode(const base::DictionaryValue& value,
                  BookmarkNode* parent,
                  BookmarkNode* node);

  // Decodes the meta info from the supplied value. If the meta info contains
  // a "sync.transaction_version" key, the value of that field will be stored
  // in the sync_transaction_version variable, then deleted. This is for
  // backward-compatibility reasons.
  // meta_info_map and sync_transaction_version must not be NULL.
  bool DecodeMetaInfo(const base::DictionaryValue& value,
                      BookmarkNode::MetaInfoMap* meta_info_map,
                      int64_t* sync_transaction_version);

  // Decodes the meta info from the supplied sub-node dictionary. The values
  // found will be inserted in meta_info_map with the given prefix added to the
  // start of their keys.
  void DecodeMetaInfoHelper(const base::DictionaryValue& dict,
                            const std::string& prefix,
                            BookmarkNode::MetaInfoMap* meta_info_map);

  // Updates the check-sum with the given string.
  void UpdateChecksum(const std::string& str);
  void UpdateChecksum(const base::string16& str);

  // Updates the check-sum with the given contents of URL/folder bookmark node.
  // NOTE: These functions take in individual properties of a bookmark node
  // instead of taking in a BookmarkNode for efficiency so that we don't convert
  // various data-types to UTF16 strings multiple times - once for serializing
  // and once for computing the check-sum.
  // The url parameter should be a valid UTF8 string.
  void UpdateChecksumWithUrlNode(const std::string& id,
                                 const base::string16& title,
                                 const std::string& url);
  void UpdateChecksumWithFolderNode(const std::string& id,
                                    const base::string16& title);

  // Initializes/Finalizes the checksum.
  void InitializeChecksum();
  void FinalizeChecksum();

  // Whether or not IDs were reassigned by the codec.
  bool ids_reassigned_;

  // Whether or not GUIDs were reassigned by the codec.
  bool guids_reassigned_;

  // Whether or not IDs are valid. This is initially true, but set to false
  // if an id is missing or not unique.
  bool ids_valid_;

  // Contains the id of each of the nodes found in the file. Used to determine
  // if we have duplicates.
  std::set<int64_t> ids_;

  // Contains the GUID of each of the nodes found in the file. Used to determine
  // if we have duplicates.
  std::set<std::string> guids_;

  // MD5 context used to compute MD5 hash of all bookmark data.
  base::MD5Context md5_context_;

  // Checksums.
  std::string computed_checksum_;
  std::string stored_checksum_;

  // Maximum ID assigned when decoding data.
  int64_t maximum_id_;

  // Meta info set on bookmark model root.
  BookmarkNode::MetaInfoMap model_meta_info_map_;

  // Sync transaction version set on bookmark model root.
  int64_t model_sync_transaction_version_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkCodec);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_CODEC_H_
