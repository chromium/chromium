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
#include "base/gtest_prod_util.h"
#include "base/hash/hash.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/persistent_cache/backend_params.h"

namespace persistent_cache {

struct FootprintReductionResult {
  int64_t current_footprint = 0;
  int64_t number_of_bytes_deleted = 0;
};

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

  // Use to get backend params matching parameters directly or through
  // `callback`. An invalid BackendParams instance is returned if `key` does
  // not respect restrictions. Keys used in this class should be as short as
  // possible to minimize the risk of them being too long to be used in a file
  // path. Not all characters are allowed. See
  // `GetAllAllowedCharactersInKeysForTesting`.
  void GetParamsSyncOrCreateAsync(BackendType backend_type,
                                  const std::string& key,
                                  AccessRights access_rights,
                                  CompletedCallback callback);
  BackendParams GetOrCreateParamsSync(BackendType backend_type,
                                      const std::string& key,
                                      AccessRights access_rights);

  // Delete all managed files.
  void DeleteAllFiles();

  // Use to reduce the total size of files on disk until it's equal or smaller
  // than `target_footprint`. Use when enforcing a quota or proactively saving
  // space. If the goal is to get rid of all files use `DeleteAllFiles()`
  // instead. Returns the number of bytes deleted.
  FootprintReductionResult BringDownTotalFootprintOfFiles(
      int64_t target_footprint);

  // Use to get a string containing all characters supported in keys.
  static std::string GetAllAllowedCharactersInKeysForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(BackendParamsManager, InvalidCharactersHandled);
  FRIEND_TEST_ALL_PREFIXES(BackendParamsManager, KeyToFileName);
  FRIEND_TEST_ALL_PREFIXES(BackendParamsManager, KeyToFileNameIsReversible);

  // Function that simplifies a key string into a form suitable to be used as a
  // file name by this class. The function also takes care of lightly
  // obfuscating the value. This is not a security measure but more a way to
  // underline the fact that the files are not meant to be discovered and
  // modified by third parties.
  //
  // On Windows some file names are reserved
  // (https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file#file-and-directory-names).
  // As such the result of this function should always be used by appending a
  // file extension as provided by this class to avoid using problems.
  static std::string FileNameFromKey(const std::string& key);

  // Inverse of `FileNameFromKey`. Will return an empty string on an invalid
  // filename which needs to be handled.
  static std::string KeyFromFileName(const std::string& key);

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
                                        const std::string& filename,
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
