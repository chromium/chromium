// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_QUOTA_HELPER_IMPL_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_QUOTA_HELPER_IMPL_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/browsing_data/content/browsing_data_quota_helper.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"

namespace blink {
class StorageKey;
}

namespace storage {
class QuotaManager;
}

// Implementation of BrowsingDataQuotaHelper.  Since a client of
// BrowsingDataQuotaHelper should live in UI thread and QuotaManager lives in
// IO thread, we have to communicate over thread using PostTask.
class BrowsingDataQuotaHelperImpl : public BrowsingDataQuotaHelper {
 public:
  void StartFetching(FetchResultCallback callback) override;
  void DeleteHostData(const std::string& host,
                      blink::mojom::StorageType type) override;
  void DeleteStorageKeyData(const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type,
                            base::OnceClosure completed) override;

  explicit BrowsingDataQuotaHelperImpl(storage::QuotaManager* quota_manager);

  BrowsingDataQuotaHelperImpl(const BrowsingDataQuotaHelperImpl&) = delete;
  BrowsingDataQuotaHelperImpl& operator=(const BrowsingDataQuotaHelperImpl&) =
      delete;

 private:
  using PendingHosts =
      std::set<std::pair<std::string, blink::mojom::StorageType>>;
  using QuotaInfoMap = std::map<blink::StorageKey, QuotaInfo>;

  ~BrowsingDataQuotaHelperImpl() override;

  // Calls QuotaManager::GetStorageKeysModifiedBetween for each storage type.
  void FetchQuotaInfoOnIOThread(FetchResultCallback callback);

  // Callback function for QuotaManager::GetStorageKeysForType.
  void GotStorageKeys(QuotaInfoMap* quota_info,
                      base::OnceClosure completion,
                      blink::mojom::StorageType type,
                      const std::set<blink::StorageKey>& storage_keys);

  // Callback function for QuotaManager::GetStorageKeyUsage.
  void GotStorageKeyUsage(QuotaInfoMap* quota_info,
                          const blink::StorageKey& storage_key,
                          blink::mojom::StorageType type,
                          int64_t usage,
                          blink::mojom::UsageBreakdownPtr usage_breakdown);

  // Called when all QuotaManager::GetHostUsage requests are complete.
  void OnGetHostsUsageComplete(FetchResultCallback callback,
                               QuotaInfoMap* quota_info);

  void DeleteHostDataOnIOThread(const std::string& host,
                                blink::mojom::StorageType type);

  void DeleteStorageKeyDataOnIOThread(const blink::StorageKey& storage_key,
                                      blink::mojom::StorageType type,
                                      base::OnceClosure completed);

  void OnStorageKeyDeletionCompleted(base::OnceClosure completed,
                                     blink::mojom::QuotaStatusCode status);

  scoped_refptr<storage::QuotaManager> quota_manager_;

  base::WeakPtrFactory<BrowsingDataQuotaHelperImpl> weak_factory_{this};
};

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_QUOTA_HELPER_IMPL_H_
