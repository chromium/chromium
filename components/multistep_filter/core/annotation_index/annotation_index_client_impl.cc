// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/annotation_index_client_impl.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "url/gurl.h"

namespace multistep_filter {

AnnotationIndexClientImpl::AnnotationIndexClientImpl() = default;
AnnotationIndexClientImpl::~AnnotationIndexClientImpl() = default;

void AnnotationIndexClientImpl::GetUrlFilterSuggestions(
    const GURL& url,
    base::span<const FilterAnnotation> filter_annotations,
    base::OnceCallback<void(std::optional<std::vector<UrlFilterSuggestion>>)>
        callback) {
  // TODO(crbug.com/483677417): Implement the logic to retrieve the
  // `UrlFilterSuggestion`s for a given url and filter annotations.
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

}  // namespace multistep_filter
