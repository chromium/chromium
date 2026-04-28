// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_FILTER_ANNOTATION_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_FILTER_ANNOTATION_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/uuid.h"

namespace multistep_filter {

// Represents a single normalized key-value attribute generated from a URL by
// `FilterExtractor`.
// Example Transformation:
//   Raw URL:  "https://shop.com/view?s=xl&c=blk"
//   Resulting `FilterAttribute`s:
//     1. { "size": "XL" }
//     2. { "color": "black" }
struct FilterAttribute {
  // The standardized, human-readable name for this attribute. (e.g., "color"
  // instead of a raw URL param like "c").
  std::string key;
  // The processed and cleaned value (e.g., "red").
  std::string value;

  FilterAttribute(std::string key, std::string value);

  FilterAttribute(const FilterAttribute&) = default;
  FilterAttribute(FilterAttribute&&) = default;
  FilterAttribute& operator=(const FilterAttribute&) = default;
  FilterAttribute& operator=(FilterAttribute&&) = default;

  ~FilterAttribute() = default;

  std::string ToString() const;

  friend bool operator==(const FilterAttribute&,
                         const FilterAttribute&) = default;
  friend auto operator<=>(const FilterAttribute&,
                          const FilterAttribute&) = default;
};

// Represents an annotation generated from a URL by `FilterExtractor`.
// It contains the set of normalized key-value attributes (`FilterAttribute`s)
// and additional metadata.
struct FilterAnnotation {
  // The UUID of the annotation.
  base::Uuid id;
  // An identifier classifying the purpose of the annotation.
  std::string task_type;
  // The eTLD+1 domain of the source URL.
  std::string source_domain;
  // The timestamp when the annotation was generated.
  base::Time creation_timestamp;
  // Set of attributes generated from the source URL.
  std::vector<FilterAttribute> attributes;

  FilterAnnotation(base::Uuid id,
                   std::string task_type,
                   std::string source_domain,
                   base::Time creation_timestamp,
                   std::vector<FilterAttribute> attributes);

  FilterAnnotation(const FilterAnnotation&);
  FilterAnnotation(FilterAnnotation&&);
  FilterAnnotation& operator=(const FilterAnnotation&);
  FilterAnnotation& operator=(FilterAnnotation&&);

  ~FilterAnnotation();

  std::string ToString() const;

  friend bool operator==(const FilterAnnotation&, const FilterAnnotation&);
  friend auto operator<=>(const FilterAnnotation&, const FilterAnnotation&);
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_FILTER_ANNOTATION_H_
