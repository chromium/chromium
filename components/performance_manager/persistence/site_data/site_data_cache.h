// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/byte_size.h"
#include "base/functional/callback_forward.h"
#include "components/performance_manager/persistence/site_data/site_data_writer.h"
#include "components/performance_manager/persistence/site_data/tab_visibility.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"
#include "url/origin.h"

class SiteDataProto;

namespace performance_manager {

// Pure virtual interface for a site data cache.
class SiteDataCache {
 public:
  SiteDataCache() = default;

  SiteDataCache(const SiteDataCache&) = delete;
  SiteDataCache& operator=(const SiteDataCache&) = delete;

  virtual ~SiteDataCache() = default;

  // Returns a SiteDataReader for the given origin.
  virtual std::unique_ptr<SiteDataReader> GetReaderForOrigin(
      const url::Origin& origin) = 0;

  // Returns a SiteDataWriter for the given origin.
  virtual std::unique_ptr<SiteDataWriter> GetWriterForOrigin(
      const url::Origin& origin) = 0;

  // Indicate if the SiteDataWriter served by this data cache actually persists
  // information.
  virtual bool IsRecording() const = 0;

  // Returns the number of element in the cache.
  virtual int Size() const = 0;

  // The following functions expose diagnostic information for the discards
  // WebUI.

  // Returns the name of the data cache, which should uniquely identify the kind
  // of storage it implements.
  virtual const char* GetDataCacheName() = 0;

  // Retrieves the origins that are currently represented by in-memory data.
  virtual std::vector<url::Origin> GetAllInMemoryOrigins() = 0;

  // Retrieves the number of rows and the on-disk size of the store. Invokes
  // the |on_have_data| callback once the data has been collected, or once it's
  // determined that the data can't be retrieved.
  // On callback |num_rows| is the number of rows in the database, or -1 if
  // the number can't be determined. |on_disk_size| is the on-disk size of
  // the database, or nullopt if the on-disk size can't be determined.
  using DataStoreSizeCallback =
      base::OnceCallback<void(std::optional<int64_t> num_rows,
                              std::optional<base::ByteSize> on_disk_size)>;
  virtual void GetDataStoreSize(DataStoreSizeCallback on_have_data) = 0;

  // Retrieves the in-memory data for a given origin.
  // On return |data| contains the available data for |origin| if available,
  // and |is_dirty| is true if the entry needs flushing to disk.
  // Returns true if an entry exists for |origin|.
  virtual bool GetDataForOrigin(const url::Origin& origin,
                                bool* is_dirty,
                                std::unique_ptr<SiteDataProto>* data) = 0;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_H_
