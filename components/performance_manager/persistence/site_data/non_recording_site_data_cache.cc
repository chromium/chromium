// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/non_recording_site_data_cache.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "components/performance_manager/persistence/site_data/noop_site_data_writer.h"
#include "components/performance_manager/persistence/site_data/site_data_writer.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"

namespace performance_manager {

NonRecordingSiteDataCache::NonRecordingSiteDataCache(
    SiteDataCache& data_cache_for_readers)
    : data_cache_for_readers_(data_cache_for_readers) {}

NonRecordingSiteDataCache::~NonRecordingSiteDataCache() = default;

std::unique_ptr<SiteDataReader> NonRecordingSiteDataCache::GetReaderForOrigin(
    const url::Origin& origin) {
  return data_cache_for_readers_->GetReaderForOrigin(origin);
}

std::unique_ptr<SiteDataWriter> NonRecordingSiteDataCache::GetWriterForOrigin(
    const url::Origin& origin) {
  // Return a fake data writer.
  SiteDataWriter* writer = new NoopSiteDataWriter();
  return base::WrapUnique(writer);
}

bool NonRecordingSiteDataCache::IsRecording() const {
  return false;
}

int NonRecordingSiteDataCache::Size() const {
  return 0;
}

const char* NonRecordingSiteDataCache::GetDataCacheName() {
  return "NonRecordingSiteDataCache";
}

std::vector<url::Origin> NonRecordingSiteDataCache::GetAllInMemoryOrigins() {
  return data_cache_for_readers_->GetAllInMemoryOrigins();
}

void NonRecordingSiteDataCache::GetDataStoreSize(
    DataStoreSizeCallback on_have_data) {
  data_cache_for_readers_->GetDataStoreSize(std::move(on_have_data));
}

bool NonRecordingSiteDataCache::GetDataForOrigin(
    const url::Origin& origin,
    bool* is_dirty,
    std::unique_ptr<SiteDataProto>* data) {
  return data_cache_for_readers_->GetDataForOrigin(origin, is_dirty, data);
}

}  // namespace performance_manager
