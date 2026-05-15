// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FETCHER_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FETCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace personal_context {

using FetchContextResponseCallback = base::OnceCallback<void(
    base::expected<const proto::FetchContextResponse, ContextMemoryError>)>;

// Fetches personal context from the remote Context Memory Service.
class PersonalContextFetcher {
 public:
  explicit PersonalContextFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  PersonalContextFetcher(const PersonalContextFetcher&) = delete;
  PersonalContextFetcher& operator=(const PersonalContextFetcher&) = delete;
  ~PersonalContextFetcher();

  // Starts the HTTP fetch and invokes the callback with the response.
  void FetchContext(proto::ContextMemoryFeature feature,
                    signin::IdentityManager* identity_manager,
                    const google::protobuf::MessageLite& request_metadata,
                    std::optional<base::TimeDelta> timeout,
                    FetchContextResponseCallback callback);

 private:
  // Converts the request metadata to a FetchContextRequest proto.
  static proto::FetchContextRequest ToFetchContextRequest(
      proto::ContextMemoryFeature feature,
      const google::protobuf::MessageLite& request_metadata);

  // Invoked when the access token is received, to continue with the request.
  void OnAccessTokenReceived(proto::ContextMemoryFeature feature,
                             std::string_view serialized_request,
                             std::optional<base::TimeDelta> timeout,
                             std::string_view access_token);

  // URL loader completion callback.
  void OnURLLoadComplete(std::optional<std::string> response_body);

  // Used to hold the callback while the SimpleURLLoader performs the request
  // asynchronously.
  FetchContextResponseCallback fetch_callback_;

  // Holds the currently active url request.
  std::unique_ptr<network::SimpleURLLoader> active_url_loader_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PersonalContextFetcher> weak_ptr_factory_{this};
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_FETCHER_H_
