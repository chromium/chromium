// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/persistence/site_data/site_data_cache.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_inspector.h"
#include "components/performance_manager/persistence/site_data/site_data_impl.h"

namespace performance_manager {

// Implementation of a SiteDataCache that serves normal reader and writers.
//
// This class should never be used for off the record profiles, the
// NonRecordingSiteDataCache class should be used instead.
class SiteDataCacheImpl : public SiteDataCache,
                          public SiteDataCacheInspector,
                          public internal::SiteDataImpl::OnDestroyDelegate {
 public:
  using SiteDataMap =
      base::flat_map<url::Origin,
                     raw_ptr<internal::SiteDataImpl, CtnExperimental>>;

  SiteDataCacheImpl(const std::string& browser_context_id,
                    const base::FilePath& browser_context_path);

  SiteDataCacheImpl(const SiteDataCacheImpl&) = delete;
  SiteDataCacheImpl& operator=(const SiteDataCacheImpl&) = delete;

  ~SiteDataCacheImpl() override;

  // SiteDataCache:
  std::unique_ptr<SiteDataReader> GetReaderForOrigin(
      const url::Origin& origin) override;
  std::unique_ptr<SiteDataWriter> GetWriterForOrigin(
      const url::Origin& origin) override;
  bool IsRecording() const override;
  int Size() const override;

  const SiteDataMap& origin_data_map_for_testing() const {
    return origin_data_map_;
  }

  // NOTE: This should be called before creating any SiteDataImpl object (this
  // doesn't update the data store used by these objects).
  void SetDataStoreForTesting(std::unique_ptr<SiteDataStore> data_store) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    data_store_ = std::move(data_store);
  }

  // SiteDataCacheImplInspector:
  const char* GetDataCacheName() override;
  std::vector<url::Origin> GetAllInMemoryOrigins() override;
  void GetDataStoreSize(DataStoreSizeCallback on_have_data) override;
  bool GetDataForOrigin(const url::Origin& origin,
                        bool* is_dirty,
                        std::unique_ptr<SiteDataProto>* data) override;
  SiteDataCacheImpl* GetDataCache() override;

  // Remove a specific set of entries from the cache and the on-disk store.
  // Virtual for testing.
  virtual void ClearSiteDataForOrigins(
      const std::vector<url::Origin>& origins_to_remove);

  // Clear the data cache and the on-disk store.
  // Virtual for testing.
  virtual void ClearAllSiteData();

  // Set a callback that will be called once the data store backing this cache
  // has been fully initialized.
  void SetInitializationCallbackForTesting(base::OnceClosure callback);

 protected:
  // Version of the constructor that doesn't create the data store, for testing
  // purposes only.
  explicit SiteDataCacheImpl(const std::string& browser_context_id);

 private:
  // Returns a pointer to the SiteDataImpl object associated with |origin|,
  // create one and add it to |origin_data_map_| if it doesn't exist.
  internal::SiteDataImpl* GetOrCreateFeatureImpl(const url::Origin& origin);

  // internal::SiteDataImpl::OnDestroyDelegate:
  void OnSiteDataImplDestroyed(internal::SiteDataImpl* impl) override;

  // Map an origin to a SiteDataImpl pointer.
  SiteDataMap origin_data_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<SiteDataStore> data_store_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The ID of the browser context this data store is associated with.
  const std::string browser_context_id_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SiteDataCacheImpl> weak_factory_{this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_IMPL_H_
