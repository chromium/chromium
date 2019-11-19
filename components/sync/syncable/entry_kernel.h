// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_ENTRY_KERNEL_H_
#define COMPONENTS_SYNC_SYNCABLE_ENTRY_KERNEL_H_

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/immutable.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/metahandle_set.h"
#include "components/sync/syncable/proto_value_ptr.h"
#include "components/sync/syncable/syncable_id.h"

namespace syncer {

class Cryptographer;

namespace syncable {

// Things you need to update if you change any of the fields below:
//  - EntryKernel struct in this file
//  - syncable_columns.h
//  - syncable_enum_conversions{.h,.cc,_unittest.cc}
//  - EntryKernel::EntryKernel(), EntryKernel::ToValue() in entry_kernel.cc
//  - operator<< in Entry.cc
//  - BindFields() and UnpackEntry() in directory_backing_store.cc
//  - kCurrentDBVersion, DirectoryBackingStore::InitializeTables in
//    directory_backing_store.cc
//  - TestSimpleFieldsPreservedDuringSaveChanges in syncable_unittest.cc

static const int64_t kInvalidMetaHandle = 0;

enum { BEGIN_FIELDS = 0, INT64_FIELDS_BEGIN = BEGIN_FIELDS };

enum MetahandleField {
  // Primary key into the table.  Keep this as a handle to the meta entry
  // across transactions.
  META_HANDLE = INT64_FIELDS_BEGIN
};

enum BaseVersion {
  // After initial upload, the version is controlled by the server, and is
  // increased whenever the data or metadata changes on the server.
  BASE_VERSION = META_HANDLE + 1,
};

enum Int64Field {
  SERVER_VERSION = BASE_VERSION + 1,
  LOCAL_EXTERNAL_ID,  // ID of an item in the external local storage that this
                      // entry is associated with. (such as bookmarks.js)
  TRANSACTION_VERSION,
  INT64_FIELDS_END
};

enum {
  INT64_FIELDS_COUNT = INT64_FIELDS_END - INT64_FIELDS_BEGIN,
  TIME_FIELDS_BEGIN = INT64_FIELDS_END,
};

enum TimeField {
  MTIME = TIME_FIELDS_BEGIN,
  SERVER_MTIME,
  CTIME,
  SERVER_CTIME,
  TIME_FIELDS_END,
};

enum {
  TIME_FIELDS_COUNT = TIME_FIELDS_END - TIME_FIELDS_BEGIN,
  ID_FIELDS_BEGIN = TIME_FIELDS_END,
};

enum IdField {
  // Code in InitializeTables relies on ID being the first IdField value.
  ID = ID_FIELDS_BEGIN,
  PARENT_ID,
  SERVER_PARENT_ID,
  ID_FIELDS_END
};

enum {
  ID_FIELDS_COUNT = ID_FIELDS_END - ID_FIELDS_BEGIN,
  BIT_FIELDS_BEGIN = ID_FIELDS_END
};

enum IndexedBitField {
  IS_UNSYNCED = BIT_FIELDS_BEGIN,
  IS_UNAPPLIED_UPDATE,
  INDEXED_BIT_FIELDS_END,
};

enum IsDelField {
  IS_DEL = INDEXED_BIT_FIELDS_END,
};

enum BitField {
  IS_DIR = IS_DEL + 1,
  SERVER_IS_DIR,
  SERVER_IS_DEL,
  BIT_FIELDS_END
};

enum {
  BIT_FIELDS_COUNT = BIT_FIELDS_END - BIT_FIELDS_BEGIN,
  STRING_FIELDS_BEGIN = BIT_FIELDS_END
};

enum StringField {
  // Name, will be truncated by server. Can be duplicated in a folder.
  NON_UNIQUE_NAME = STRING_FIELDS_BEGIN,
  // The server version of |NON_UNIQUE_NAME|.
  SERVER_NON_UNIQUE_NAME,

