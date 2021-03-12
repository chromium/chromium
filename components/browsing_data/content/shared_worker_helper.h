// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_SHARED_WORKER_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_SHARED_WORKER_HELPER_H_

#include <stddef.h>

#include <list>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class StoragePartition;
class ResourceContext;
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
                     const url::Origin& constructor_origin);
    SharedWorkerInfo(const SharedWorkerInfo& other);
    ~SharedWorkerInfo();

    bool operator<(const SharedWorkerInfo& other) const;

    GURL worker;
    std::string name;
    url::Origin constructor_origin;
  };

  using FetchCallback =
      base::OnceCallback<void(const std::list<SharedWorkerInfo>&)>;

  SharedWorkerHelper(content::StoragePartition* storage_partition,
                     content::ResourceContext* resource_context);

  // Starts the fetching process returning the list of shared workers, which
  // will notify its completion via |callback|. This must be called only in the
  // UI thread.
  virtual void StartFetching(FetchCallback callback);

  // Requests the given Shared Worker to be deleted.
  virtual void DeleteSharedWorker(const GURL& worker,
                                  const std::string& name,
                                  const url::Origin& constructor_origin);

 protected:
  virtual ~SharedWorkerHelper();

 private:
  friend class base::RefCountedThreadSafe<SharedWorkerHelper>;

  content::StoragePartition* storage_partition_;
  content::ResourceContext* resource_context_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHelper);
};

// This class is an implementation of SharedWorkerHelper that does
// not fetch its information from the Shared Worker context, but is passed the
// info as a parameter.
class CannedSharedWorkerHelper : public SharedWorkerHelper {
 public:
  CannedSharedWorkerHelper(content::StoragePartition* storage_partition,
                           content::ResourceContext* resource_context);

  // Adds Shared Worker to the set of canned Shared Workers that is returned by
  // this helper.
  void AddSharedWorker(const GURL& worker,
                       const std::string& name,
                       const url::Origin& constructor_origin);

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
                          const url::Origin& constructor_origin) override;

 private:
  ~CannedSharedWorkerHelper() override;

  std::set<SharedWorkerInfo> pending_shared_worker_info_;

  DISALLOW_COPY_AND_ASSIGN(CannedSharedWorkerHelper);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_SHARED_WORKER_HELPER_H_
