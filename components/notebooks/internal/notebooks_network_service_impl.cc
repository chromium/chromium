// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_network_service_impl.h"

#include "base/check.h"
#include "base/notimplemented.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

using endpoint_fetcher::EndpointFetcher;
using endpoint_fetcher::EndpointResponse;

namespace notebooks {

std::unique_ptr<EndpointFetcher>
NotebooksNetworkServiceImpl::CreateEndpointFetcher(
    const GURL& url,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, identity_manager_,
      EndpointFetcher::RequestParams::Builder(
          endpoint_fetcher::HttpMethod::kPost, annotation_tag)
          .SetAuthType(endpoint_fetcher::OAUTH)
          .SetConsentLevel(signin::ConsentLevel::kSignin)
          .SetUrl(url)
          .SetOAuthConsumerId(signin::OAuthConsumerId::kNotebooksService)
          .SetPostData(post_data)
          .Build());
}

NotebooksNetworkServiceImpl::NotebooksNetworkServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager) {
  CHECK(identity_manager);
}

NotebooksNetworkServiceImpl::~NotebooksNetworkServiceImpl() = default;

// NotebooksNetworkService Impl.
void NotebooksNetworkServiceImpl::CreateNotebook(
    std::string_view notebook_display_name,
    NetworkLoaderCallback callback) {
  NOTIMPLEMENTED();
}

void NotebooksNetworkServiceImpl::CreateNotebookSource(
    std::string_view notebook_id,
    std::string_view source_id,
    NetworkLoaderCallback callback) {
  NOTIMPLEMENTED();
}

void NotebooksNetworkServiceImpl::OnDownloadComplete(
    NetworkLoaderCallback callback,
    std::unique_ptr<endpoint_fetcher::EndpointFetcher> fetcher,
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  NOTIMPLEMENTED();
}

}  // namespace notebooks
