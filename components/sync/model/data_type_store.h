// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_H_
#define COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_store_base.h"
#include "components/sync/model/model_error.h"

namespace syncer {

class MetadataBatch;

// DataTypeStore is leveldb backed store for data type's data, metadata and
// global metadata.
//
// Store keeps records for entries identified by ids. For each entry store keeps
// data and metadata. Also store keeps one record for global metadata.
//
// To create store call one of Create*Store static factory functions. Data type
// controls store's lifetime with returned unique_ptr. Call to Create*Store
// function triggers asynchronous store backend initialization, callback will be
// called with results when initialization is done.
//
// Read operations are asynchronous, initiated with one of Read* functions,
// provided callback will be called with result code and output of read
// operation.
//
// Write operations are done in context of write batch. To get one call
// CreateWriteBatch(). After that pass write batch object to Write/Delete
// functions. WriteBatch only accumulates pending changes, doesn't actually do
// data modification. Calling CommitWriteBatch writes all accumulated changes to
// disk atomically. Callback passed to CommitWriteBatch will be called with
// result of write operation. If write batch object is destroyed without
// comitting accumulated write operations will not be persisted.
//
// Destroying store object doesn't necessarily cancel asynchronous operations
// issued previously. You should be prepared to handle callbacks from those
// operations.
class DataTypeStore : public DataTypeStoreBase {
 public:
  using InitCallback =
      base::OnceCallback<void(const std::optional<ModelError>& error,
                              std::unique_ptr<DataTypeStore> store)>;
  using CallbackWithResult =
      base::OnceCallback<void(const std::optional<ModelError>& error)>;
  using ReadDataCallback =
      base::OnceCallback<void(const std::optional<ModelError>& error,
                              std::unique_ptr<RecordList> data_records,
                              std::unique_ptr<IdList> missing_id_list)>;
  using ReadAllDataCallback =
      base::OnceCallback<void(const std::optional<ModelError>& error,
                              std::unique_ptr<RecordList> data_records)>;
  using ReadMetadataCallback =
      base::OnceCallback<void(const std::optional<ModelError>& error,
                              std::unique_ptr<MetadataBatch> metadata_batch)>;
  using ReadAllDataAndMetadataCallback =
      base::OnceCallback<void(const std::optional<ModelError>& error,
                              std::unique_ptr<RecordList> data_records,
                              std::unique_ptr<MetadataBatch> metadata_batch)>;
  // Callback that runs on the backend sequence, see ReadAllDataAndPreprocess().
  using PreprocessCallback = base::OnceCallback<std::optional<ModelError>(
      std::unique_ptr<RecordList> data_records)>;

  // Read operations return records either for all entries or only for ones
  // identified in |id_list|. |error| is nullopt if all records were read
  // successfully, otherwise an empty or partial list of read records is
  // returned.
  // Callback for ReadData (ReadDataCallback) in addition receives list of ids
  // that were not found in store (missing_id_list).
  virtual void ReadData(const IdList& id_list, ReadDataCallback callback) = 0;
  virtual void ReadAllData(ReadAllDataCallback callback) = 0;
  // ReadMetadataCallback will be invoked with three parameters: result of
  // operation, list of metadata records and global metadata.
  virtual void ReadAllMetadata(ReadMetadataCallback callback) = 0;
  // Convenience wrapper calling `ReadAllData()` and `ReadAllMetadata()`. If
  // either one returns an error, the error is forwarded and the data and
  // metadata passed to the `callback` are empty.
  virtual void ReadAllDataAndMetadata(
      ReadAllDataAndMetadataCallback callback) = 0;

  // Similar to ReadAllData() but allows some custom processing in the
  // background sequence (e.g. proto parsing). Note that |preprocess_callback|
  // will not run if reading itself triggers an error.
  // |completion_on_frontend_sequence_callback| is guaranteed to outlive
  // |preprocess_on_backend_sequence_callback|.
  virtual void ReadAllDataAndPreprocess(
      PreprocessCallback preprocess_on_backend_sequence_callback,
      CallbackWithResult completion_on_frontend_sequence_callback) = 0;

  // Creates write batch for write operations.
  virtual std::unique_ptr<WriteBatch> CreateWriteBatch() = 0;

  // Commits write operations accumulated in write batch. If write operation
  // fails result is UNSPECIFIED_ERROR and write operations will not be
  // reflected in the store.
  virtual void CommitWriteBatch(std::unique_ptr<WriteBatch> write_batch,
                                CallbackWithResult callback) = 0;

  // Deletion of everything, usually exercised during DisableSync().
  virtual void DeleteAllDataAndMetadata(CallbackWithResult callback) = 0;
};

// Typedef for a store factory that has all params bound except InitCallback.
using RepeatingDataTypeStoreFactory =
    base::RepeatingCallback<void(DataType type, DataTypeStore::InitCallback)>;

// Same as above but as a OnceCallback.
using OnceDataTypeStoreFactory =
    base::OnceCallback<void(DataType type, DataTypeStore::InitCallback)>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_TYPE_STORE_H_
