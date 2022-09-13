// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/conditional_cache_counting_helper.h"

#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace browsing_data {

// static
void ConditionalCacheCountingHelper::Count(
    content::StoragePartition* storage_partition,
    base::Time begin_time,
    base::Time end_time,
    CacheCountCallback result_callback) {
  DCHECK(!result_callback.is_null());

  storage_partition->GetNetworkContext()->ComputeHttpCacheSize(
      begin_time, end_time,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(result_callback),
          /* is_upper_limit = */ false,
          /* result_or_error = */ net::ERR_FAILED));
}

}  // namespace browsing_data
