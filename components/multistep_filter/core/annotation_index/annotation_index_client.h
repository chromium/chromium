// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "url/gurl.h"

namespace multistep_filter {

struct FilterAttribute;
struct FilterAnnotation;
class UrlFilterSuggestion;

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
  virtual ~AnnotationIndexClient() = default;

  // Evaluates potential filter candidates and generates a list of
  // `UrlFilterSuggestion`s. If no suggestions are found, returns an empty
  // vector.
  virtual void GetUrlFilterSuggestions(
      const GURL& url,
      std::string_view task_type,
      base::span<const FilterAttribute> filter_attributes,
      base::OnceCallback<void(std::optional<std::vector<UrlFilterSuggestion>>)>
          callback) = 0;

  // Retrieves the supported task types for a specific domain. If the domain is
  // not supported, returns `std::nullopt`.
  virtual void GetSupportedTaskTypesForDomain(
      std::string_view domain,
      base::OnceCallback<void(std::optional<std::vector<std::string>>)>
          callback) = 0;

  // Parses a raw URL to identify and extract a `FilterAnnotation`. If no
  // annotation is present, returns `std::nullopt`.
  virtual void ExtractFilterAnnotation(
      const GURL& url,
      base::OnceCallback<void(std::optional<FilterAnnotation>)> callback) = 0;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_H_
