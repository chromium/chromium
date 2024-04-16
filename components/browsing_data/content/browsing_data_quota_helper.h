// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_QUOTA_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_QUOTA_HELPER_H_

#include <stdint.h>

#include <list>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

class BrowsingDataQuotaHelper;

namespace content {
class StoragePartition;
}  // namespace content

struct BrowsingDataQuotaHelperDeleter {
  static void Destruct(const BrowsingDataQuotaHelper* helper);
};

// This class is an interface class to bridge between Cookies Tree and Unified
// Quota System.  This class provides a way to get usage and quota information
// through the instance.
//
// Call Create to create an instance for a profile and call StartFetching with
// a callback to fetch information asynchronously.
//
// Parallel fetching is not allowed, a fetching task should start after end of
// previous task.  All method of this class should called from UI thread.
class BrowsingDataQuotaHelper
    : public base::RefCountedThreadSafe<BrowsingDataQuotaHelper,
                                        BrowsingDataQuotaHelperDeleter> {
 public:
  // QuotaInfo contains StorageKey-based quota and usage information for
  // temporary storage.
  struct QuotaInfo {
    QuotaInfo();
    explicit QuotaInfo(const blink::StorageKey& storage_key);
    QuotaInfo(const blink::StorageKey& storage_key,
              int64_t temporary_usage,
              int64_t syncable_usage);
    ~QuotaInfo();

    // Certain versions of MSVC 2008 have bad implementations of ADL for nested
    // classes so they require these operators to be declared here instead of in
    // the global namespace.
    bool operator<(const QuotaInfo& rhs) const;
    bool operator==(const QuotaInfo& rhs) const;

    blink::StorageKey storage_key;
    int64_t temporary_usage = 0;
    int64_t syncable_usage = 0;
  };

  using QuotaInfoArray = std::list<QuotaInfo>;
  using FetchResultCallback = base::OnceCallback<void(const QuotaInfoArray&)>;

  static scoped_refptr<BrowsingDataQuotaHelper> Create(
      content::StoragePartition* storage_partition);

  BrowsingDataQuotaHelper(const BrowsingDataQuotaHelper&) = delete;
  BrowsingDataQuotaHelper& operator=(const BrowsingDataQuotaHelper&) = delete;

  virtual void StartFetching(FetchResultCallback callback) = 0;

  // TODO(crbug.com/40273188): DEPRECATED please prefer using
  // `DeleteStorageKeyData`. This should be removed as part of
  // `CookiesTreeModel` deprecation.
  virtual void DeleteHostData(const std::string& host,
                              blink::mojom::StorageType type) = 0;

  virtual void DeleteStorageKeyData(const blink::StorageKey& storage_key,
                                    blink::mojom::StorageType type,
                                    base::OnceClosure completed) = 0;

 protected:
  BrowsingDataQuotaHelper();
  virtual ~BrowsingDataQuotaHelper();

 private:
  friend class base::DeleteHelper<BrowsingDataQuotaHelper>;
  friend struct BrowsingDataQuotaHelperDeleter;
};

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_QUOTA_HELPER_H_
