// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_NETWORK_PERSONAL_CONTEXT_MANAGER_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_NETWORK_PERSONAL_CONTEXT_MANAGER_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace personal_context {

class PersonalContextFetcher;

class PersonalContextManager final {
 public:
  PersonalContextManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  PersonalContextManager(const PersonalContextManager&) = delete;
  PersonalContextManager& operator=(const PersonalContextManager&) = delete;
  ~PersonalContextManager();

  // Fetches personal context for `feature`. Invokes `callback` on request
  // completion.
  void FetchContext(proto::ContextMemoryFeature feature,
                    const google::protobuf::MessageLite& request_metadata,
                    std::optional<base::TimeDelta> timeout,
                    FetchContextCallback callback);

  // Fetches unmasked PII entities for `request`. Invokes `callback` on request
  // completion.
  void FetchPiiEntities(const proto::FetchPiiEntitiesRequest& request,
                        std::optional<base::TimeDelta> timeout,
                        FetchPiiContextCallback callback);

  void Shutdown();

 private:
  // Identifies a PersonalContextFetcher.
  using FetcherId = size_t;

  // All active fetchers for a certain feature.
  using ActiveFeatureFetchers =
      base::flat_map<FetcherId, std::unique_ptr<PersonalContextFetcher>>;

  // Invoked when the fetch context result is available.
  void OnFetchContextResponse(
      proto::ContextMemoryFeature feature,
      FetcherId fetcher_id,
      FetchContextCallback callback,
      base::expected<const proto::FetchContextResponse, ContextMemoryError>
          fetch_response);

  // Invoked when the fetch PII entities result is available.
  void OnFetchPiiEntitiesResponse(
      proto::ContextMemoryFeature feature,
      FetcherId fetcher_id,
      FetchPiiContextCallback callback,
      base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>
          fetch_response);

  // The next available `FetcherId`. Assigned in increasing order.
  FetcherId next_fetcher_id_ = 0;

  // The active fetchers per ContextMemoryFeature.
  base::flat_map<proto::ContextMemoryFeature, ActiveFeatureFetchers>
      active_fetchers_;

  // The active PII fetchers per ContextMemoryFeature.
  base::flat_map<proto::ContextMemoryFeature, ActiveFeatureFetchers>
      active_pii_fetchers_;

  // The URL Loader Factory that will be used by the fetchers.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned IdentityManager for fetching access tokens.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<PersonalContextManager> weak_ptr_factory_{this};
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_NETWORK_PERSONAL_CONTEXT_MANAGER_H_
