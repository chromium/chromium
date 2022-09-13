// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_H_

#include <memory>

#include "components/performance_manager/persistence/site_data/site_data_writer.h"
#include "components/performance_manager/persistence/site_data/tab_visibility.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"
#include "url/origin.h"

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
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_H_
