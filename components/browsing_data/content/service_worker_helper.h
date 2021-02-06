// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_SERVICE_WORKER_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_SERVICE_WORKER_HELPER_H_

#include <stddef.h>

#include <list>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/service_worker_context.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
struct StorageUsageInfo;
}

namespace browsing_data {

// ServiceWorkerHelper is an interface for classes dealing with
// aggregating and deleting browsing data stored for Service Workers -
// registrations, scripts, and caches.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI thread at
// some later point.
class ServiceWorkerHelper
    : public base::RefCountedThreadSafe<ServiceWorkerHelper> {
 public:
  using FetchCallback =
      base::OnceCallback<void(const std::list<content::StorageUsageInfo>&)>;

  // Create a ServiceWorkerHelper instance for the Service Workers
  // stored in |context|'s associated profile's user data directory.
  explicit ServiceWorkerHelper(content::ServiceWorkerContext* context);

  // Starts the fetching process, which will notify its completion via
  // |callback|. This must be called only in the UI thread.
  virtual void StartFetching(FetchCallback callback);
  // Requests the Service Worker data for an origin be deleted.
  virtual void DeleteServiceWorkers(const url::Origin& origin);

 protected:
  virtual ~ServiceWorkerHelper();

  // Owned by the profile.
  content::ServiceWorkerContext* service_worker_context_;

 private:
  friend class base::RefCountedThreadSafe<ServiceWorkerHelper>;

  // Enumerates all Service Worker instances on the service worker core thread.
  void FetchServiceWorkerUsageInfoOnCoreThread(FetchCallback callback);

  // Deletes Service Workers for an origin on the service worker core thread.
  void DeleteServiceWorkersOnCoreThread(const url::Origin& origin);

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHelper);
};

// This class is an implementation of ServiceWorkerHelper that does
// not fetch its information from the Service Worker context, but is passed the
// info by call when accessed.
class CannedServiceWorkerHelper : public ServiceWorkerHelper {
 public:
  explicit CannedServiceWorkerHelper(content::ServiceWorkerContext* context);

  // Add a Service Worker to the set of canned Service Workers that is
  // returned by this helper.
  void Add(const url::Origin& origin);

  // Clear the list of canned Service Workers.
  void Reset();

  // True if no Service Workers are currently stored.
  bool empty() const;

  // Returns the number of currently stored Service Workers.
  size_t GetCount() const;

  // Returns the current list of Service Workers.
  const std::set<url::Origin>& GetOrigins() const;

  // ServiceWorkerHelper methods.
  void StartFetching(FetchCallback callback) override;
  void DeleteServiceWorkers(const url::Origin& origin) override;

 private:
  ~CannedServiceWorkerHelper() override;

  std::set<url::Origin> pending_origins_;

  DISALLOW_COPY_AND_ASSIGN(CannedServiceWorkerHelper);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_SERVICE_WORKER_HELPER_H_
