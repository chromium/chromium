// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_STORE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_STORE_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/performance_manager/persistence/site_data/site_data.pb.h"
#include "url/origin.h"

class SiteDataProto;

namespace performance_manager {

// Interface for an on-disk SiteData store.
class SiteDataStore {
 public:
  // Callback to call once the initialization from the store has completed,
  // |site_data_proto| should be equal to std::nullopt if the initialization
  // has failed.
  using ReadSiteDataFromStoreCallback =
      base::OnceCallback<void(std::optional<SiteDataProto> site_data_proto)>;
  using GetStoreSizeCallback =
      base::OnceCallback<void(std::optional<int64_t> num_rows,
                              std::optional<int64_t> on_disk_size_kb)>;

  SiteDataStore() = default;
  virtual ~SiteDataStore() = default;

  // Checks the if there's an entry with the key |origin| and if pass the
  // corresponding proto to |callback|.
  virtual void ReadSiteDataFromStore(
      const url::Origin& origin,
      ReadSiteDataFromStoreCallback callback) = 0;

  // Store an entry in the store, create it if it doesn't exist and update it it
  // it does.
  virtual void WriteSiteDataIntoStore(const url::Origin& origin,
                                      const SiteDataProto& site_data_proto) = 0;

  // Removes some entries from the store.
  virtual void RemoveSiteDataFromStore(
      const std::vector<url::Origin>& site_origins) = 0;

  // Clear the store, removes every entries that it contains.
  virtual void ClearStore() = 0;

  // Retrieve the size of the store.
  virtual void GetStoreSize(GetStoreSizeCallback callback) = 0;

  // Set a callback that will be called once the data store has been fully
  // initialized.
  virtual void SetInitializationCallbackForTesting(
      base::OnceClosure callback) = 0;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_STORE_H_
