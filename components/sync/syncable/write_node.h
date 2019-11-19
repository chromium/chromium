// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_WRITE_NODE_H_
#define COMPONENTS_SYNC_SYNCABLE_WRITE_NODE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/sync/base/model_type.h"
#include "components/sync/syncable/base_node.h"

namespace sync_pb {
class BookmarkSpecifics;
class EntitySpecifics;
class NigoriSpecifics;
class PasswordSpecificsData;
class TypedUrlSpecifics;
}

namespace syncer {

class WriteTransaction;

namespace syncable {
class Id;
class Entry;
class MutableEntry;
}

// WriteNode extends BaseNode to add mutation, and wraps
// syncable::MutableEntry. A WriteTransaction is needed to create a WriteNode.
class WriteNode : public BaseNode {
 public:
  enum InitUniqueByCreationResult {
    INIT_SUCCESS,
    // The tag passed into this method was empty.
    INIT_FAILED_EMPTY_TAG,
    // The constructor for a new MutableEntry with the specified data failed.
    INIT_FAILED_COULD_NOT_CREATE_ENTRY,
    // Setting the predecessor failed
    INIT_FAILED_SET_PREDECESSOR,
    // Found existing entry, but was unable to decrypt.
    INIT_FAILED_DECRYPT_EXISTING_ENTRY,
  };

  // Create a WriteNode using the given transaction.
  explicit WriteNode(WriteTransaction* transaction);
  ~WriteNode() override;

  // A client must use one (and only one) of the following Init variants to
  // populate the node.

  // BaseNode implementation.
  InitByLookupResult InitByIdLookup(int64_t id) override;
  InitByLookupResult InitByClientTagLookup(ModelType model_type,
                                           const std::string& tag) override;

  // Create a new bookmark node with the specified parent and predecessor.  Use
  // a null |predecessor| to indicate that this is to be the first child.
  // |predecessor| must be a child of |new_parent| or null. Returns false on
  // failure.
  bool InitBookmarkByCreation(const BaseNode& parent,
                              const BaseNode* predecessor);

  // Create nodes using this function if they're unique items that
  // you want to fetch using client_tag. Note that the behavior of these
  // items is slightly different than that of normal items.
  // Most importantly, if it exists locally but is deleted, this function will
  // actually undelete it. Otherwise it will reuse the existing node.
  // Client unique tagged nodes must NOT be folders.
  InitUniqueByCreationResult InitUniqueByCreation(
      ModelType model_type,
      const BaseNode& parent,
      const std::string& client_tag);

  // InitUniqueByCreation overload for model types without hierarchy.
  // The parent node isn't stored but is assumed to be the type root folder.
  InitUniqueByCreationResult InitUniqueByCreation(
      ModelType model_type,
      const std::string& client_tag);

  // Looks up the type's root folder.  This is usually created by the sync
  // server during initial sync, though we do eventually wish to remove it from
  // the protocol and have the client "fake it" instead.
  InitByLookupResult InitTypeRoot(ModelType type);

  // These Set() functions correspond to the Get() functions of BaseNode.
  void SetIsFolder(bool folder);
  void SetTitle(const std::string& title);

  // External ID is a client-only field, so setting it doesn't cause the item to
  // be synced again.
  void SetExternalId(int64_t external_id);

  // Remove this node and its children and sync deletion to server.
  void Tombstone();

  // If the node is known by server, remove it and its children but don't sync
  // deletion to server. Do nothing if the node is not known by server so that
  // server can have a record of the node.
  void Drop();

  // Set a new parent and position.  Position is specified by |predecessor|; if
  // it is null, the node is moved to the first position.  |predecessor| must
  // be a child of |new_parent| or null.  Returns false on failure..
  bool SetPosition(const BaseNode& new_parent, const BaseNode* predecessor);

  // Set the bookmark specifics (url and favicon).
  // Should only be called if GetModelType() == BOOKMARK.
  void SetBookmarkSpecifics(const sync_pb::BookmarkSpecifics& specifics);

  // Generic set specifics method. Will extract the model type from |specifics|.
  void SetEntitySpecifics(const sync_pb::EntitySpecifics& specifics);

  // Resets the EntitySpecifics for this node based on the unencrypted data.
  // Will encrypt if necessary.
  void ResetFromSpecifics();

  // TODO(sync): Remove the setters below when the corresponding data
  // types are ported to the new sync service API.

  // Set the nigori specifics.
  // Should only be called if GetModelType() == NIGORI.
  void SetNigoriSpecifics(const sync_pb::NigoriSpecifics& specifics);

  // Set the password specifics.
  // Should only be called if GetModelType() == PASSWORD.
  void SetPasswordSpecifics(const sync_pb::PasswordSpecificsData& specifics);

  // Set the wifi configuration specifics.
  // Should only be called if GetModelType() == WIFI_CONFIGURATION.
  void SetWifiConfigurationSpecifics(
      const sync_pb::WifiConfigurationSpecificsData& specifics);

  // Set the typed_url specifics (url, title, typed_count, etc).
  // Should only be called if GetModelType() == TYPED_URLS.
  void SetTypedUrlSpecifics(const sync_pb::TypedUrlSpecifics& specifics);

  // Implementation of BaseNode's abstract virtual accessors.
  const syncable::Entry* GetEntry() const override;

  const BaseTransaction* GetTransaction() const override;

  syncable::MutableEntry* GetMutableEntryForTest();

 private:
  FRIEND_TEST_ALL_PREFIXES(SyncManagerTest, EncryptBookmarksWithLegacyData);

  void* operator new(size_t size);  // Node is meant for stack use only.

  InitUniqueByCreationResult InitUniqueByCreationImpl(
      ModelType model_type,
      const syncable::Id& parent_id,
      const std::string& client_tag);

  // Helper to set the previous node.
  bool PutPredecessor(const BaseNode* predecessor) WARN_UNUSED_RESULT;

  // Sets IS_UNSYNCED and SYNCING to ensure this entry is considered in an
  // upcoming commit pass.
  void MarkForSyncing();

  // The underlying syncable object which this class wraps.
  syncable::MutableEntry* entry_;

  // The sync API transaction that is the parent of this node.
  WriteTransaction* transaction_;

  DISALLOW_COPY_AND_ASSIGN(WriteNode);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_WRITE_NODE_H_
