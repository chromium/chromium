// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/data_models/filter_annotation.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

namespace multistep_filter {

FilterAttribute::FilterAttribute(std::string key, std::string value)
    : key(std::move(key)), value(std::move(value)) {
  DCHECK(!this->key.empty());
  DCHECK(!this->value.empty());
}

FilterAnnotation::FilterAnnotation(base::Uuid id,
                                   std::string task_type,
                                   std::string source_domain,
                                   base::Time creation_timestamp,
                                   std::vector<FilterAttribute> attributes)
    : id(std::move(id)),
      task_type(std::move(task_type)),
      source_domain(std::move(source_domain)),
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

std::string FilterAttribute::ToString() const {
  return base::StrCat({"FilterAttribute(key=", key, ", value=", value, ")"});
}

std::string FilterAnnotation::ToString() const {
  std::vector<std::string> attribute_strings;
  for (const FilterAttribute& attr : attributes) {
    attribute_strings.push_back(attr.ToString());
  }
  return base::StrCat(
      {"FilterAnnotation(id=", id.AsLowercaseString(), ", task_type=",
       task_type, ", source_domain=", source_domain, ", creation_timestamp=",
       base::NumberToString(
           creation_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds()),
       ", attributes=[", base::JoinString(attribute_strings, ", "), "])"});
}

}  // namespace multistep_filter
