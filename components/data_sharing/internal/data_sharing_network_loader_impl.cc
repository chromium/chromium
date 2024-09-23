// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_network_loader_impl.h"

#include "base/time/time.h"
#include "components/data_sharing/public/data_sharing_network_loader.h"
#include "components/data_sharing/public/group_data.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"

namespace data_sharing {

namespace {

constexpr base::TimeDelta kTimeout = base::Milliseconds(10000);
const char kOauthConsumerName[] = "datasharing";
const char kRequestContentType[] = "application/x-protobuf";

}  // namespace

DataSharingNetworkLoaderImpl::DataSharingNetworkLoaderImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {
  CHECK(identity_manager);
}

DataSharingNetworkLoaderImpl::~DataSharingNetworkLoaderImpl() = default;

void DataSharingNetworkLoaderImpl::LoadUrl(
    const GURL& url,
    const std::vector<std::string>& scopes,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    NetworkLoaderCallback callback) {
  std::unique_ptr<EndpointFetcher> endpoint_fetcher =
      CreateEndpointFetcher(url, scopes, post_data, annotation_tag);
  auto* const fetcher_ptr = endpoint_fetcher.get();
  fetcher_ptr->Fetch(
      base::BindOnce(&DataSharingNetworkLoaderImpl::OnDownloadComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(endpoint_fetcher)));
}

std::unique_ptr<EndpointFetcher>
DataSharingNetworkLoaderImpl::CreateEndpointFetcher(
    const GURL& url,
    const std::vector<std::string>& scopes,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, kOauthConsumerName, url,
      net::HttpRequestHeaders::kPostMethod, kRequestContentType, scopes,
      kTimeout, post_data, annotation_tag, identity_manager_,
      signin::ConsentLevel::kSignin);
}

void DataSharingNetworkLoaderImpl::OnDownloadComplete(
    NetworkLoaderCallback callback,
    std::unique_ptr<EndpointFetcher> fetcher,
    std::unique_ptr<EndpointResponse> response) {
  NetworkLoaderStatus status = NetworkLoaderStatus::kSuccess;
  if (response->http_status_code != net::HTTP_OK || response->error_type) {
    VLOG(1) << "Data sharing network request failed http status: "
            << response->http_status_code << " "
            << (response->error_type ? static_cast<int>(*response->error_type)
                                     : -1);
    // TODO(ssid): Investigate whether some auth errors are permanent.
    status = NetworkLoaderStatus::kTransientFailure;
  }
  std::move(callback).Run(
      std::make_unique<DataSharingNetworkLoader::LoadResult>(
          std::move(response->response), status));
}

}  // namespace data_sharing
