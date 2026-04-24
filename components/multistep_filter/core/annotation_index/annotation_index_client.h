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

struct FilterAnnotation;
struct FilterSuggestionCandidate;

// `AnnotationIndexClient` serves as the dedicated network and translation layer
// between the `multistep_filter` component and the remote
// `SiteAutomationIndexServer`.
//
// This class abstracts away the complexities of network communication and
// Protocol Buffer handling from the core `multistep_filter` logic. It
// achieves this by:
//  - Accepting standard C++ types as input and serializing them into the
//    specific Protocol Buffer format required by the backend API.
//  - Managing asynchronous network requests, including internal handling of
//    network state, timeouts, and HTTP response codes.
//  - Deserializing the raw Protocol Buffer byte stream received in the
//    response.
//  - Extracting client-relevant data from the deserialized proto and
//    packaging it into clean, lightweight C++ structs for callers.
class AnnotationIndexClient {
 public:
  // Creates a default instance of `AnnotationIndexClient`.
  static std::unique_ptr<AnnotationIndexClient> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

  virtual ~AnnotationIndexClient() = default;

  // Evaluates potential filter candidates and generates a list of
  // `FilterSuggestionCandidate`s. If no candidates were found, invokes
  // `callback` with `std::nullopt`.
  virtual void GetFilterSuggestionCandidates(
      const GURL& url,
      base::span<const FilterAnnotation> filter_annotations,
      base::OnceCallback<void(
          std::optional<std::vector<FilterSuggestionCandidate>>)> callback) = 0;

  // Retrieves the supported task types for a specific domain. If the domain is
  // not supported, invokes `callback` with `std::nullopt`.
  virtual void GetSupportedTaskTypesForDomain(
      std::string_view domain,
      base::OnceCallback<void(std::optional<std::vector<std::string>>)>
          callback) = 0;

  // Parses a raw URL to identify and extract a `FilterAnnotation`. If no
  // annotation is present, invokes `callback` with `std::nullopt`.
  virtual void ExtractFilterAnnotation(
      const GURL& url,
      base::OnceCallback<void(std::optional<FilterAnnotation>)> callback) = 0;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_H_
