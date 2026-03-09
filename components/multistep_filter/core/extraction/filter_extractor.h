// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_EXTRACTION_FILTER_EXTRACTOR_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_EXTRACTION_FILTER_EXTRACTOR_H_

#include "url/gurl.h"

namespace multistep_filter {

// Service responsible for extraction of `FilterAnnotation` from raw data.
class FilterExtractor {
 public:
  FilterExtractor();

  FilterExtractor(const FilterExtractor&) = delete;
  FilterExtractor& operator=(const FilterExtractor&) = delete;

  ~FilterExtractor();

  // Parses the given url to extract a `FilterAnnotation` using the
  // `AnnotationIndexClient` and stores it to the `FilterAnnotationTable`
  // using the `FilterStore`.
  void ExtractAnnotationFromUrl(const GURL& url);
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_EXTRACTION_FILTER_EXTRACTOR_H_
