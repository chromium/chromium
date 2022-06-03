// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_NETWORK_REQUEST_FACTORY_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_NETWORK_REQUEST_FACTORY_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {
class GetOperationRequest;

class PrefetchNetworkRequestFactory {
 public:
  virtual ~PrefetchNetworkRequestFactory() = default;

  // Returns 'true' if there is at least one outstanding active network
  // request. Used to determine if the background processing window can be
  // closed.
  virtual bool HasOutstandingRequests() const = 0;

  // Creates and starts a new GeneratePageBundle request, retaining ownership.
  // If a GeneratePageBundle request for one or more specified URLs already
  // exists, this will create another one regardless.
  virtual void MakeGeneratePageBundleRequest(
      const std::vector<std::string>& prefetch_urls,
      const std::string& gcm_registration_id,
      PrefetchRequestFinishedCallback callback) = 0;

  // Returns a list of URLs included into all currently ongoing
  // GeneratePageBundle requests.
  virtual std::unique_ptr<std::set<std::string>> GetAllUrlsRequested()
      const = 0;

  // Creates and starts a new GetOperationRequest, retaining ownership.
  // If a GetOperation request already exists with the same operation name, this
  // will cancel the existing request and start a new one.
  virtual void MakeGetOperationRequest(
      const std::string& operation_name,
      PrefetchRequestFinishedCallback callback) = 0;

  // Returns the current GetOperationRequest with the given name, if any.
  virtual GetOperationRequest* FindGetOperationRequestByName(
      const std::string& operation_name) const = 0;

  // Returns a list of operation names included into all currently ongoing
  // GetOperation requests.
  virtual std::unique_ptr<std::set<std::string>> GetAllOperationNamesRequested()
      const = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_NETWORK_REQUEST_FACTORY_H_
