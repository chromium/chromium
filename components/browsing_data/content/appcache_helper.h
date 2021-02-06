// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_APPCACHE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_APPCACHE_HELPER_H_

#include <stddef.h>

#include <list>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/appcache_service.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
struct StorageUsageInfo;
}

namespace browsing_data {

// This class fetches appcache information on behalf of a caller
// on the UI thread.
class AppCacheHelper : public base::RefCountedThreadSafe<AppCacheHelper> {
 public:
  using FetchCallback =
      base::OnceCallback<void(const std::list<content::StorageUsageInfo>&)>;

  // If appcache_service is null, then StartFetching will return no results
  // and DeleteAppCaches will silently be a noop.
  explicit AppCacheHelper(content::AppCacheService* appcache_service);

  virtual void StartFetching(FetchCallback completion_callback);
  virtual void DeleteAppCaches(const url::Origin& origin_url);

 protected:
  friend class base::RefCountedThreadSafe<AppCacheHelper>;
  virtual ~AppCacheHelper();

 private:
  // Owned by the profile.
  content::AppCacheService* appcache_service_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheHelper);
};

// This class is a thin wrapper around AppCacheHelper that does not
// fetch its information from the appcache service, but gets them passed when
// called on access.
class CannedAppCacheHelper : public AppCacheHelper {
 public:
  explicit CannedAppCacheHelper(content::AppCacheService* appcache_service);

  // Add an appcache to the set of canned caches that is returned by this
  // helper.
  void Add(const url::Origin& origin);

  // Clears the list of canned caches.
  void Reset();

  // True if no appcaches are currently stored.
  bool empty() const;

  // Returns the number of app cache resources.
  size_t GetCount() const;

  // Returns the set or origins with app caches.
  const std::set<url::Origin>& GetOrigins() const;

  // AppCacheHelper methods.
  void StartFetching(FetchCallback callback) override;
  void DeleteAppCaches(const url::Origin& origin) override;

 private:
  ~CannedAppCacheHelper() override;

  std::set<url::Origin> pending_origins_;

  DISALLOW_COPY_AND_ASSIGN(CannedAppCacheHelper);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_APPCACHE_HELPER_H_
