// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_MOCK_ANNOTATION_INDEX_CLIENT_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_MOCK_ANNOTATION_INDEX_CLIENT_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace multistep_filter {

struct FilterAttribute;

class MockAnnotationIndexClient : public AnnotationIndexClient {
 public:
  MockAnnotationIndexClient();
  ~MockAnnotationIndexClient() override;

  MOCK_METHOD(
      void,
      GetUrlFilterSuggestions,
      (const GURL& url,
       std::string_view task_type,
       base::span<const FilterAttribute> filter_attributes,
       base::OnceCallback<void(std::optional<std::vector<UrlFilterSuggestion>>)>
           callback),
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
