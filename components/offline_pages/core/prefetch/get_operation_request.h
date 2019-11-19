// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_GET_OPERATION_REQUEST_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_GET_OPERATION_REQUEST_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/version_info/channel.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace offline_pages {

class PrefetchRequestFetcher;

// Sends this request to find out the current state of an operation that is
// triggered by GeneratePageBundleRequest but not finished at that time.
class GetOperationRequest {
 public:
  // |name| identifies the operation triggered by the GeneratePageBundleRequest.
  // It is retrieved from the operation data returned in the
  // GeneratePageBundleRequest response.
  GetOperationRequest(
      const std::string& name,
      version_info::Channel channel,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefetchRequestFinishedCallback callback);
  ~GetOperationRequest();

  // Returns the stored callback. Note that this moves the internal value
  // making it null.
  PrefetchRequestFinishedCallback GetCallbackForTesting();

 private:
  void OnCompleted(const std::string& operation_name,
                   PrefetchRequestStatus status,
                   const std::string& data);

  PrefetchRequestFinishedCallback callback_;
  std::unique_ptr<PrefetchRequestFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(GetOperationRequest);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_GET_OPERATION_REQUEST_H_
