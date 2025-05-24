// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_DATA_MAP_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_DATA_MAP_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/services/storage/dom_storage/session_storage_metadata.h"
#include "components/services/storage/dom_storage/storage_area_impl.h"

namespace storage {

class AsyncDomStorageDatabase;

// Holds the StorageArea for a session storage data map. Every
// namespace-origin area has a data map. To support shallow copying of the data
// (copy-on-write), a single data map can be shared between multiple namespaces.
// Thus this class is refcounted. This class has a one-to-one relationship with
// the SessionStorageMetadata::MapData object, accessible from
// |map_data()|.
//
// Neither this data map nor the inner StorageArea is bound to, as it needs
// to be shared between multiple connections if it is shallow-copied. However,
// it does allow it's user to keep track of the number of binding using
// |binding_count()|, |AddBindingReference()|, and |RemoveBindingReference()|.
class SessionStorageDataMap final
    : public StorageAreaImpl::Delegate,
      public base::RefCounted<SessionStorageDataMap> {
 public:
  class Listener {
   public:
    virtual ~Listener() = default;
    virtual void OnDataMapCreation(const std::vector<uint8_t>& map_id,
                                   SessionStorageDataMap* map) = 0;
    virtual void OnDataMapDestruction(const std::vector<uint8_t>& map_id) = 0;
    virtual void OnCommitResult(leveldb::Status status) = 0;
  };

  static scoped_refptr<SessionStorageDataMap> CreateFromDisk(
      Listener* listener,
      scoped_refptr<SessionStorageMetadata::MapData> map_data,
      AsyncDomStorageDatabase* database);

  static scoped_refptr<SessionStorageDataMap> CreateEmpty(
      Listener* listener,
      scoped_refptr<SessionStorageMetadata::MapData> map_data,
      AsyncDomStorageDatabase* database);

  static scoped_refptr<SessionStorageDataMap> CreateClone(
      Listener* listener,
      scoped_refptr<SessionStorageMetadata::MapData> map_data,
      scoped_refptr<SessionStorageDataMap> clone_from);

  SessionStorageDataMap(const SessionStorageDataMap&) = delete;
  SessionStorageDataMap& operator=(const SessionStorageDataMap&) = delete;

  Listener* listener() const { return listener_; }

  StorageAreaImpl* storage_area() { return storage_area_ptr_; }

  scoped_refptr<SessionStorageMetadata::MapData> map_data() {
    return map_data_.get();
  }

  int binding_count() { return binding_count_; }
  void AddBindingReference() { ++binding_count_; }
  // When the binding count reaches 0, we schedule an immediate commit on our
  // area, but we don't close the connection.
  void RemoveBindingReference();

  // Note: this is irrelevant, as the parent area is handling binding.
  void OnNoBindings() override {}

  void DidCommit(leveldb::Status status) override;

 private:
  friend class base::RefCounted<SessionStorageDataMap>;

  SessionStorageDataMap(
      Listener* listener,
      scoped_refptr<SessionStorageMetadata::MapData> map_entry,
      AsyncDomStorageDatabase* database,
      bool is_empty);
  SessionStorageDataMap(
      Listener* listener,
      scoped_refptr<SessionStorageMetadata::MapData> map_entry,
      scoped_refptr<SessionStorageDataMap> forking_from);
  ~SessionStorageDataMap() override;

  void OnMapLoaded(leveldb::Status status) override;

  static StorageAreaImpl::Options GetOptions();

  raw_ptr<Listener> listener_;
  int binding_count_ = 0;

  // If we're cloning from another map, we need to keep it alive while it forks.
  // Note, this has to be above the storage_area_impl_ in case the Fork call
  // completes synchronously.
  scoped_refptr<SessionStorageDataMap> clone_from_data_map_;

  scoped_refptr<SessionStorageMetadata::MapData> map_data_;
  std::unique_ptr<StorageAreaImpl> storage_area_impl_;
  // Holds the same value as |storage_area_impl_|. The reason for this is that
  // during destruction of the StorageAreaImpl instance we might still get
  // called and need access  to the StorageAreaImpl instance. The
  // unique_ptr could already be null, but this field should still be valid.
  // TODO(dmurph): Change delegate ownership so this doesn't have to be done.
  raw_ptr<StorageAreaImpl> storage_area_ptr_;
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_SESSION_STORAGE_DATA_MAP_H_
