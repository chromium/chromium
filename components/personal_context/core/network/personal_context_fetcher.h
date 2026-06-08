// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_NETWORK_PERSONAL_CONTEXT_FETCHER_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_NETWORK_PERSONAL_CONTEXT_FETCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "url/gurl.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

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

using FetchPiiEntitiesResponseCallback = base::OnceCallback<void(
    base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>)>;

// PersonalContextFetcher is a helper class used to issue network requests to
// the backend (Context Memory Service).
//
// Lifecycle: Each instance is designed to handle exactly one active network
// request at a time (either a context fetch or a PII entities fetch).
// Callers (typically the PersonalContextManager) should instantiate a new
// fetcher for each request. If the fetcher is destroyed while a request is
// in flight, the request is cancelled.
class PersonalContextFetcher {
 public:
  PersonalContextFetcher(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  PersonalContextFetcher(const PersonalContextFetcher&) = delete;
  PersonalContextFetcher& operator=(const PersonalContextFetcher&) = delete;
  ~PersonalContextFetcher();

  // Starts the HTTP fetch and invokes the callback with the response.
  void FetchContext(proto::ContextMemoryFeature feature,
                    const google::protobuf::MessageLite& request_metadata,
                    std::optional<base::TimeDelta> timeout,
                    FetchContextResponseCallback callback);

  // Starts the HTTP fetch for PII entities and invokes the callback with the
  // response.
  void FetchPiiEntities(proto::ContextMemoryFeature feature,
                        const proto::FetchPiiEntitiesRequest& request,
                        std::optional<base::TimeDelta> timeout,
                        FetchPiiEntitiesResponseCallback callback);

 private:
  // Converts the request metadata to a FetchContextRequest proto.
  static proto::FetchContextRequest ToFetchContextRequest(
      proto::ContextMemoryFeature feature,
      const google::protobuf::MessageLite& request_metadata);

  // Invokes the active callback (if any) with the provided error.
  void RunErrorCallback(ContextMemoryError error);

  // Generic helper to launch an HTTP fetch request.
  template <typename RequestProto, typename CallbackType>
  void Fetch(proto::ContextMemoryFeature feature,
             const RequestProto& request,
             std::string_view rpc_method,
             std::optional<base::TimeDelta> timeout,
             CallbackType callback);

  // Invoked when the access token is received, to continue with the request.
  void OnAccessTokenReceived(proto::ContextMemoryFeature feature,
                             GURL endpoint_url,
                             std::string serialized_request,
                             std::optional<base::TimeDelta> timeout,
                             std::string_view access_token);

  // URL loader completion callback.
  void OnURLLoadComplete(std::optional<std::string> response_body);

  // Used to hold the callback while the SimpleURLLoader performs the request
  // asynchronously.
  std::variant<std::monostate,
               FetchContextResponseCallback,
               FetchPiiEntitiesResponseCallback>
      callback_;

  // Holds the currently active url request.
  std::unique_ptr<network::SimpleURLLoader> active_url_loader_;

  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PersonalContextFetcher> weak_ptr_factory_{this};
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_NETWORK_PERSONAL_CONTEXT_FETCHER_H_
