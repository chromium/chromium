// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_BACKEND_PARAMS_MANAGER_H_
#define COMPONENTS_PERSISTENT_CACHE_BACKEND_PARAMS_MANAGER_H_

#include <type_traits>
#include <unordered_map>

#include "base/component_export.h"
#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/persistent_cache/backend_params.h"

namespace persistent_cache {

// Use to retrieve or create BackendParams to open a PersistentCache. Existing
// params are cached so that they can be retrieved synchronously when possible.
//
// Example:
//  BackendParamsManager params_manager(GetPath());
//  params_manager.GetParamsSyncOrCreateAsync(BackendType::kSqlite, "key",
//      AccessRights::kReadOnly,
//      std::move(callback));
//  // `callback` called synchronously and result can be used right away.
//  // ... or
//  // `callback` will be invoked asynchronously to return result.
//
class COMPONENT_EXPORT(PERSISTENT_CACHE) BackendParamsManager {
 public:
  enum class AccessRights { kReadonly, kReadWrite };
  // `top_directory` is the where BackendParamsManager will try to find existing
  // files and create new ones.
  explicit BackendParamsManager(base::FilePath top_directory);
  ~BackendParamsManager();

  using CompletedCallback = base::OnceCallback<void(const BackendParams&)>;

  // Use to get backend params matching parameters through `callback`.
  void GetParamsSyncOrCreateAsync(BackendType backend_type,
                                  const std::string& key,
                                  AccessRights access_rights,
                                  CompletedCallback callback);

 private:
  struct BackendParamsKey {
    BackendType backend_type;
    std::string key;
  };

  struct BackendParamsKeyHash {
    BackendParamsKeyHash() = default;
    ~BackendParamsKeyHash() = default;
    std::size_t operator()(const BackendParamsKey& k) const {
      size_t hash = 0;
      return base::HashCombine(hash, k.backend_type, k.key);
    }
  };

  struct BackendParamsKeyEqual {
    bool operator()(const BackendParamsKey& lhs,
                    const BackendParamsKey& rhs) const {
      return lhs.backend_type == rhs.backend_type && lhs.key == rhs.key;
    }
  };

  static BackendParams CreateParamsSync(base::FilePath directory,
                                        BackendType backend_type,
                                        const std::string& key,
                                        AccessRights access_rights);

  // Saves params for later retrieval.
  void SaveParams(const std::string& key,
                  CompletedCallback callback,
                  BackendParams backend_params);

  base::HashingLRUCache<BackendParamsKey,
                        BackendParams,
                        BackendParamsKeyHash,
                        BackendParamsKeyEqual>
      backend_params_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  const base::FilePath top_directory_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BackendParamsManager> weak_factory_{this};
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_BACKEND_PARAMS_MANAGER_H_
