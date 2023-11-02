// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/session_storage_data_map.h"

#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"

namespace storage {

// static
scoped_refptr<SessionStorageDataMap> SessionStorageDataMap::CreateFromDisk(
    Listener* listener,
    scoped_refptr<SessionStorageMetadata::MapData> map_data,
    AsyncDomStorageDatabase* database) {
  return base::WrapRefCounted(new SessionStorageDataMap(
      listener, std::move(map_data), database, false));
}

// static
scoped_refptr<SessionStorageDataMap> SessionStorageDataMap::CreateEmpty(
    Listener* listener,
    scoped_refptr<SessionStorageMetadata::MapData> map_data,
    AsyncDomStorageDatabase* database) {
  return base::WrapRefCounted(
      new SessionStorageDataMap(listener, std::move(map_data), database, true));
}

// static
scoped_refptr<SessionStorageDataMap> SessionStorageDataMap::CreateClone(
    Listener* listener,
    scoped_refptr<SessionStorageMetadata::MapData> map_data,
    scoped_refptr<SessionStorageDataMap> clone_from) {
  return base::WrapRefCounted(new SessionStorageDataMap(
      listener, std::move(map_data), std::move(clone_from)));
}

void SessionStorageDataMap::DidCommit(leveldb::Status status) {
  listener_->OnCommitResult(status);
}

SessionStorageDataMap::SessionStorageDataMap(
    Listener* listener,
    scoped_refptr<SessionStorageMetadata::MapData> map_data,
    AsyncDomStorageDatabase* database,
    bool is_empty)
    : listener_(listener),
      map_data_(std::move(map_data)),
      storage_area_impl_(
          std::make_unique<StorageAreaImpl>(database,
                                            map_data_->KeyPrefix(),
                                            this,
                                            GetOptions())),
      storage_area_ptr_(storage_area_impl_.get()) {
  if (is_empty)
    storage_area_impl_->InitializeAsEmpty();
  DCHECK(listener_);
  DCHECK(map_data_);
  listener_->OnDataMapCreation(map_data_->MapNumberAsBytes(), this);
}

SessionStorageDataMap::SessionStorageDataMap(
    Listener* listener,
    scoped_refptr<SessionStorageMetadata::MapData> map_data,
    scoped_refptr<SessionStorageDataMap> forking_from)
    : listener_(listener),
      clone_from_data_map_(std::move(forking_from)),
      map_data_(std::move(map_data)),
      storage_area_impl_(clone_from_data_map_->storage_area()->ForkToNewPrefix(
          map_data_->KeyPrefix(),
          this,
          GetOptions())),
      storage_area_ptr_(storage_area_impl_.get()) {
  DCHECK(listener_);
  DCHECK(map_data_);
  listener_->OnDataMapCreation(map_data_->MapNumberAsBytes(), this);
}

SessionStorageDataMap::~SessionStorageDataMap() {
  listener_->OnDataMapDestruction(map_data_->MapNumberAsBytes());
}

void SessionStorageDataMap::RemoveBindingReference() {
  DCHECK_GT(binding_count_, 0);
  --binding_count_;
  if (binding_count_ > 0)
    return;
  // Don't delete ourselves, but do schedule an immediate commit. Possible
  // deletion will happen under memory pressure or when another sessionstorage
  // area is opened.
  storage_area()->ScheduleImmediateCommit();
}

void SessionStorageDataMap::OnMapLoaded(leveldb::Status) {
  clone_from_data_map_.reset();
}

// static
StorageAreaImpl::Options SessionStorageDataMap::GetOptions() {
  // Delay for a moment after a value is set in anticipation
  // of other values being set, so changes are batched.
  constexpr const base::TimeDelta kCommitDefaultDelaySecs = base::Seconds(5);

  // To avoid excessive IO we apply limits to the amount of data being
  // written and the frequency of writes.
  StorageAreaImpl::Options options;
  options.max_size = kPerStorageAreaQuota + kPerStorageAreaOverQuotaAllowance;
  options.default_commit_delay = kCommitDefaultDelaySecs;
  options.max_bytes_per_hour = kPerStorageAreaQuota;
  options.max_commits_per_hour = 60;
  options.cache_mode = StorageAreaImpl::CacheMode::KEYS_ONLY_WHEN_POSSIBLE;
  return options;
}

}  // namespace storage
