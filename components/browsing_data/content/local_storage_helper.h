// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_STORAGE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_STORAGE_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/storage_usage_info.h"

namespace content {
class StoragePartition;
}  // namespace content

namespace browsing_data {

// This class fetches local storage information, but should be removed in favor
// of directly fetching from the `StoragePartition`.
class LocalStorageHelper : public base::RefCounted<LocalStorageHelper> {
 public:
  using FetchCallback =
      base::OnceCallback<void(const std::list<content::StorageUsageInfo>&)>;

  explicit LocalStorageHelper(content::StoragePartition* storage_partition);

  LocalStorageHelper(const LocalStorageHelper&) = delete;
  LocalStorageHelper& operator=(const LocalStorageHelper&) = delete;

  // Starts the fetching process, which will notify its completion via
  // callback. This must be called only in the UI thread.
  virtual void StartFetching(FetchCallback callback);

 protected:
  friend class base::RefCounted<LocalStorageHelper>;
  virtual ~LocalStorageHelper();

  raw_ptr<content::DOMStorageContext>
      dom_storage_context_;  // Owned by the context
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_LOCAL_STORAGE_HELPER_H_
