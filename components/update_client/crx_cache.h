// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_CRX_CACHE_H_
#define COMPONENTS_UPDATE_CLIENT_CRX_CACHE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "components/update_client/update_client_errors.h"

namespace update_client {

// CRX cache for storing and looking up the previous CRX for a given product
// so that it can be used in the Puffin patching process.
class CrxCache : public base::RefCountedThreadSafe<CrxCache> {
 public:
  CrxCache(const CrxCache&) = delete;
  CrxCache& operator=(const CrxCache&) = delete;

  // Contains the result of the Adding a new CRX.
  struct Options {
    explicit Options(const base::FilePath& crx_cache_root_path);

    // Path in the CRX cache to the newly added CRX.
    base::FilePath crx_cache_root_path;
  };

  // Constructs an CrxCache to facilitate lookups and updates on a given path.
  explicit CrxCache(const Options& options);

  // Contains the result of a CrxCache request.
  struct Result {
    Result() = default;

    // Unpack error: kNone indicates success.
    UnpackerError error = UnpackerError::kNone;

    // Path in the CRX cache to the newly added CRX.
    base::FilePath crx_cache_path;
  };

  // Returns true if the specified CRX is already cached.
  bool Contains(const std::string& id, const std::string& fp);

  // Requests a lookup of the previous CRX for the requested component given
  // `id` and `fp`.
  void Get(const std::string& id,
           const std::string& fp,
           base::OnceCallback<void(const Result& result)> callback);

  // Requests an entry for the current CRX to be added given the path `crx`,
  // `id` and `fp`. An entry with the same `id` is overwritten. This helps
  // to reduce cache size. This method takes ownership of the file, moves it,
  // and can only be accessed via the new path in the cache, given by `result`.
  // `callback` is called with the result.
  void Put(const base::FilePath& crx,
           const std::string& id,
           const std::string& fp,
           base::OnceCallback<void(const Result& result)> callback);

  // Removes any stale entries for the given product, should any exist. Runs as
  // a best effort and ignores any delete errors.
  void RemoveAll(const std::string& id);

 private:
  friend class base::RefCountedThreadSafe<CrxCache>;

  // Builds a full path to a crx in the cache, based on `id` and `fp`.
  base::FilePath BuildCrxFilePath(const std::string& id, const std::string& fp);

  // Processes a given Get request on the internal SequencedTaskRunner.
  Result ProcessGet(const std::string& id, const std::string& fp);

  // Processes a given Put request on the internal SequencedTaskRunner.
  Result ProcessPut(const base::FilePath& crx,
                    const std::string& id,
                    const std::string& fp);

  // Moves the CRX located at `original_crx_path` into its new location in the
  // cache located at `crx_cache_path`. Returns UnpackerError::kNone on success.
  UnpackerError MoveFileToCache(const base::FilePath& src_path,
                                const base::FilePath& dest_path);

  // Ensures the object is released on its sequence.
  void EndRequest(base::OnceCallback<void(const Result& result)> callback,
                  Result result);

  virtual ~CrxCache();

  SEQUENCE_CHECKER(main_sequence_checker_);
  base::FilePath crx_cache_root_path_;
  scoped_refptr<base::TaskRunner> task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_CRX_CACHE_H_
