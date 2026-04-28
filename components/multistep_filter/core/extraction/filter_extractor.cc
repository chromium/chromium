// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/extraction/filter_extractor.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

void LogExtractionFailed(MultistepFilterLogRouter* log_router,
                         int64_t navigation_id,
                         std::string_view domain,
                         std::string_view detail_key) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kAnnotationsExtracted, domain)
      << LogDetail{std::string(detail_key), false};
}

void LogAnnotationFetched(MultistepFilterLogRouter* log_router,
                          int64_t navigation_id,
                          std::string_view domain,
                          const FilterAnnotation& annotation) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kAnnotationsExtracted, domain)
      << LogDetail{"fetched", true}
      << LogDetail{"annotation", annotation.ToString()};
}

void LogAnnotationStored(MultistepFilterLogRouter* log_router,
                         int64_t navigation_id,
                         std::string_view domain) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kAnnotationsExtracted, domain)
      << LogDetail{"stored", true};
}

}  // namespace

FilterExtractor::FilterExtractor(AnnotationIndexClient& annotation_index_client,
                                 FilterStore& filter_store,
                                 MultistepFilterLogRouter* log_router)
    : annotation_index_client_(annotation_index_client),
      filter_store_(filter_store),
      log_router_(log_router) {}

FilterExtractor::~FilterExtractor() = default;

void FilterExtractor::ExtractAnnotationFromUrl(
    const GURL& url,
    base::OnceCallback<void(std::optional<base::Uuid>)> callback,
    int64_t navigation_id,
    std::string_view domain) {
  annotation_index_client_->ExtractFilterAnnotation(
      url, base::BindOnce(&FilterExtractor::OnAnnotationExtracted,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                          navigation_id, std::string(domain)));
}

void FilterExtractor::OnAnnotationExtracted(
    base::OnceCallback<void(std::optional<base::Uuid>)> callback,
    int64_t navigation_id,
    std::string_view domain,
    std::optional<FilterAnnotation> annotation) {
  if (annotation) {
    LogAnnotationFetched(log_router_, navigation_id, domain, *annotation);
    base::Uuid annotation_id = annotation->id;
    filter_store_->StoreAnnotation(
        *annotation,
        base::BindOnce(&FilterExtractor::OnAnnotationStored,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(annotation_id), navigation_id,
                       std::string(domain)));
  } else {
    LogExtractionFailed(log_router_, navigation_id, domain, "extracted");
    std::move(callback).Run(std::nullopt);
  }
}

void FilterExtractor::OnAnnotationStored(
    base::OnceCallback<void(std::optional<base::Uuid>)> callback,
    base::Uuid annotation_id,
    int64_t navigation_id,
    std::string_view domain,
    bool success) {
  if (!success) {
    LogExtractionFailed(log_router_, navigation_id, domain, "stored");
    std::move(callback).Run(std::nullopt);
  } else {
    LogAnnotationStored(log_router_, navigation_id, domain);
    std::move(callback).Run(std::move(annotation_id));
  }
}

}  // namespace multistep_filter
