// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_network_request_factory_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"
#include "components/offline_pages/core/prefetch/get_operation_request.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
// Max size of all articles archives to be generated from a single request. This
// 20 MiB value matches the current daily download limit.
constexpr int kMaxBundleSizeBytes = 20 * 1024 * 1024;  // 20 MB

// Max size of all articles archives to be generated from a single request when
// limitless prefetching is enabled. The 200 MiB value allows for 100 URLs (the
// maximum allowed in a single request) with 2 MiB articles (approximately
// double the average article size).
constexpr int kMaxBundleSizeForLimitlessBytes = 200 * 1024 * 1024;  // 200 MB

// Max concurrent outstanding requests. If more requests asked to be created,
// the requests are silently not created (considered failed). This is used
// as emergency limit that should rarely be encountered in normal operations.
constexpr int kMaxConcurrentRequests = 10;
}  // namespace

namespace offline_pages {

void RecordGetOperationStatusUma(PrefetchRequestStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.Prefetching.ServiceGetOperationStatus", status);
}

void RecordGeneratePageBundleStatusUma(PrefetchRequestStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "OfflinePages.Prefetching.ServiceGetPageBundleStatus", status);
}

PrefetchNetworkRequestFactoryImpl::PrefetchNetworkRequestFactoryImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel,
    const std::string& user_agent,
    PrefService* prefs)
    : url_loader_factory_(std::move(url_loader_factory)),
      channel_(channel),
      user_agent_(user_agent),
      prefs_(prefs) {}

PrefetchNetworkRequestFactoryImpl::~PrefetchNetworkRequestFactoryImpl() =
    default;

bool PrefetchNetworkRequestFactoryImpl::HasOutstandingRequests() const {
  return !(generate_page_bundle_requests_.empty() &&
           get_operation_requests_.empty());
}

void PrefetchNetworkRequestFactoryImpl::MakeGeneratePageBundleRequest(
    const std::vector<std::string>& url_strings,
    const std::string& gcm_registration_id,
    PrefetchRequestFinishedCallback callback) {
  DCHECK(callback);
  if (!AddConcurrentRequest())
    return;
  int max_bundle_size = prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs_)
                            ? kMaxBundleSizeForLimitlessBytes
                            : kMaxBundleSizeBytes;
  uint64_t request_id = GetNextRequestId();
  generate_page_bundle_requests_[request_id] =
      std::make_unique<GeneratePageBundleRequest>(
          user_agent_, gcm_registration_id, max_bundle_size, url_strings,
          channel_,

          prefetch_prefs::GetPrefetchTestingHeader(prefs_), url_loader_factory_,
          base::BindOnce(
              &PrefetchNetworkRequestFactoryImpl::GeneratePageBundleRequestDone,
              weak_factory_.GetWeakPtr(), std::move(callback), request_id));
}

std::unique_ptr<std::set<std::string>>
PrefetchNetworkRequestFactoryImpl::GetAllUrlsRequested() const {
  auto result = std::make_unique<std::set<std::string>>();
  for (const auto& request_pair : generate_page_bundle_requests_) {
    for (const auto& url : request_pair.second->requested_urls())
      result->insert(url);
  }
  return result;
}

void PrefetchNetworkRequestFactoryImpl::MakeGetOperationRequest(
    const std::string& operation_name,
    PrefetchRequestFinishedCallback callback) {
  DCHECK(callback);
  if (!AddConcurrentRequest())
    return;
  get_operation_requests_[operation_name] =
      std::make_unique<GetOperationRequest>(
          operation_name, channel_, url_loader_factory_,
          base::BindOnce(
              &PrefetchNetworkRequestFactoryImpl::GetOperationRequestDone,
              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PrefetchNetworkRequestFactoryImpl::GeneratePageBundleRequestDone(
    PrefetchRequestFinishedCallback callback,
    uint64_t request_id,
    PrefetchRequestStatus status,
    const std::string& operation_name,
    const std::vector<RenderPageInfo>& pages) {
  if (status == PrefetchRequestStatus::kShouldSuspendForbiddenByOPS ||
      status == PrefetchRequestStatus::kShouldSuspendNewlyForbiddenByOPS) {
    prefetch_prefs::SetEnabledByServer(prefs_, false);
  }
  std::move(callback).Run(status, operation_name, pages);
  generate_page_bundle_requests_.erase(request_id);
  ReleaseConcurrentRequest();
  RecordGeneratePageBundleStatusUma(status);
}

void PrefetchNetworkRequestFactoryImpl::GetOperationRequestDone(
    PrefetchRequestFinishedCallback callback,
    PrefetchRequestStatus status,
    const std::string& operation_name,
    const std::vector<RenderPageInfo>& pages) {
  std::move(callback).Run(status, operation_name, pages);
  get_operation_requests_.erase(operation_name);
  ReleaseConcurrentRequest();
  RecordGetOperationStatusUma(status);
}

GetOperationRequest*
PrefetchNetworkRequestFactoryImpl::FindGetOperationRequestByName(
    const std::string& operation_name) const {
  auto iter = get_operation_requests_.find(operation_name);
  if (iter == get_operation_requests_.end())
    return nullptr;

  return iter->second.get();
}

bool PrefetchNetworkRequestFactoryImpl::AddConcurrentRequest() {
  if (concurrent_request_count_ >= kMaxConcurrentRequests)
    return false;
  ++concurrent_request_count_;
  return true;
}

std::unique_ptr<std::set<std::string>>
PrefetchNetworkRequestFactoryImpl::GetAllOperationNamesRequested() const {
  auto result = std::make_unique<std::set<std::string>>();
  for (const auto& request_pair : get_operation_requests_)
    result->insert(request_pair.first);
  return result;
}

void PrefetchNetworkRequestFactoryImpl::ReleaseConcurrentRequest() {
  DCHECK_GT(concurrent_request_count_, 0U);
  --concurrent_request_count_;
}

// In-memory request id, incremented for each new GeneratePageBundleRequest.
uint64_t PrefetchNetworkRequestFactoryImpl::GetNextRequestId() {
  return ++request_id_;
}

}  // namespace offline_pages
