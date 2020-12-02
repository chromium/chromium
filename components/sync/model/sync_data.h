// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_DATA_H_
#define COMPONENTS_SYNC_MODEL_SYNC_DATA_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/immutable.h"
#include "components/sync/base/model_type.h"

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
}  // namespace sync_pb

namespace syncer {

// A light-weight container for immutable sync data. Pass-by-value and storage
// in STL containers are supported and encouraged if helpful.
class SyncData {
 public:
  // Creates an empty and invalid SyncData.
  SyncData();
  SyncData(const SyncData& other);
  ~SyncData();

  // Default copy and assign welcome.

  // Helper methods for creating SyncData objects for local data.
  //
  // |client_tag_unhashed| Must be a non-empty string unique to this entity and
  // is used (in hashed form) as a node identifier server-side.
  //
  // For deletes: |datatype| must specify the datatype who node is being
  // deleted.
  //
  // For adds/updates: |specifics| must be valid and |non_unique_title| (can be
  // the same as |client_tag_unhashed|) must be specfied.  Note:
  // |non_unique_title| is primarily for debug purposes, and will be overwritten
  // if the datatype is encrypted.
  static SyncData CreateLocalDelete(const std::string& client_tag_unhashed,
                                    ModelType datatype);
  static SyncData CreateLocalData(const std::string& client_tag_unhashed,
                                  const std::string& non_unique_title,
                                  const sync_pb::EntitySpecifics& specifics);

  // Helper method for creating SyncData objects originating from the syncer.
  static SyncData CreateRemoteData(
      sync_pb::EntitySpecifics specifics,
      const ClientTagHash& client_tag_hash = ClientTagHash());

  // Whether this SyncData holds valid data. The only way to have a SyncData
  // without valid data is to use the default constructor.
  bool IsValid() const;

  // Return the datatype we're holding information about. Derived from the sync
  // datatype specifics.
  ModelType GetDataType() const;

  // Return the value of the unique client tag hash.
  ClientTagHash GetClientTagHash() const;

  // Return the current sync datatype specifics.
  const sync_pb::EntitySpecifics& GetSpecifics() const;

  // Return the non unique title (for debugging). Currently only set for data
  // going TO the syncer, not from.
  const std::string& GetTitle() const;

  // Whether this sync data is for local data or data coming from the syncer.
  bool IsLocal() const;

  std::string ToString() const;

 private:
  // Necessary since we forward-declare sync_pb::SyncEntity; see
  // comments in immutable.h.
  struct ImmutableSyncEntityTraits {
    using Wrapper = sync_pb::SyncEntity*;

    static void InitializeWrapper(Wrapper* wrapper);

    static void DestroyWrapper(Wrapper* wrapper);

    static const sync_pb::SyncEntity& Unwrap(const Wrapper& wrapper);

    static sync_pb::SyncEntity* UnwrapMutable(Wrapper* wrapper);

    static void Swap(sync_pb::SyncEntity* t1, sync_pb::SyncEntity* t2);
  };

  using ImmutableSyncEntity =
      Immutable<sync_pb::SyncEntity, ImmutableSyncEntityTraits>;

  // The actual shared sync entity being held.
  ImmutableSyncEntity immutable_entity_;

  // Whether this SyncData represents a local change.
  bool is_local_;

  // Whether this SyncData holds valid data.
  bool is_valid_;

  // Clears |entity|.
  SyncData(bool is_local_, sync_pb::SyncEntity* entity);
};

// gmock printer helper.
void PrintTo(const SyncData& sync_data, std::ostream* os);

using SyncDataList = std::vector<SyncData>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_DATA_H_
