// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_BASE_H_
#define COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_BASE_H_

#include <compare>
#include <memory>
#include <string>
#include <vector>

#include "components/sync/model/metadata_change_list.h"

namespace syncer {

// Base class for leveldb-based storage layers.
class DataTypeStoreBase {
 public:
  // Output of read operations is passed back as list of Record structures.
  struct Record {
    Record(const std::string& id, const std::string& value)
        : id(id), value(value) {}

    friend std::strong_ordering operator<=>(const Record&,
                                            const Record&) = default;

    std::string id;
    std::string value;
  };

  // WriteBatch object is used in all modification operations.
  class WriteBatch {
   public:
    // Creates a MetadataChangeList that will accumulate metadata changes and
    // can later be passed to a WriteBatch via TransferChanges. Use this when
    // you need a MetadataChangeList and do not have a WriteBatch in scope.
    static std::unique_ptr<MetadataChangeList> CreateMetadataChangeList();

    WriteBatch();

    WriteBatch(const WriteBatch&) = delete;
    WriteBatch& operator=(const WriteBatch&) = delete;

    virtual ~WriteBatch();

    // Write the given |value| for data with |id|.
    virtual void WriteData(const std::string& id, const std::string& value) = 0;

    // Delete the record for data with |id|.
    virtual void DeleteData(const std::string& id) = 0;

    // Provides access to a MetadataChangeList that will pass its changes
    // directly into this WriteBatch.
    virtual MetadataChangeList* GetMetadataChangeList() = 0;

    // Transfers the changes from a MetadataChangeList into this WriteBatch.
    // |mcl| must have previously been created by CreateMetadataChangeList().
    // TODO(mastiz): Revisit whether the last requirement above can be removed
    // and make this API more type-safe.
    void TakeMetadataChangesFrom(std::unique_ptr<MetadataChangeList> mcl);
  };

  using RecordList = std::vector<Record>;
  using IdList = std::vector<std::string>;

  DataTypeStoreBase(const DataTypeStoreBase&) = delete;
  DataTypeStoreBase& operator=(const DataTypeStoreBase&) = delete;

 protected:
  DataTypeStoreBase();
  virtual ~DataTypeStoreBase();
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_BASE_H_
