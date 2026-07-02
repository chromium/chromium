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
#include "components/multistep_filter/core/storage/filter_store.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

void LogExtractionFailed(MultistepFilterLogRouter* log_router,
                         int64_t navigation_id,
                         std::string_view host,
                         std::string_view detail_key) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kAnnotationsExtracted, host)
      << LogDetail{detail_key, false};
}

void LogAnnotationFetched(MultistepFilterLogRouter* log_router,
                          int64_t navigation_id,
                          std::string_view host,
                          const FilterAnnotation& annotation) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kAnnotationsExtracted, host)
      << LogDetail{"fetched", true}
      << LogDetail{"annotation", annotation.ToString()};
}

void LogAnnotationStored(MultistepFilterLogRouter* log_router,
                         int64_t navigation_id,
                         std::string_view host) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id,
                       LogEventType::kAnnotationsExtracted, host)
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
    base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
    int64_t navigation_id) {
  annotation_index_client_->ExtractFilterAnnotation(
      url,
      base::BindOnce(&FilterExtractor::OnAnnotationExtracted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     navigation_id, url.GetHost()),
      navigation_id);
}

void FilterExtractor::OnAnnotationExtracted(
    base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
    int64_t navigation_id,
    std::string_view host,
    std::optional<FilterAnnotation> annotation) {
  if (annotation) {
    LogAnnotationFetched(log_router_, navigation_id, host, *annotation);
    filter_store_->StoreAnnotation(
        *annotation,
        base::BindOnce(&FilterExtractor::OnAnnotationStored,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       annotation, navigation_id, std::string(host)));
  } else {
    LogExtractionFailed(log_router_, navigation_id, host, "extracted");
    std::move(callback).Run(std::nullopt);
  }
}

void FilterExtractor::OnAnnotationStored(
    base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
    std::optional<FilterAnnotation> annotation,
    int64_t navigation_id,
    std::string_view host,
    bool success) {
  if (!success) {
    LogExtractionFailed(log_router_, navigation_id, host, "stored");
    std::move(callback).Run(std::nullopt);
  } else {
    LogAnnotationStored(log_router_, navigation_id, host);
    std::move(callback).Run(std::move(annotation));
  }
}

}  // namespace multistep_filter
