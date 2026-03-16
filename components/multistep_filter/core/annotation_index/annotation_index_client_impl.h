// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_IMPL_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_IMPL_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"

namespace multistep_filter {

struct FilterAnnotation;
class UrlFilterSuggestion;

class AnnotationIndexClientImpl : public AnnotationIndexClient {
 public:
  AnnotationIndexClientImpl();
  ~AnnotationIndexClientImpl() override;

  // AnnotationIndexClient overrides:
  void GetUrlFilterSuggestions(
      const GURL& url,
      base::span<const FilterAnnotation> filter_annotations,
      base::OnceCallback<void(std::optional<std::vector<UrlFilterSuggestion>>)>
          callback) override;

  void GetSupportedTaskTypesForDomain(
      std::string_view domain,
      base::OnceCallback<void(std::optional<std::vector<std::string>>)>
          callback) override;

  void ExtractFilterAnnotation(
      const GURL& url,
      base::OnceCallback<void(std::optional<FilterAnnotation>)> callback)
      override;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_IMPL_H_
