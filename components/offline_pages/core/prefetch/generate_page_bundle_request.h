// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_GENERATE_PAGE_BUNDLE_REQUEST_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_GENERATE_PAGE_BUNDLE_REQUEST_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/version_info/channel.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace offline_pages {

class PrefetchRequestFetcher;

class GeneratePageBundleRequest {
 public:
  GeneratePageBundleRequest(
      const std::string& user_agent,
      const std::string& gcm_registration_id,
      int max_bundle_size_bytes,
      const std::vector<std::string>& page_urls,
      version_info::Channel channel,
      const std::string& testing_header_value,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefetchRequestFinishedCallback callback);

  GeneratePageBundleRequest(const GeneratePageBundleRequest&) = delete;
  GeneratePageBundleRequest& operator=(const GeneratePageBundleRequest&) =
      delete;

  ~GeneratePageBundleRequest();

  const std::vector<std::string>& requested_urls() { return requested_urls_; }

 private:
  void OnCompleted(PrefetchRequestStatus status, const std::string& data);

  PrefetchRequestFinishedCallback callback_;
  std::vector<std::string> requested_urls_;
  std::unique_ptr<PrefetchRequestFetcher> fetcher_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_GENERATE_PAGE_BUNDLE_REQUEST_H_
