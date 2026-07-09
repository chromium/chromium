// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/network/personal_context_manager.h"

#include <algorithm>
#include <optional>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/context_memory_features.h"
#include "components/personal_context/core/network/personal_context_fetcher.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace personal_context {

namespace {

constexpr char kContextMemoryServiceBaseUrl[] =
    "https://contextmemoryservice.pa.googleapis.com/v1";

// Returns the base URL of the Context Memory Service. If the
// `kContextMemoryServiceBaseUrlSwitch` is set, its value is returned. If the
// `kContextMemoryServiceBaseUrlParam` feature parameter is set, its value is
// returned. Otherwise, the production URL is returned.
GURL GetContextMemoryServiceBaseUrl() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          features::debug::kContextMemoryServiceBaseUrlSwitch)) {
    return GURL(command_line->GetSwitchValueASCII(
        features::debug::kContextMemoryServiceBaseUrlSwitch));
  }
  const std::string base_url_override =
      features::debug::kContextMemoryServiceBaseUrlParam.Get();
  if (!base_url_override.empty()) {
    return GURL(base_url_override);
  }

  return GURL(kContextMemoryServiceBaseUrl);
}

// The maximum number of parallel `FetchContext()` calls allowed for the
// `feature`. Must be at least 1.
// If a new fetch request exceeds this limit, the oldest pending
// execution is cancelled.
size_t GetMaxParallelFeatureFetchers(proto::ContextMemoryFeature feature) {
  switch (feature) {
    case proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL:
      return 2;
    case proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY:
    case proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS:
      return 1;
    default:
      NOTREACHED();
  }
}

// The maximum number of parallel `FetchPiiEntities()` calls allowed for the
// `feature`. Must be at least 1.
// If a new fetch request exceeds this limit, the oldest pending
// execution is cancelled.
size_t GetMaxParallelPiiFeatureFetchers(proto::ContextMemoryFeature feature) {
  switch (feature) {
    case proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL:
    case proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY:
    case proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS:
      return 1;
    default:
      NOTREACHED();
  }
}

void RecordFetchContextResultHistogram(proto::ContextMemoryFeature feature,
                                       bool success) {
  base::UmaHistogramBoolean(
      base::StrCat({"PersonalContext.FetchContext.Result.",
                    GetStringNameForContextMemoryFeature(feature)}),
      success);
}

void RecordFetchContextLatencyHistogram(proto::ContextMemoryFeature feature,
                                        base::TimeDelta latency) {
  base::UmaHistogramMediumTimes(
      base::StrCat({"PersonalContext.FetchContext.Latency.",
                    GetStringNameForContextMemoryFeature(feature)}),
      latency);
}

void RecordFetchPiiEntitiesResultHistogram(proto::ContextMemoryFeature feature,
                                           bool success) {
  base::UmaHistogramBoolean(
      base::StrCat({"PersonalContext.FetchPiiEntities.Result.",
                    GetStringNameForContextMemoryFeature(feature)}),
      success);
}

void RecordFetchPiiEntitiesLatencyHistogram(proto::ContextMemoryFeature feature,
                                            base::TimeDelta latency) {
  base::UmaHistogramMediumTimes(
      base::StrCat({"PersonalContext.FetchPiiEntities.Latency.",
                    GetStringNameForContextMemoryFeature(feature)}),
      latency);
}

}  // namespace

PersonalContextManager::PersonalContextManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager),
      memory_service_url_(GetContextMemoryServiceBaseUrl()) {}

PersonalContextManager::~PersonalContextManager() = default;

void PersonalContextManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Invalidate the weak pointers before clearing the active fetchers, which
  // will cause the drop all the fetch consumer callbacks, and avoid
  // all processing during destructor.
  weak_ptr_factory_.InvalidateWeakPtrs();
  active_fetchers_.clear();
  active_pii_fetchers_.clear();
}

