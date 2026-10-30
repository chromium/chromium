// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_QUOTA_HELPER_IMPL_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_QUOTA_HELPER_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/browsing_data/content/browsing_data_quota_helper.h"

namespace blink {
class StorageKey;
}

namespace storage {
class QuotaManagerProxy;
}

// Implementation of BrowsingDataQuotaHelper, used on the UI thread.
class BrowsingDataQuotaHelperImpl : public BrowsingDataQuotaHelper {
 public:
  void StartFetching(FetchResultCallback callback) override;
  void DeleteStorageKeyData(const blink::StorageKey& storage_key,
                            base::OnceClosure completed) override;

  explicit BrowsingDataQuotaHelperImpl(
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy);

  BrowsingDataQuotaHelperImpl(const BrowsingDataQuotaHelperImpl&) = delete;
  BrowsingDataQuotaHelperImpl& operator=(const BrowsingDataQuotaHelperImpl&) =
      delete;

 private:
  ~BrowsingDataQuotaHelperImpl() override;

  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
};

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_QUOTA_HELPER_IMPL_H_
