// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/get_operation_request.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/offline_pages/core/prefetch/prefetch_proto_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_request_fetcher.h"
#include "components/offline_pages/core/prefetch/prefetch_server_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace offline_pages {

GetOperationRequest::GetOperationRequest(
    const std::string& name,
    version_info::Channel channel,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefetchRequestFinishedCallback callback)
    : callback_(std::move(callback)) {
  fetcher_ = PrefetchRequestFetcher::CreateForGet(
      GetOperationRequestURL(name, channel), url_loader_factory,
      base::BindOnce(&GetOperationRequest::OnCompleted,
                     // Fetcher is owned by this instance.
                     base::Unretained(this), name));
}

GetOperationRequest::~GetOperationRequest() {}

void GetOperationRequest::OnCompleted(
    const std::string& assigned_operation_name,
    PrefetchRequestStatus status,
    const std::string& data) {
  if (status != PrefetchRequestStatus::kSuccess) {
    std::move(callback_).Run(status, assigned_operation_name,
                             std::vector<RenderPageInfo>());
    return;
  }

  std::vector<RenderPageInfo> pages;
  std::string found_operation_name = ParseOperationResponse(data, &pages);
  if (found_operation_name.empty()) {
    std::move(callback_).Run(PrefetchRequestStatus::kShouldRetryWithBackoff,
                             assigned_operation_name,
                             std::vector<RenderPageInfo>());
    return;
  }

  std::move(callback_).Run(PrefetchRequestStatus::kSuccess,
                           assigned_operation_name, pages);
}

PrefetchRequestFinishedCallback GetOperationRequest::GetCallbackForTesting() {
  return std::move(callback_);
}

}  // namespace offline_pages
