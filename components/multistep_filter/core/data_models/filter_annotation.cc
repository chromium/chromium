// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/data_models/filter_annotation.h"

#include "base/time/time.h"
#include "url/gurl.h"

namespace multistep_filter {

FilterAttribute::FilterAttribute(std::string normalized_key,
                                 std::string normalized_value)
    : normalized_key(std::move(normalized_key)),
      normalized_value(std::move(normalized_value)) {
  DCHECK(!this->normalized_key.empty());
  DCHECK(!this->normalized_value.empty());
}

FilterAnnotation::FilterAnnotation(base::Uuid id,
                                   std::string task_type,
                                   std::string source_domain,
                                   GURL source_url,
                                   base::Time creation_timestamp,
                                   std::vector<FilterAttribute> attributes)
    : id(std::move(id)),
      task_type(std::move(task_type)),
      source_domain(std::move(source_domain)),
      source_url(std::move(source_url)),
      creation_timestamp(creation_timestamp),
      attributes(std::move(attributes)) {
  DCHECK(!this->task_type.empty());
  DCHECK(!this->source_domain.empty());
}

FilterAnnotation::FilterAnnotation(const FilterAnnotation&) = default;
FilterAnnotation::FilterAnnotation(FilterAnnotation&&) = default;
FilterAnnotation& FilterAnnotation::operator=(const FilterAnnotation&) =
    default;
FilterAnnotation& FilterAnnotation::operator=(FilterAnnotation&&) = default;
FilterAnnotation::~FilterAnnotation() = default;

bool operator==(const FilterAnnotation&, const FilterAnnotation&) = default;
auto operator<=>(const FilterAnnotation&, const FilterAnnotation&) = default;

}  // namespace multistep_filter
