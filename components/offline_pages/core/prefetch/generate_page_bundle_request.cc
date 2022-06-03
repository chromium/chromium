// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"

#include "base/bind.h"
#include "base/location.h"
#include "components/offline_pages/core/prefetch/prefetch_proto_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_request_fetcher.h"
#include "components/offline_pages/core/prefetch/prefetch_server_urls.h"
#include "components/offline_pages/core/prefetch/proto/offline_pages.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace offline_pages {

GeneratePageBundleRequest::GeneratePageBundleRequest(
    const std::string& user_agent,
    const std::string& gcm_registration_id,
    int max_bundle_size_bytes,
    const std::vector<std::string>& page_urls,
    version_info::Channel channel,
    const std::string& testing_header_value,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefetchRequestFinishedCallback callback)
    : callback_(std::move(callback)), requested_urls_(page_urls) {
  proto::GeneratePageBundleRequest request;
  request.set_user_agent(user_agent);
  request.set_max_bundle_size_bytes(max_bundle_size_bytes);
  request.set_output_format(proto::FORMAT_MHTML);
  request.set_gcm_registration_id(gcm_registration_id);

  for (const auto& url : requested_urls_) {
    proto::PageParameters* page = request.add_pages();
    page->set_url(url);
    page->set_transformation(proto::NO_TRANSFORMATION);
  }

  std::string upload_data;
  request.SerializeToString(&upload_data);

  fetcher_ = PrefetchRequestFetcher::CreateForPost(
      GeneratePageBundleRequestURL(channel), upload_data, testing_header_value,
      requested_urls_.empty(), url_loader_factory,
      base::BindOnce(&GeneratePageBundleRequest::OnCompleted,
                     // Fetcher is owned by this instance.
                     base::Unretained(this)));
}

GeneratePageBundleRequest::~GeneratePageBundleRequest() {}

void GeneratePageBundleRequest::OnCompleted(PrefetchRequestStatus status,
                                            const std::string& data) {
  if (status != PrefetchRequestStatus::kSuccess) {
    std::move(callback_).Run(status, std::string(),
                             std::vector<RenderPageInfo>());
    return;
  }

  std::vector<RenderPageInfo> pages;
  std::string operation_name = ParseOperationResponse(data, &pages);
  if (operation_name.empty()) {
    std::move(callback_).Run(PrefetchRequestStatus::kShouldRetryWithBackoff,
                             std::string(), std::vector<RenderPageInfo>());
    return;
  }

  std::move(callback_).Run(status, operation_name, pages);
}

}  // namespace offline_pages
