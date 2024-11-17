// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_CRX_CACHE_H_
#define COMPONENTS_UPDATE_CLIENT_CRX_CACHE_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/update_client/update_client_errors.h"

namespace update_client {

// CRX cache for storing and looking up the previous CRX for a given product
// so that it can be used in the Puffin patching process.
class CrxCache : public base::RefCountedThreadSafe<CrxCache> {
 public:
  CrxCache(const CrxCache&) = delete;
  CrxCache& operator=(const CrxCache&) = delete;

  // Constructs an CrxCache to facilitate lookups and updates on a given path.
  explicit CrxCache(std::optional<base::FilePath> path);

  // Requests a lookup of the previous CRX for the requested component given
  // `id` and `fp`.
  void Get(
      const std::string& id,
      const std::string& fp,
      base::OnceCallback<void(base::expected<base::FilePath, UnpackerError>)>
          callback);

  // Requests an entry for the current CRX to be added given the path `crx`,
  // `id` and `fp`. An entry with the same `id` is overwritten. This helps
  // to reduce cache size. This method takes ownership of the file, moves it,
  // and can only be accessed via the new path in the cache, given by `result`.
  // `callback` is called with the result.
  void Put(
      const base::FilePath& crx,
      const std::string& id,
      const std::string& fp,
      base::OnceCallback<void(base::expected<base::FilePath, UnpackerError>)>
          callback);

  // Removes any stale entries for the given product, should any exist. Runs as
  // a best effort and ignores any delete errors.
  void RemoveAll(const std::string& id);

 private:
  friend class base::RefCountedThreadSafe<CrxCache>;

  // Builds a full path to a crx in the cache, based on `id` and `fp`.
  base::FilePath BuildCrxFilePath(const std::string& id, const std::string& fp);

  virtual ~CrxCache();

  SEQUENCE_CHECKER(main_sequence_checker_);
  const std::optional<base::FilePath> crx_cache_root_path_;
  const scoped_refptr<base::TaskRunner> task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_CRX_CACHE_H_
