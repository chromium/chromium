// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_CRX_CACHE_H_
#define COMPONENTS_UPDATE_CLIENT_CRX_CACHE_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/types/expected.h"

namespace base {
class FilePath;
}

namespace update_client {

enum class UnpackerError;
class CrxCacheSynchronous;

// A CRX cache is a filesystem-backed cache of files. It keeps files inside a
// particular directory, and uses a metadata file within that directory to
// track data about the entries.
//
// When constructed, the cache loads the existing metadata file in the provided
// directory and immediately deletes any files in the directory that don't have
// a corresponding entry in the metadata file. If there is no metadata file,
// the cache does not delete anything. The metadata is created after the first
// Put.
//
// It is unsafe to instantiate multiple CrxCaches with a shared `cache_root`.
//
// The cache maintains only a single cached element per app ID.
class CrxCache : public base::RefCountedThreadSafe<CrxCache> {
 public:
  CrxCache(const CrxCache&) = delete;
  CrxCache& operator=(const CrxCache&) = delete;

  // Constructs a CrxCache that operates in `cache_root`. If `cache_root` is
  // not provided, the CrxCache returns errors from most methods. Deletes any
  // contents in `cache_root` that do not belong to the cache.
  explicit CrxCache(std::optional<base::FilePath> cache_root);

  // Returns a multimap of app IDs to hashes that are present in the cache.
  void ListHashesByAppId(
      base::OnceCallback<void(const std::multimap<std::string, std::string>&)>
          callback) const;

  // Returns a cached element with a matching hash. If there is no element
  // cached, returns base::unexpected(kCrxCacheFileNotCached). O(1) lookup.
  void GetByHash(
      const std::string& hash,
      base::OnceCallback<void(base::expected<base::FilePath, UnpackerError>)>
          callback) const;

  // Adds `file` to the cache. Any entries with the same `app_id` are first
  // evicted. The `file` is moved into the cache, `file`'s parent directory (now
  // expected to be empty) is deleted, and the new path to the file is returned.
  // Hashes should be in ASCII. O(N) time.
  void Put(
      const base::FilePath& file,
      const std::string& app_id,
      const std::string& hash,
      base::OnceCallback<void(base::expected<base::FilePath, UnpackerError>)>
          callback);

  // Removes all entries associated with a particular app ID. O(N) time.
  void RemoveAll(const std::string& app_id, base::OnceClosure callback);

  // Removes all entries that are not associated with any of the listed
  // app IDs. O(N+M) time.
  void RemoveIfNot(const std::vector<std::string>& app_ids,
                   base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<CrxCache>;

  // Destroying the CrxCache schedules a write of the metadata to disk. The
  // write will block shutdown, but callers can't depend on it happening before
  // the destructor returns, so it is unsafe to construct a new CrxCache with
  // the same cache_root before the process shuts down.
  virtual ~CrxCache();

  SEQUENCE_CHECKER(sequence_checker_);
  base::SequenceBound<CrxCacheSynchronous> delegate_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_CRX_CACHE_H_
