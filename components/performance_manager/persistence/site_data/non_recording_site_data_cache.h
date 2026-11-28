// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_NON_RECORDING_SITE_DATA_CACHE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_NON_RECORDING_SITE_DATA_CACHE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "components/performance_manager/persistence/site_data/site_data_cache.h"

namespace performance_manager {

// Implementation of a SiteDataCache that ensures that no data gets persisted.
//
// This class should be used for off the record profiles.
class NonRecordingSiteDataCache : public SiteDataCache {
 public:
  // `data_cache_for_readers` is used to create the readers served by this
  // cache, and to serve the diagnostic information. E.g. during an incognito
  // session it should point to the data cache used by the parent session. It
  // must outlive this object.
  explicit NonRecordingSiteDataCache(SiteDataCache& data_cache_for_readers);

  NonRecordingSiteDataCache(const NonRecordingSiteDataCache&) = delete;
  NonRecordingSiteDataCache& operator=(const NonRecordingSiteDataCache&) =
      delete;

  ~NonRecordingSiteDataCache() override;

  // SiteDataCache:
  std::unique_ptr<SiteDataReader> GetReaderForOrigin(
      const url::Origin& origin) override;
  std::unique_ptr<SiteDataWriter> GetWriterForOrigin(
      const url::Origin& origin) override;
  bool IsRecording() const override;
  int Size() const override;
  const char* GetDataCacheName() override;
  std::vector<url::Origin> GetAllInMemoryOrigins() override;
  void GetDataStoreSize(DataStoreSizeCallback on_have_data) override;
  bool GetDataForOrigin(const url::Origin& origin,
                        bool* is_dirty,
                        std::unique_ptr<SiteDataProto>* data) override;

 private:
  const raw_ref<SiteDataCache> data_cache_for_readers_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_NON_RECORDING_SITE_DATA_CACHE_H_
