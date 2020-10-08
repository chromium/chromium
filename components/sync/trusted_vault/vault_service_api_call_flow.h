// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_VAULT_SERVICE_API_CALL_FLOW_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_VAULT_SERVICE_API_CALL_FLOW_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "url/gurl.h"

struct CoreAccountId;

namespace signin {
struct AccessTokenInfo;
}  // namespace signin

namespace syncer {

class TrustedVaultAccessTokenFetcher;

// Allows calling VaultService API using proto-over-http.
class VaultServiceApiCallFlow : public OAuth2ApiCallFlow {
 public:
  using CompletionCallback =
      base::OnceCallback<void(bool success, const std::string& response_body)>;

  enum class HttpMethod { kGet, kPost };

  // |callback| will be run upon completion and it's allowed to delete this
  // object upon |callback| call. For GET requests, |serialized_request_proto|
  // must be null. For |POST| requests, it can be either way (optional payload).
  VaultServiceApiCallFlow(
      HttpMethod http_method,
      const GURL& request_url,
      const net::PartialNetworkTrafficAnnotationTag partial_annotation_tag,
      const base::Optional<std::string>& serialized_request_proto);
  VaultServiceApiCallFlow(const VaultServiceApiCallFlow& other) = delete;
  VaultServiceApiCallFlow& operator=(const VaultServiceApiCallFlow& other) =
      delete;
  ~VaultServiceApiCallFlow() override;

  // Attempts to fetch access token, Start() the flow if fetch is successful
  // and populate error into ResultCallback otherwise. Should be called at most
  // once.
  void FetchAccessTokenAndStartFlow(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TrustedVaultAccessTokenFetcher* access_token_fetcher,
      CompletionCallback callback);

  // Starts the flow by forwarding call to OAuth2ApiCallFlow::Start().
  // Overridden to make public class API explicit.
  // WARNING: don't call this directly and use FetchAccessTokenAndStartFlow().
  void Start(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             const std::string& access_token) override;

 protected:
  // OAuth2ApiCallFlow implementation.
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;
  std::string CreateApiCallBodyContentType() override;
  std::string GetRequestTypeForBody(const std::string& body) override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  net::PartialNetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag()
      override;

 private:
  void OnAccessTokenFetched(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::Optional<signin::AccessTokenInfo> access_token_info);

  // Running |completion_callback_| may cause destroying of this object, so all
  // callers of this method must not run any code afterwards.
  void RunCompletionCallbackAndMaybeDestroySelf(
      bool success,
      const std::string& response_body);

  const HttpMethod http_method_;
  const GURL request_url_;
  const base::Optional<std::string> serialized_request_proto_;
  const net::PartialNetworkTrafficAnnotationTag partial_annotation_tag_;

  CompletionCallback completion_callback_;

  base::WeakPtrFactory<VaultServiceApiCallFlow> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_VAULT_SERVICE_API_CALL_FLOW_H_
