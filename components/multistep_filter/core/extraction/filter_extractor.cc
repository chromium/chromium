// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/extraction/filter_extractor.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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

void FilterExtractor::ExtractAnnotationFromUrl(const GURL& url) {
  annotation_index_client_->ExtractFilterAnnotation(
      url, base::BindOnce(&FilterExtractor::OnAnnotationExtracted,
                          weak_ptr_factory_.GetWeakPtr()));
}

void FilterExtractor::OnAnnotationExtracted(
    std::optional<FilterAnnotation> annotation) {
  // TODO(crbug.com/483673955): Handle the case where the annotation is null.
  if (annotation) {
    filter_store_->StoreAnnotation(*annotation, base::DoNothing());
  }
}

}  // namespace multistep_filter
