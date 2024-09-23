// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/non_recording_site_data_cache.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "components/performance_manager/persistence/site_data/noop_site_data_writer.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/persistence/site_data/site_data_writer.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"

namespace performance_manager {

NonRecordingSiteDataCache::NonRecordingSiteDataCache(
    const std::string& browser_context_id,
    SiteDataCacheInspector* data_cache_inspector,
    SiteDataCache* data_cache_for_readers)
    : data_cache_for_readers_(data_cache_for_readers),
      data_cache_inspector_(data_cache_inspector),
      browser_context_id_(browser_context_id) {
  DCHECK(data_cache_for_readers_);
  // Register the debug interface against the browser context. The factory
  // should always exist by the time this is called, since it is created once
  // there's a browser context with keyed services enabled.
  auto* factory = SiteDataCacheFactory::GetInstance();
  CHECK(factory);
  factory->SetDataCacheInspectorForBrowserContext(this, browser_context_id_);
}

NonRecordingSiteDataCache::~NonRecordingSiteDataCache() {
  SiteDataCacheFactory::GetInstance()->SetDataCacheInspectorForBrowserContext(
      nullptr, browser_context_id_);
}

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
  if (!data_cache_inspector_)
    return std::vector<url::Origin>();

  return data_cache_inspector_->GetAllInMemoryOrigins();
}

void NonRecordingSiteDataCache::GetDataStoreSize(
    DataStoreSizeCallback on_have_data) {
  if (!data_cache_inspector_) {
    std::move(on_have_data).Run(std::nullopt, std::nullopt);
    return;
  }

  data_cache_inspector_->GetDataStoreSize(std::move(on_have_data));
}

bool NonRecordingSiteDataCache::GetDataForOrigin(
    const url::Origin& origin,
    bool* is_dirty,
    std::unique_ptr<SiteDataProto>* data) {
  if (!data_cache_inspector_)
    return false;

  return data_cache_inspector_->GetDataForOrigin(origin, is_dirty, data);
}

NonRecordingSiteDataCache* NonRecordingSiteDataCache::GetDataCache() {
  return this;
}

}  // namespace performance_manager
