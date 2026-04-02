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
#include "components/multistep_filter/core/storage/filter_store.h"
#include "url/gurl.h"

namespace multistep_filter {

FilterExtractor::FilterExtractor(AnnotationIndexClient& annotation_index_client,
                                 FilterStore& filter_store)
    : annotation_index_client_(annotation_index_client),
      filter_store_(filter_store) {}

FilterExtractor::~FilterExtractor() = default;

void FilterExtractor::ExtractAnnotationFromUrl(
    const GURL& url,
    base::OnceCallback<void(std::optional<base::Uuid>)> callback) {
  annotation_index_client_->ExtractFilterAnnotation(
      url, base::BindOnce(&FilterExtractor::OnAnnotationExtracted,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FilterExtractor::OnAnnotationExtracted(
    base::OnceCallback<void(std::optional<base::Uuid>)> callback,
    std::optional<FilterAnnotation> annotation) {
  if (annotation) {
    base::Uuid id = annotation->id;
    filter_store_->StoreAnnotation(
        *annotation, base::BindOnce(&FilterExtractor::OnAnnotationStored,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback), std::move(id)));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void FilterExtractor::OnAnnotationStored(
    base::OnceCallback<void(std::optional<base::Uuid>)> callback,
    base::Uuid id,
    bool success) {
  std::move(callback).Run(success ? std::make_optional(std::move(id))
                                  : std::nullopt);
}

}  // namespace multistep_filter