  // A tag string which identifies this node as a particular top-level
  // permanent object.  The tag can be thought of as a unique key that
  // identifies a singleton instance.
  UNIQUE_SERVER_TAG,    // Tagged by the server
  UNIQUE_CLIENT_TAG,    // Tagged by the client
  UNIQUE_BOOKMARK_TAG,  // Client tags for bookmark items
  STRING_FIELDS_END,
};

enum {
  STRING_FIELDS_COUNT = STRING_FIELDS_END - STRING_FIELDS_BEGIN,
  PROTO_FIELDS_BEGIN = STRING_FIELDS_END
};

// From looking at the sqlite3 docs, it's not directly stated, but it
// seems the overhead for storing a null blob is very small.
enum ProtoField {
  SPECIFICS = PROTO_FIELDS_BEGIN,
  SERVER_SPECIFICS,
  BASE_SERVER_SPECIFICS,
  PROTO_FIELDS_END,
};

enum {
  PROTO_FIELDS_COUNT = PROTO_FIELDS_END - PROTO_FIELDS_BEGIN,
  UNIQUE_POSITION_FIELDS_BEGIN = PROTO_FIELDS_END
};

enum UniquePositionField {
  SERVER_UNIQUE_POSITION = UNIQUE_POSITION_FIELDS_BEGIN,
  UNIQUE_POSITION,
  UNIQUE_POSITION_FIELDS_END
};

enum {
  UNIQUE_POSITION_FIELDS_COUNT =
      UNIQUE_POSITION_FIELDS_END - UNIQUE_POSITION_FIELDS_BEGIN
};

enum {
  // If FIELD_COUNT is changed then g_metas_columns must be updated.
  FIELD_COUNT = UNIQUE_POSITION_FIELDS_END - BEGIN_FIELDS,
  // Past this point we have temporaries, stored in memory only.
  BEGIN_TEMPS = UNIQUE_POSITION_FIELDS_END,
  BIT_TEMPS_BEGIN = BEGIN_TEMPS,
};

enum BitTemp {
  // Whether a server commit operation was started and has not yet completed
  // for this entity.
  SYNCING = BIT_TEMPS_BEGIN,
  // Whether a local change was made to an entity that had SYNCING set to true,
  // and was therefore in the middle of a commit operation.
  // Note: must only be set if SYNCING is true.
  DIRTY_SYNC,
  BIT_TEMPS_END,
};

enum { BIT_TEMPS_COUNT = BIT_TEMPS_END - BIT_TEMPS_BEGIN };

struct EntryKernel {
 private:
  using EntitySpecificsPtr = ProtoValuePtr<sync_pb::EntitySpecifics>;

  std::string string_fields[STRING_FIELDS_COUNT];
  EntitySpecificsPtr specifics_fields[PROTO_FIELDS_COUNT];
  int64_t int64_fields[INT64_FIELDS_COUNT];
  base::Time time_fields[TIME_FIELDS_COUNT];
  Id id_fields[ID_FIELDS_COUNT];
  UniquePosition unique_position_fields[UNIQUE_POSITION_FIELDS_COUNT];
  std::bitset<BIT_FIELDS_COUNT> bit_fields;
  std::bitset<BIT_TEMPS_COUNT> bit_temps;

  friend std::ostream& operator<<(std::ostream& s, const EntryKernel& e);

 public:
  EntryKernel();
  EntryKernel(const EntryKernel& other);
  ~EntryKernel();

  // Set the dirty bit, and optionally add this entry's metahandle to
  // a provided index on dirty bits in |dirty_index|. Parameter may be null,
  // and will result only in setting the dirty bit of this entry.
  inline void mark_dirty(syncable::MetahandleSet* dirty_index) {
    if (!dirty_ && dirty_index) {
      DCHECK_NE(0, ref(META_HANDLE));
      dirty_index->insert(ref(META_HANDLE));
    }
    dirty_ = true;
    memory_usage_ = kMemoryUsageUnknown;
  }

  // Clear the dirty bit, and optionally remove this entry's metahandle from
  // a provided index on dirty bits in |dirty_index|. Parameter may be null,
  // and will result only in clearing dirty bit of this entry.
  inline void clear_dirty(syncable::MetahandleSet* dirty_index) {
    if (dirty_ && dirty_index) {
      DCHECK_NE(0, ref(META_HANDLE));
      dirty_index->erase(ref(META_HANDLE));
    }
    dirty_ = false;
  }

  inline bool is_dirty() const { return dirty_; }

  // Setters.
  inline void put(MetahandleField field, int64_t value) {
    int64_fields[field - INT64_FIELDS_BEGIN] = value;
  }
  inline void put(Int64Field field, int64_t value) {
    int64_fields[field - INT64_FIELDS_BEGIN] = value;
  }
  inline void put(TimeField field, const base::Time& value) {
    // Round-trip to proto time format and back so that we have
    // consistent time resolutions (ms).
    time_fields[field - TIME_FIELDS_BEGIN] =
        ProtoTimeToTime(TimeToProtoTime(value));
  }
  inline void put(IdField field, const Id& value) {
    id_fields[field - ID_FIELDS_BEGIN] = value;
  }
  inline void put(BaseVersion field, int64_t value) {
    int64_fields[field - INT64_FIELDS_BEGIN] = value;
  }
  inline void put(IndexedBitField field, bool value) {
    bit_fields[field - BIT_FIELDS_BEGIN] = value;
  }
  inline void put(IsDelField field, bool value) {
    bit_fields[field - BIT_FIELDS_BEGIN] = value;
  }
  inline void put(BitField field, bool value) {
    bit_fields[field - BIT_FIELDS_BEGIN] = value;
  }
  inline void put(StringField field, const std::string& value) {
    string_fields[field - STRING_FIELDS_BEGIN] = value;
  }
  inline void put(ProtoField field, const sync_pb::EntitySpecifics& value) {
    specifics_fields[field - PROTO_FIELDS_BEGIN].set_value(value);
  }
  inline void put(UniquePositionField field, const UniquePosition& value) {
    unique_position_fields[field - UNIQUE_POSITION_FIELDS_BEGIN] = value;
  }
  inline void put(BitTemp field, bool value) {
    bit_temps[field - BIT_TEMPS_BEGIN] = value;
  }