void PersonalContextManager::FetchContext(
    proto::ContextMemoryFeature feature,
    const google::protobuf::MessageLite& request_metadata,
    std::optional<base::TimeDelta> timeout,
    FetchContextCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ActiveFeatureFetchers& fetchers_for_feature = active_fetchers_[feature];
  if (fetchers_for_feature.size() == GetMaxParallelFeatureFetchers(feature)) {
    // Cancel the fetcher with the smallest ID. Since IDs are assigned in
    // increasing order, this cancels the oldest one.
    fetchers_for_feature.erase(fetchers_for_feature.begin());
  }

  FetcherId fetcher_id = next_fetcher_id_++;
  auto fetcher = std::make_unique<PersonalContextFetcher>(
      feature, identity_manager_, url_loader_factory_, memory_service_url_);

  auto fetcher_it =
      fetchers_for_feature.emplace(fetcher_id, std::move(fetcher));
  fetcher_it.first->second->FetchContext(
      request_metadata, timeout,
      base::BindOnce(&PersonalContextManager::OnFetchContextResponse,
                     weak_ptr_factory_.GetWeakPtr(), feature, fetcher_id,
                     base::TimeTicks::Now(), std::move(callback)));
}

void PersonalContextManager::OnFetchContextResponse(
    proto::ContextMemoryFeature feature,
    FetcherId fetcher_id,
    base::TimeTicks start_time,
    FetchContextCallback callback,
    base::expected<const proto::FetchContextResponse, ContextMemoryError>
        fetch_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_fetchers_[feature].erase(fetcher_id);

  RecordFetchContextLatencyHistogram(feature,
                                     base::TimeTicks::Now() - start_time);

  if (!fetch_response.has_value()) {
    RecordFetchContextResultHistogram(feature, /*success=*/false);
    std::move(callback).Run(FetchContextResult(
        base::unexpected(fetch_response.error()), /*server_request_id=*/""));
    return;
  }

  if (!fetch_response->has_response_metadata()) {
    RecordFetchContextResultHistogram(feature, /*success=*/false);
    std::move(callback).Run(FetchContextResult(
        base::unexpected(ContextMemoryError::FromExecutionError(
            ContextMemoryError::ExecutionError::kGenericFailure)),
        fetch_response->server_request_id()));
    return;
  }

  RecordFetchContextResultHistogram(feature, /*success=*/true);
  std::move(callback).Run(
      FetchContextResult(base::ok(fetch_response->response_metadata()),
                         fetch_response->server_request_id()));
}

void PersonalContextManager::FetchPiiEntities(
    const proto::FetchPiiEntitiesRequest& request,
    std::optional<base::TimeDelta> timeout,
    FetchPiiContextCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  proto::ContextMemoryFeature feature = request.feature();
  ActiveFeatureFetchers& fetchers_for_feature = active_pii_fetchers_[feature];
  if (fetchers_for_feature.size() ==
      GetMaxParallelPiiFeatureFetchers(feature)) {
    // Cancel the fetcher with the smallest ID. Since IDs are assigned in
    // increasing order, this cancels the oldest one.
    fetchers_for_feature.erase(fetchers_for_feature.begin());
  }

  FetcherId fetcher_id = next_fetcher_id_++;
  auto fetcher = std::make_unique<PersonalContextFetcher>(
      feature, identity_manager_, url_loader_factory_, memory_service_url_);

  auto fetcher_it =
      fetchers_for_feature.emplace(fetcher_id, std::move(fetcher));
  fetcher_it.first->second->FetchPiiEntities(
      request, timeout,
      base::BindOnce(&PersonalContextManager::OnFetchPiiEntitiesResponse,
                     weak_ptr_factory_.GetWeakPtr(), feature, fetcher_id,
                     base::TimeTicks::Now(), std::move(callback)));
}

void PersonalContextManager::OnFetchPiiEntitiesResponse(
    proto::ContextMemoryFeature feature,
    FetcherId fetcher_id,
    base::TimeTicks start_time,
    FetchPiiContextCallback callback,
    base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>
        fetch_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_pii_fetchers_[feature].erase(fetcher_id);

  RecordFetchPiiEntitiesLatencyHistogram(feature,
                                         base::TimeTicks::Now() - start_time);

  if (!fetch_response.has_value()) {
    RecordFetchPiiEntitiesResultHistogram(feature, /*success=*/false);
    std::move(callback).Run(FetchPiiEntitiesResult(std::move(fetch_response)));
    return;
  }

  RecordFetchPiiEntitiesResultHistogram(feature, /*success=*/true);
  std::move(callback).Run(FetchPiiEntitiesResult(std::move(fetch_response)));
}

}  // namespace personal_context
