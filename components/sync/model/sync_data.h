// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_DATA_H_
#define COMPONENTS_SYNC_MODEL_SYNC_DATA_H_

#include <stdint.h>

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/sync/base/immutable.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/weak_handle.h"

namespace sync_pb {
class EntitySpecifics;
class SyncEntity;
}  // namespace sync_pb

namespace syncer {

class ClientTagHash;
class SyncDataLocal;
class SyncDataRemote;

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
  // |sync_tag| Must be a string unique to this datatype and is used as a node
  // identifier server-side.
  //
  // For deletes: |datatype| must specify the datatype who node is being
  // deleted.
  //
  // For adds/updates: |specifics| must be valid and |non_unique_title| (can be
  // the same as |sync_tag|) must be specfied.  Note: |non_unique_title| is
  // primarily for debug purposes, and will be overwritten if the datatype is
  // encrypted.
  static SyncData CreateLocalDelete(const std::string& sync_tag,
                                    ModelType datatype);
  static SyncData CreateLocalData(const std::string& sync_tag,
                                  const std::string& non_unique_title,
                                  const sync_pb::EntitySpecifics& specifics);

  // Helper method for creating SyncData objects originating from the syncer.
  static SyncData CreateRemoteData(int64_t id,
                                   sync_pb::EntitySpecifics specifics,
                                   std::string client_tag_hash = std::string());

  // Whether this SyncData holds valid data. The only way to have a SyncData
  // without valid data is to use the default constructor.
  bool IsValid() const;

  // Return the datatype we're holding information about. Derived from the sync
  // datatype specifics.
  ModelType GetDataType() const;

  // Return the current sync datatype specifics.
  const sync_pb::EntitySpecifics& GetSpecifics() const;

  // Return the non unique title (for debugging). Currently only set for data
  // going TO the syncer, not from.
  const std::string& GetTitle() const;

  // Whether this sync data is for local data or data coming from the syncer.
  bool IsLocal() const;

  std::string ToString() const;

  // TODO(zea): Query methods for other sync properties: parent, successor, etc.

 protected:
  // These data members are protected so derived types like SyncDataLocal and
  // SyncDataRemote can access them.

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

  int64_t id_;

  // The actual shared sync entity being held.
  ImmutableSyncEntity immutable_entity_;

 private:
  // Whether this SyncData represents a local change.
  bool is_local_;

  // Whether this SyncData holds valid data.
  bool is_valid_;

  // Clears |entity|.
  SyncData(bool is_local_, int64_t id, sync_pb::SyncEntity* entity);
};

// A SyncData going to the syncer.
class SyncDataLocal : public SyncData {
 public:
  // Construct a SyncDataLocal from a SyncData.
  //
  // |sync_data|'s IsLocal() must be true.
  explicit SyncDataLocal(const SyncData& sync_data);
  ~SyncDataLocal();

  // Return the value of the unique client tag. This is only set for data going
  // TO the syncer, not coming from.
  const std::string& GetTag() const;
};

// A SyncData that comes from the syncer.
class SyncDataRemote : public SyncData {
 public:
  // Construct a SyncDataRemote from a SyncData.
  //
  // |sync_data|'s IsLocal() must be false.
  explicit SyncDataRemote(const SyncData& sync_data);
  ~SyncDataRemote();

  // Returns the tag hash value. May not always be present, in which case an
  // empty string will be returned.
  ClientTagHash GetClientTagHash() const;

  // Deprecated: might not be populated in SyncableService API.
  // TODO(crbug.com/870624): Remove when directory is removed.
  int64_t GetId() const;
};

// gmock printer helper.
void PrintTo(const SyncData& sync_data, std::ostream* os);

using SyncDataList = std::vector<SyncData>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_DATA_H_
