// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_STORAGE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_STORAGE_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <set>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/storage_usage_info.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}

namespace browsing_data {

// This class fetches local storage information and provides a
// means to delete the data associated with an origin.
class LocalStorageHelper : public base::RefCounted<LocalStorageHelper> {
 public:
  using FetchCallback =
      base::OnceCallback<void(const std::list<content::StorageUsageInfo>&)>;

  explicit LocalStorageHelper(content::BrowserContext* context);

  // Starts the fetching process, which will notify its completion via
  // callback. This must be called only in the UI thread.
  virtual void StartFetching(FetchCallback callback);

  // Deletes the local storage for the |origin|. |callback| is called when
  // the deletion is sent to the database and |StartFetching()| doesn't return
  // entries for |origin_url| anymore.
  virtual void DeleteOrigin(const url::Origin& origin,
                            base::OnceClosure callback);

 protected:
  friend class base::RefCounted<LocalStorageHelper>;
  virtual ~LocalStorageHelper();

  content::DOMStorageContext* dom_storage_context_;  // Owned by the context

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalStorageHelper);
};

// This class is a thin wrapper around LocalStorageHelper that does
// not fetch its information from the local storage context, but gets them
// passed by a call when accessed.
class CannedLocalStorageHelper : public LocalStorageHelper {
 public:
  explicit CannedLocalStorageHelper(content::BrowserContext* context);

  // Add a local storage to the set of canned local storages that is returned
  // by this helper.
  void Add(const url::Origin& origin_url);

  // Clear the list of canned local storages.
  void Reset();

  // True if no local storages are currently stored.
  bool empty() const;

  // Returns the number of local storages currently stored.
  size_t GetCount() const;

  // Returns the set of origins that use local storage.
  const std::set<url::Origin>& GetOrigins() const;

  // LocalStorageHelper implementation.
  void StartFetching(FetchCallback callback) override;
  void DeleteOrigin(const url::Origin& origin,
                    base::OnceClosure callback) override;

 private:
  ~CannedLocalStorageHelper() override;

  std::set<url::Origin> pending_origins_;

  DISALLOW_COPY_AND_ASSIGN(CannedLocalStorageHelper);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_STORAGE_HELPER_H_
