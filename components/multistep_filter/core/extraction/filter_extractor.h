// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_EXTRACTION_FILTER_EXTRACTOR_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_EXTRACTION_FILTER_EXTRACTOR_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace multistep_filter {

class AnnotationIndexClient;
class FilterStore;
struct FilterAnnotation;

// Responsible for extracting `FilterAnnotation`s from URLs and storing them.
// This is owned by the `MultistepFilterService`.
class FilterExtractor {
 public:
  FilterExtractor(AnnotationIndexClient& annotation_index_client,
                  FilterStore& filter_store);

  FilterExtractor(const FilterExtractor&) = delete;
  FilterExtractor& operator=(const FilterExtractor&) = delete;

  ~FilterExtractor();

  // Parses the given url to extract a `FilterAnnotation` using the
  // `AnnotationIndexClient` and stores it to the `FilterAnnotationTable`
  // using the `FilterStore`.
  void ExtractAnnotationFromUrl(const GURL& url);

 private:
  // Callback invoked when the `AnnotationIndexClient` task posted in
  // `ExtractAnnotationFromUrl()` is complete. If an annotation was
  // successfully extracted, this function persists it locally via the
  // `FilterStore`.
  void OnAnnotationExtracted(std::optional<FilterAnnotation> annotation);

  // The client used to extract annotations from URLs. This is a non-owning
  // reference. The lifetime of the `AnnotationIndexClient` object is managed
  // by the `MultistepFilterService` instance that owns this
  // `FilterExtractor`.
  const base::raw_ref<AnnotationIndexClient> annotation_index_client_;

  // The store used to persist the extracted annotations. This is a non-owning
  // reference. The lifetime of the `FilterStore` object is managed by the
  // `MultistepFilterService` instance that owns this `FilterExtractor`.
  const base::raw_ref<FilterStore> filter_store_;

  // This should be kept at the end so that it is the first member to be
  // destroyed.
  base::WeakPtrFactory<FilterExtractor> weak_ptr_factory_{this};
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_EXTRACTION_FILTER_EXTRACTOR_H_
