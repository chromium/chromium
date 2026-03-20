// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_MOCK_ANNOTATION_INDEX_CLIENT_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_MOCK_ANNOTATION_INDEX_CLIENT_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace multistep_filter {

class MockAnnotationIndexClient : public AnnotationIndexClient {
 public:
  MockAnnotationIndexClient();
  ~MockAnnotationIndexClient() override;

  MOCK_METHOD(
      void,
      GetFilterSuggestionCandidates,
      (const GURL& url,
       base::span<const FilterAnnotation> filter_annotations,
       base::OnceCallback<void(
           std::optional<std::vector<FilterSuggestionCandidate>>)> callback),
      (override));

  MOCK_METHOD(void,
              GetSupportedTaskTypesForDomain,
              (std::string_view domain,
               base::OnceCallback<void(std::optional<std::vector<std::string>>)>
                   callback),
              (override));

  MOCK_METHOD(
      void,
      ExtractFilterAnnotation,
      (const GURL& url,
       base::OnceCallback<void(std::optional<FilterAnnotation>)> callback),
      (override));
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_MOCK_ANNOTATION_INDEX_CLIENT_H_
