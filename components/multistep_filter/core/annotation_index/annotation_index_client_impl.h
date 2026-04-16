// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_IMPL_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_IMPL_H_

#include <optional>
#include <string_view>
#include <list>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "components/version_info/channel.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
struct ResourceRequest;
}  // namespace network

namespace multistep_filter {

struct FilterAnnotation;
struct FilterSuggestionCandidate;

class AnnotationIndexClientImpl : public AnnotationIndexClient {
 public:
  AnnotationIndexClientImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel);
  ~AnnotationIndexClientImpl() override;

  // AnnotationIndexClient overrides:
  void GetFilterSuggestionCandidates(
      const GURL& url,
      base::span<const FilterAnnotation> filter_annotations,
      base::OnceCallback<
          void(std::optional<std::vector<FilterSuggestionCandidate>>)> callback)
      override;

  void GetSupportedTaskTypesForDomain(
      std::string_view domain,
      base::OnceCallback<void(std::optional<std::vector<std::string>>)>
          callback) override;

  void ExtractFilterAnnotation(
      const GURL& url,
      base::OnceCallback<void(std::optional<FilterAnnotation>)> callback)
      override;

 private:
  friend class AnnotationIndexClientImplTestApi;

  using SimpleURLLoaderList =
      std::list<std::unique_ptr<network::SimpleURLLoader>>;

  // Creates a resource POST request for the given endpoint.
  std::unique_ptr<network::ResourceRequest> CreatePostResourceRequest(
      const GURL& api_base_url,
      std::string_view endpoint) const;

  // Centralized helper to launch a network request. It creates the loader,
  // stores it in `active_url_loaders_` to keep it alive, and dispatches the
  // network request. It forwards the raw response to the provided callback.
  void ExecuteRequest(
      std::unique_ptr<network::ResourceRequest> request,
      std::string request_body,
      base::OnceCallback<void(std::optional<std::string>)> callback);

  // Invoked when `SimpleURLLoader` finishes. Cleans up the specific loader
  // from `active_url_loaders_` and forwards the raw response to the parser.
  void OnSimpleURLLoaderComplete(
      SimpleURLLoaderList::iterator loader_it,
      base::OnceCallback<void(std::optional<std::string>)> callback,
      std::optional<std::string> response_body);

  // Returns the base URL for the `SiteAutomationIndexServer` server APIs.
  GURL GetIndexServerApiBaseUrl() const;

 protected:
  AnnotationIndexClientImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string api_key);

 private:
  // The factory used to instantiate network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Holds all currently active network requests. Removing a loader from this
  // list immediately cancels its underlying network traffic.
  SimpleURLLoaderList active_url_loaders_;

  // API key to be used for the requests.
  std::string api_key_;

  // This should be kept at the end so that it is the first member to be
  // destroyed.
  base::WeakPtrFactory<AnnotationIndexClientImpl> weak_ptr_factory_{this};
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_IMPL_H_
