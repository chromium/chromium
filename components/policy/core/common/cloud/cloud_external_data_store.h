// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_EXTERNAL_DATA_STORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_EXTERNAL_DATA_STORE_H_

#include <stddef.h>

#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class ResourceCache;

// Stores external data referenced by policies. Data is keyed by (key, hash),
// where |key| is an opaque string that comes from the policy referencing the
// data, and |hash| is the data's SHA256 hash. Outdated entries are removed by
// calling Prune() with the list of (key, hash) entries that are to be kept.
// Instances of this class may be created on any thread and may share the same
// cache, however:
// * After creation, the cache and all stores using it must always be accessed
//   via the same |task_runner| only.
// * Stores sharing a cache must use different cache_keys to avoid namespace
//   overlaps.
// * The cache must outlive all stores using it.
class POLICY_EXPORT CloudExternalDataStore {
 public:
  CloudExternalDataStore(const std::string& cache_key,
                         scoped_refptr<base::SequencedTaskRunner> task_runner,
                         ResourceCache* cache);
  CloudExternalDataStore(const CloudExternalDataStore&) = delete;
  CloudExternalDataStore& operator=(const CloudExternalDataStore&) = delete;
  ~CloudExternalDataStore();

  // Removes all entries from the store whose (policy, hash) pair is not found
  // in |metadata|.
  using PruningData = std::vector<std::pair<std::string, std::string>>;
  void Prune(const PruningData& key_hash_pairs_to_keep);

  // Stores |data| under (key, hash). Returns file path if the store
  // succeeded, and empty FilePath otherwise.
  base::FilePath Store(const std::string& key,
                       const std::string& hash,
                       const std::string& data);

  // Loads the entry at (key, hash) into |data|, verifies that it does not
  // exceed |max_size| and matches the expected |hash|, then returns true.
  // Returns empty FilePath if no entry is found at (key, hash), there is a
  // problem during the load, the entry exceeds |max_size| or does not match
  // |hash|, and file path otherwise.
  base::FilePath Load(const std::string& key,
                      const std::string& hash,
                      size_t max_size,
                      std::string* data);

 private:
  std::string cache_key_;

  // Task runner that |this| runs on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  raw_ptr<ResourceCache, DanglingUntriaged> cache_;  // Not owned.
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_EXTERNAL_DATA_STORE_H_
