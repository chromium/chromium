// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_H_

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/version_info/channel.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace multistep_filter {

class MultistepFilterLogRouter;
struct FilterAnnotation;
struct FilterSuggestionCandidate;

// An interface for querying site automation annotation indexing services and
// data sources for the `multistep_filter` component.
//
// Implementations of this interface evaluate and retrieve filter suggestion
// candidates for given URLs and annotations, determine supported task types,
// and extract filter annotations.
class AnnotationIndexClient {
 public:
  // Creates a default instance of `AnnotationIndexClient`.
  static std::unique_ptr<AnnotationIndexClient> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      MultistepFilterLogRouter* log_router);

  virtual ~AnnotationIndexClient() = default;

  // Evaluates potential filter candidates and generates a list of
  // `FilterSuggestionCandidate`s. If no candidates were found, invokes
  // `callback` with `std::nullopt`.
  virtual void GetFilterSuggestionCandidates(
      const GURL& url,
      base::span<const FilterAnnotation> filter_annotations,
      base::OnceCallback<
          void(std::optional<std::vector<FilterSuggestionCandidate>>)> callback,
      int64_t navigation_id) = 0;

  // Retrieves the supported task types for `url`. If none are supported or an
  // error occurs, invokes `callback` with an empty vector.
  virtual void GetSupportedTasks(
      const GURL& url,
      base::OnceCallback<void(std::vector<std::string>)> callback,
      int64_t navigation_id) = 0;

  // Parses a raw URL to identify and extract a `FilterAnnotation`. If no
  // annotation is present, invokes `callback` with `std::nullopt`.
  virtual void ExtractFilterAnnotation(
      const GURL& url,
      base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
      int64_t navigation_id) = 0;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_H_