  // Const ref getters.
  inline int64_t ref(MetahandleField field) const {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline int64_t ref(Int64Field field) const {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline const base::Time& ref(TimeField field) const {
    return time_fields[field - TIME_FIELDS_BEGIN];
  }
  inline const Id& ref(IdField field) const {
    return id_fields[field - ID_FIELDS_BEGIN];
  }
  inline int64_t ref(BaseVersion field) const {
    return int64_fields[field - INT64_FIELDS_BEGIN];
  }
  inline bool ref(IndexedBitField field) const {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline bool ref(IsDelField field) const {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline bool ref(BitField field) const {
    return bit_fields[field - BIT_FIELDS_BEGIN];
  }
  inline const std::string& ref(StringField field) const {
    return string_fields[field - STRING_FIELDS_BEGIN];
  }
  inline const sync_pb::EntitySpecifics& ref(ProtoField field) const {
    return specifics_fields[field - PROTO_FIELDS_BEGIN].value();
  }
  inline const UniquePosition& ref(UniquePositionField field) const {
    return unique_position_fields[field - UNIQUE_POSITION_FIELDS_BEGIN];
  }
  inline bool ref(BitTemp field) const {
    return bit_temps[field - BIT_TEMPS_BEGIN];
  }

  // Non-const, mutable ref getters for object types only.
  inline std::string& mutable_ref(StringField field) {
    return string_fields[field - STRING_FIELDS_BEGIN];
  }
  inline Id& mutable_ref(IdField field) {
    return id_fields[field - ID_FIELDS_BEGIN];
  }
  inline UniquePosition& mutable_ref(UniquePositionField field) {
    return unique_position_fields[field - UNIQUE_POSITION_FIELDS_BEGIN];
  }

  // Deserialization methods for ::google::protobuf::MessageLite derived types.
  inline void load(ProtoField field, const void* blob, int length) {
    specifics_fields[field - PROTO_FIELDS_BEGIN].load(blob, length);
  }

  // Sharing data methods for ::google::protobuf::MessageLite derived types.
  inline void copy(ProtoField src, ProtoField dest) {
    DCHECK_NE(src, dest);
    specifics_fields[dest - PROTO_FIELDS_BEGIN] =
        specifics_fields[src - PROTO_FIELDS_BEGIN];
  }

  ModelType GetModelType() const;
  ModelType GetServerModelType() const;
  bool ShouldMaintainPosition() const;
  bool ShouldMaintainHierarchy() const;

  // Dumps all kernel info into a DictionaryValue and returns it.
  // Note: |cryptographer| is an optional parameter for use in decrypting
  // encrypted specifics. If it is null or the specifics are not decryptsble,
  // they will be serialized as empty proto's.
  std::unique_ptr<base::DictionaryValue> ToValue(
      const Cryptographer* cryptographer) const;

  size_t EstimateMemoryUsage() const;

 private:
  // Tracks whether this entry needs to be saved to the database.
  bool dirty_;
  mutable size_t memory_usage_;
  constexpr static size_t kMemoryUsageUnknown = size_t(-1);
};

template <typename T>
class EntryKernelLessByMetaHandle {
 public:
  inline bool operator()(T a, T b) const {
    return a->ref(META_HANDLE) < b->ref(META_HANDLE);
  }
};

using EntryKernelSet =
    std::set<const EntryKernel*,
             EntryKernelLessByMetaHandle<const EntryKernel*>>;

using OwnedEntryKernelSet =
    std::set<std::unique_ptr<EntryKernel>,
             EntryKernelLessByMetaHandle<const std::unique_ptr<EntryKernel>&>>;

struct EntryKernelMutation {
  EntryKernel original, mutated;
};

using EntryKernelMutationMap = std::map<int64_t, EntryKernelMutation>;

using ImmutableEntryKernelMutationMap = Immutable<EntryKernelMutationMap>;

std::unique_ptr<base::DictionaryValue> EntryKernelMutationToValue(
    const EntryKernelMutation& mutation);

std::unique_ptr<base::ListValue> EntryKernelMutationMapToValue(
    const EntryKernelMutationMap& mutations);

std::ostream& operator<<(std::ostream& os, const EntryKernel& entry_kernel);

}  // namespace syncable
}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_ENTRY_KERNEL_H_
