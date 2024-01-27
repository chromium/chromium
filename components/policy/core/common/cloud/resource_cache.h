// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_RESOURCE_CACHE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_RESOURCE_CACHE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

// Manages storage of data at a given path. The data is keyed by a key and
// a subkey, and can be queried by (key, subkey) or (key) lookups.
// The contents of the cache have to be manually cleared using Delete() or
// Purge*().
// The class can be instantiated on any thread but from then on, it must be
// accessed via the |task_runner| only. The |task_runner| must support file I/O.
// The class needs to have exclusive control on cache directory since it should
// know about all files changes for correct recalculating cache directory size.
class POLICY_EXPORT ResourceCache {
 public:
  ResourceCache(const base::FilePath& cache_path,
                scoped_refptr<base::SequencedTaskRunner> task_runner,
                const std::optional<int64_t> max_cache_size);
  ResourceCache(const ResourceCache&) = delete;
  ResourceCache& operator=(const ResourceCache&) = delete;
  virtual ~ResourceCache();

  // Stores |data| under (key, subkey). Returns file path if the store
  // succeeded, and empty FilePath otherwise.
  base::FilePath Store(const std::string& key,
                       const std::string& subkey,
                       const std::string& data);

  // Loads the contents of (key, subkey) into |data| and returns true. Returns
  // empty FilePath if (key, subkey) isn't found or if there is a problem
  // reading the data, and file path otherwise.
  base::FilePath Load(const std::string& key,
                      const std::string& subkey,
                      std::string* data);

  // Loads all the subkeys of |key| into |contents|.
  void LoadAllSubkeys(const std::string& key,
                      std::map<std::string, std::string>* contents);

  // Deletes (key, subkey).
  void Delete(const std::string& key, const std::string& subkey);

  // Deletes all the subkeys of |key|.
  void Clear(const std::string& key);

  // Deletes the subkeys of |key| for which the |filter| returns true.
  typedef base::RepeatingCallback<bool(const std::string&)> SubkeyFilter;
  void FilterSubkeys(const std::string& key, const SubkeyFilter& filter);

  // Deletes all keys not in |keys_to_keep|, along with their subkeys.
  void PurgeOtherKeys(const std::set<std::string>& keys_to_keep);

  // Deletes all the subkeys of |key| not in |subkeys_to_keep|.
  void PurgeOtherSubkeys(const std::string& key,
                         const std::set<std::string>& subkeys_to_keep);

 private:
  // Points |path| at the cache directory for |key| and returns whether the
  // directory exists. If |allow_create| is |true|, the directory is created if
  // it did not exist yet.
  bool VerifyKeyPath(const std::string& key,
                     bool allow_create,
                     base::FilePath* path);

  // Points |subkey_path| at the file in which data for (key, subkey) should be
  // stored and returns whether the parent directory of this file exists. If
  // |allow_create_key| is |true|, the directory is created if it did not exist
  // yet. This method does not check whether the file at |subkey_path| exists or
  // not.
  bool VerifyKeyPathAndGetSubkeyPath(const std::string& key,
                                     bool allow_create_key,
                                     const std::string& subkey,
                                     base::FilePath* subkey_path);

  // Initializes |current_cache_size_| with the size of cache directory.
  // It's called once from constructor and is executed in provided
  // |task_runner_|.
  void InitCurrentCacheSize();

  // Writes the given data into the file in the cache directory and updates
  // |current_cache_size_| accordingly.
  // Deletes the file before writing to it. This ensures that the write does not
  // follow a symlink planted at |subkey_path|, clobbering a file outside the
  // cache directory. The mechanism is meant to foil file-system-level attacks
  // where a symlink is planted in the cache directory before Chrome has
  // started. An attacker controlling a process running concurrently with Chrome
  // would be able to race against the protection by re-creating the symlink
  // between these two calls. There is nothing in file_util that could be used
  // to protect against such races, especially as the cache is cross-platform
  // and therefore cannot use any POSIX-only tricks.
  // Note that |path| must belong to |cache_dir_|.
  bool WriteCacheFile(const base::FilePath& path, const std::string& data);

  // Deletes the given path in the cache directory and updates
  // |current_cache_size_| accordingly.
  // Note that |path| must belong to |cache_dir_|.
  bool DeleteCacheFile(const base::FilePath& path, bool recursive);

  // Returns the size of given directory or file in the cache directory skipping
  // symlinks or 0 if any error occurred.
  // We couldn't use |base::ComputeDirectorySize| here because it doesn't allow
  // to skip symlinks.
  // Skipping symlinks is important here to prevent exploit which puts symlink
  // to e.g. root directory and freezes this thread since traversing the whole
  // filesystem takes a while.
  // Note that |path| must belong to |cache_dir_|.
  int64_t GetCacheDirectoryOrFileSize(const base::FilePath& path) const;

  base::FilePath cache_dir_;

  // Task runner that |this| runs on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Maximum size of the cache directory.
  const std::optional<int64_t> max_cache_size_;

  // Note that this variable could be created on any thread, but is modified
  // only on the |task_runner_| thread.
  int64_t current_cache_size_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_RESOURCE_CACHE_H_
