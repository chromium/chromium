// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_EXTRACTION_FILTER_EXTRACTOR_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_EXTRACTION_FILTER_EXTRACTOR_H_

#include <optional>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "url/gurl.h"

namespace multistep_filter {

class AnnotationIndexClient;
class FilterStore;
class MultistepFilterLogRouter;
struct FilterAnnotation;

// Responsible for extracting `FilterAnnotation`s from URLs and storing them.
// This is owned by the `MultistepFilterService`.
class FilterExtractor {
 public:
  FilterExtractor(AnnotationIndexClient& annotation_index_client,
                  FilterStore& filter_store,
                  MultistepFilterLogRouter* log_router);

  FilterExtractor(const FilterExtractor&) = delete;
  FilterExtractor& operator=(const FilterExtractor&) = delete;

  // Virtual for testing.
  virtual ~FilterExtractor();

  // Parses `url` to extract and locally store a `FilterAnnotation`. Virtual
  // for testing. Invokes `callback` with the `base::Uuid` of the successfully
  // stored annotation, or `std::nullopt` on failure.
  //
  // The extraction process follows these steps:
  // 1) Query the server via the `AnnotationIndexClient` to extract an
  //    annotation from the given `url`.
  // 2) On the extraction response (`OnAnnotationExtracted()`), if successful,
  //    store the annotation locally via the `FilterStore`, otherwise invoke the
  //    `callback` with `std::nullopt`.
  // 3) On the store response (`OnAnnotationStored()`), invoke the `callback`
  //    with the annotation ID if successfully stored, or `std::nullopt`
  //    otherwise.
  virtual void ExtractAnnotationFromUrl(
      const GURL& url,
      base::OnceCallback<void(std::optional<base::Uuid>)> callback,
      int64_t navigation_id,
      std::string_view domain);

 private:
  // See documentation of `ExtractAnnotationFromUrl()` for more details.
  void OnAnnotationExtracted(
      base::OnceCallback<void(std::optional<base::Uuid>)> callback,
      int64_t navigation_id,
      std::string_view domain,
      std::optional<FilterAnnotation> annotation);
  void OnAnnotationStored(
      base::OnceCallback<void(std::optional<base::Uuid>)> callback,
      base::Uuid annotation_id,
      int64_t navigation_id,
      std::string_view domain,
      bool success);

  // The client used to extract annotations from URLs. This is a non-owning
  // reference. The lifetime of the `AnnotationIndexClient` object is managed
  // by the `MultistepFilterService` instance that owns this
  // `FilterExtractor`.
  const base::raw_ref<AnnotationIndexClient> annotation_index_client_;

  // The store used to persist the extracted annotations. This is a non-owning
  // reference. The lifetime of the `FilterStore` object is managed by the
  // `MultistepFilterService` instance that owns this `FilterExtractor`.
  const base::raw_ref<FilterStore> filter_store_;

  // Log router for the internals page.
  const raw_ptr<MultistepFilterLogRouter> log_router_;

  // This should be kept at the end so that it is the first member to be
  // destroyed.
  base::WeakPtrFactory<FilterExtractor> weak_ptr_factory_{this};
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_EXTRACTION_FILTER_EXTRACTOR_H_
