// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/annotation_index_client_impl.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client_impl_config.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_conversion_util.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/version_info/channel.h"
#include "google_apis/common/api_key_request_util.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

// Determines whether a HTTP request was successful based on its response code.
bool IsHttpSuccess(int response_code) {
  return response_code >= 200 && response_code < 300;
}

std::string GetAPIKeyForUrl(version_info::Channel channel) {
  return google_apis::GetAPIKey(channel);
}

}  // namespace

// static
std::unique_ptr<AnnotationIndexClient> AnnotationIndexClient::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel) {
  return std::make_unique<AnnotationIndexClientImpl>(
      std::move(url_loader_factory), channel);
}

// TODO(crbug.com/483673955): Add UMA metrics for latency, traffic, and error
// tracking.
AnnotationIndexClientImpl::AnnotationIndexClientImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel)
    : AnnotationIndexClientImpl(std::move(url_loader_factory),
                                GetAPIKeyForUrl(channel)) {}

AnnotationIndexClientImpl::AnnotationIndexClientImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string api_key)
    : url_loader_factory_(std::move(url_loader_factory)),
      api_key_(std::move(api_key)) {}

AnnotationIndexClientImpl::~AnnotationIndexClientImpl() = default;

void AnnotationIndexClientImpl::GetFilterSuggestionCandidates(
    const GURL& url,
    base::span<const FilterAnnotation> filter_annotations,
    base::OnceCallback<
        void(std::optional<std::vector<FilterSuggestionCandidate>>)> callback) {
  // TODO(crbug.com/483677417): Implement the logic to retrieve the
  // `FilterSuggestionCandidate`s for a given url and filter annotations.
  NOTIMPLEMENTED();
  std::move(callback).Run(std::nullopt);
}

void AnnotationIndexClientImpl::GetSupportedTaskTypesForDomain(
    std::string_view domain,
    base::OnceCallback<void(std::optional<std::vector<std::string>>)>
        callback) {
  // TODO(crbug.com/483677417): Implement the logic to retrieve supported
  // task types for a given domain.
  NOTIMPLEMENTED();
  std::move(callback).Run(std::nullopt);
}

void AnnotationIndexClientImpl::ExtractFilterAnnotation(
    const GURL& url,
    base::OnceCallback<void(std::optional<FilterAnnotation>)> callback) {
  // TODO(crbug.com/483677417): Implement the logic to retrieve the extracted
  // `FilterAnnotation` from the url.
  NOTIMPLEMENTED();
  std::move(callback).Run(std::nullopt);
}

void AnnotationIndexClientImpl::ExecuteRequest(
    std::unique_ptr<network::ResourceRequest> request,
    std::string request_body,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    base::OnceCallback<void(std::optional<std::string>)> callback) {
  if (!api_key_.empty()) {
    google_apis::AddAPIKeyToRequest(*request, api_key_);
  }

  active_url_loaders_.push_back(
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation));
  network::SimpleURLLoader& loader =
      CHECK_DEREF(active_url_loaders_.back().get());

  loader.AttachStringForUpload(
      std::move(request_body),
      annotation_index_client_impl_config::kApplicationProtobufContentType);
  loader.SetAllowHttpErrorResults(
      annotation_index_client_impl_config::kAllowHttpErrorResults);
  loader.SetTimeoutDuration(
      annotation_index_client_impl_config::kNetworkRequestTimeout);
  loader.DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&AnnotationIndexClientImpl::OnSimpleURLLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr(), &loader,
                     std::move(callback)),
      annotation_index_client_impl_config::kMaxDownloadSize);
}

void AnnotationIndexClientImpl::OnSimpleURLLoaderComplete(
    network::SimpleURLLoader* loader,
    base::OnceCallback<void(std::optional<std::string>)> callback,
    std::optional<std::string> response_body) {
  int response_code = -1;
  if (loader->ResponseInfo() && loader->ResponseInfo()->headers) {
    response_code = loader->ResponseInfo()->headers->response_code();
  }
  const bool is_success = IsHttpSuccess(response_code) && response_body;

  std::erase_if(active_url_loaders_, base::MatchesUniquePtr(loader));

  if (!is_success) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(std::move(response_body));
}

}  // namespace multistep_filter
