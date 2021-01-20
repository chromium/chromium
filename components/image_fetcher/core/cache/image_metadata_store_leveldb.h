// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_METADATA_STORE_LEVELDB_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_METADATA_STORE_LEVELDB_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/image_fetcher/core/cache/image_metadata_store.h"
#include "components/image_fetcher/core/cache/image_store_types.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace base {
class Clock;
class SequencedTaskRunner;
}  // namespace base

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace image_fetcher {

class CachedImageMetadataProto;

// Stores image metadata in leveldb.
class ImageMetadataStoreLevelDB : public ImageMetadataStore {
 public:
  // Initializes the database with |proto_database_provider|.
  ImageMetadataStoreLevelDB(
      leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
      const base::FilePath& database_dir,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::Clock* clock);

  // Creates storage using the given |database| for local storage. Useful for
  // testing.
  ImageMetadataStoreLevelDB(
      std::unique_ptr<leveldb_proto::ProtoDatabase<CachedImageMetadataProto>>
          database,
      base::Clock* clock);
  ~ImageMetadataStoreLevelDB() override;

  // ImageMetadataStorage:
  void Initialize(base::OnceClosure callback) override;
  bool IsInitialized() override;
  void LoadImageMetadata(const std::string& key,
                         ImageMetadataCallback callback) override;
  void SaveImageMetadata(const std::string& key,
                         const size_t data_size,
                         bool needs_transcoding,
                         ExpirationInterval expiration_interval) override;
  void DeleteImageMetadata(const std::string& key) override;
  void UpdateImageMetadata(const std::string& key) override;
  void GetAllKeys(KeysCallback callback) override;
  // Gets a size estimate. This is updated when things are added to the cache
  // and when eviction is performed. This doesn't update more often because
  // we want to minimize disk io. If the estimate is inaccurate, it's either 0
  // which means it hasn't been calculated yet, or it's an over-estimate. In
  // the case of an over estimate, it will be recified if you call into an
  // eviction routine.
  int64_t GetEstimatedSize(CacheOption cache_option) override;
  void EvictImageMetadata(base::Time expiration_time,
                          const size_t bytes_left,
                          KeysCallback callback) override;

 private:
  void OnDatabaseInitialized(base::OnceClosure callback,
                             leveldb_proto::Enums::InitStatus status);
  void LoadImageMetadataImpl(
      ImageMetadataCallback callback,
      bool success,
      std::unique_ptr<std::vector<CachedImageMetadataProto>> entries);
  void OnImageUpdated(bool success);
  void UpdateImageMetadataImpl(
      bool success,
      std::unique_ptr<std::vector<CachedImageMetadataProto>> entries);
  void GetAllKeysImpl(KeysCallback callback,
                      bool success,
                      std::unique_ptr<std::vector<std::string>> keys);
  void EvictImageMetadataImpl(
      base::Time expiration_time,
      const size_t bytes_left,
      KeysCallback callback,
      bool success,
      std::unique_ptr<std::vector<CachedImageMetadataProto>> entries);
  void GetMetadataToRemove(CacheOption cache_option,
                           std::vector<const CachedImageMetadataProto*> entries,
                           base::Time expiration_time,
                           const size_t bytes_left,
                           std::vector<std::string>* keys_to_remove);
  void OnEvictImageMetadataDone(KeysCallback callback,
                                std::vector<std::string> deleted_keys,
                                bool success);

  std::map<CacheOption, int64_t> estimated_size_;
  InitializationStatus initialization_status_;
  std::unique_ptr<leveldb_proto::ProtoDatabase<CachedImageMetadataProto>>
      database_;
  // Clock is owned by the service that creates this object.
  base::Clock* clock_;
  base::WeakPtrFactory<ImageMetadataStoreLevelDB> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImageMetadataStoreLevelDB);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_METADATA_STORE_LEVELDB_H_
