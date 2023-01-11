// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_SHARED_WORKER_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_SHARED_WORKER_HELPER_H_

#include <stddef.h>

#include <list>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class StoragePartition;
}  // namespace content

namespace browsing_data {

// Shared Workers don't persist browsing data outside of the regular storage
// mechanisms of their origin, however, we need the helper anyways to be able
// to show them as cookie-like data.
class SharedWorkerHelper
    : public base::RefCountedThreadSafe<SharedWorkerHelper> {
 public:
  // Contains information about a Shared Worker.
  struct SharedWorkerInfo {
    SharedWorkerInfo(const GURL& worker,
                     const std::string& name,
                     const blink::StorageKey& storage_key);
    SharedWorkerInfo(const SharedWorkerInfo& other);
    ~SharedWorkerInfo();

    bool operator<(const SharedWorkerInfo& other) const;

    GURL worker;
    std::string name;
    blink::StorageKey storage_key;
  };

  using FetchCallback =
      base::OnceCallback<void(const std::list<SharedWorkerInfo>&)>;

  explicit SharedWorkerHelper(content::StoragePartition* storage_partition);

  SharedWorkerHelper(const SharedWorkerHelper&) = delete;
  SharedWorkerHelper& operator=(const SharedWorkerHelper&) = delete;

  // Starts the fetching process returning the list of shared workers, which
  // will notify its completion via |callback|. This must be called only in the
  // UI thread.
  virtual void StartFetching(FetchCallback callback);

  // Requests the given Shared Worker to be deleted.
  virtual void DeleteSharedWorker(const GURL& worker,
                                  const std::string& name,
                                  const blink::StorageKey& storage_key);

 protected:
  virtual ~SharedWorkerHelper();

 private:
  friend class base::RefCountedThreadSafe<SharedWorkerHelper>;

  raw_ptr<content::StoragePartition> storage_partition_;
};

// This class is an implementation of SharedWorkerHelper that does
// not fetch its information from the Shared Worker context, but is passed the
// info as a parameter.
class CannedSharedWorkerHelper : public SharedWorkerHelper {
 public:
  explicit CannedSharedWorkerHelper(
      content::StoragePartition* storage_partition);

  CannedSharedWorkerHelper(const CannedSharedWorkerHelper&) = delete;
  CannedSharedWorkerHelper& operator=(const CannedSharedWorkerHelper&) = delete;

  // Adds Shared Worker to the set of canned Shared Workers that is returned by
  // this helper.
  void AddSharedWorker(const GURL& worker,
                       const std::string& name,
                       const blink::StorageKey& storage_key);

  // Clears the list of canned Shared Workers.
  void Reset();

  // True if no Shared Workers are currently in the set.
  bool empty() const;

  // Returns the number of Shared Workers in the set.
  size_t GetSharedWorkerCount() const;

  // Returns the current list of Shared Workers.
  const std::set<CannedSharedWorkerHelper::SharedWorkerInfo>&
  GetSharedWorkerInfo() const;

  // SharedWorkerHelper methods.
  void StartFetching(FetchCallback callback) override;
  void DeleteSharedWorker(const GURL& worker,
                          const std::string& name,
                          const blink::StorageKey& storage_key) override;

 private:
  ~CannedSharedWorkerHelper() override;

  std::set<SharedWorkerInfo> pending_shared_worker_info_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_SHARED_WORKER_HELPER_H_
