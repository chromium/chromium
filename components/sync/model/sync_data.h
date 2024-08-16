// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_DATA_H_
#define COMPONENTS_SYNC_MODEL_SYNC_DATA_H_

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"

namespace sync_pb {
class EntitySpecifics;
}  // namespace sync_pb

namespace syncer {

// A light-weight container for immutable sync data. Pass-by-value and storage
// in STL containers are supported and encouraged if helpful.
class SyncData {
 public:
  // Creates an empty and invalid SyncData.
  SyncData();

  // Copyable and movable, all cheap.
  SyncData(const SyncData& other);
  SyncData(SyncData&& other);
  SyncData& operator=(const SyncData& other);
  SyncData& operator=(SyncData&& other);

  ~SyncData();

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
  static SyncData CreateLocalDelete(std::string_view client_tag_unhashed,
                                    DataType datatype);
  static SyncData CreateLocalData(std::string_view client_tag_unhashed,
                                  std::string_view non_unique_title,
                                  const sync_pb::EntitySpecifics& specifics);

  // Helper method for creating SyncData objects originating from the syncer.
  static SyncData CreateRemoteData(sync_pb::EntitySpecifics specifics,
                                   const ClientTagHash& client_tag_hash);

  // Whether this SyncData holds valid data. An instance can be invalid either
  // if the default value was used upon construction or if an instance was moved
  // away.
  bool IsValid() const;

  // Return the datatype we're holding information about. Derived from the sync
  // datatype specifics.
  DataType GetDataType() const;

  // Return the value of the unique client tag hash.
  ClientTagHash GetClientTagHash() const;

  // Return the current sync datatype specifics.
  const sync_pb::EntitySpecifics& GetSpecifics() const;

  // Return the non unique title (for debugging). Currently only set for data
  // going TO the syncer, not from.
  const std::string& GetTitle() const;

  std::string ToString() const;

 private:
  // Forward-declared to avoid includes for EntitySpecifics.
  struct InternalData;

  // Null if data invalid, i.e. default constructor used or moved-away instance.
  scoped_refptr<InternalData> ptr_;

  explicit SyncData(scoped_refptr<InternalData> ptr);
};

// gmock printer helper.
void PrintTo(const SyncData& sync_data, std::ostream* os);

using SyncDataList = std::vector<SyncData>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_DATA_H_
