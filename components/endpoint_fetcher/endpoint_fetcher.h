// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENDPOINT_FETCHER_ENDPOINT_FETCHER_H_
#define COMPONENTS_ENDPOINT_FETCHER_ENDPOINT_FETCHER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/json_sanitizer.h"

namespace {

enum class CredentialsMode {
  kOmit = 0,
  kInclude = 1,
};

}  // namespace

class EndpointFetcherTest;

namespace base {
class TimeDelta;
}  // namespace base

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
}  // namespace network

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace version_info {
enum class Channel;
}

class GoogleServiceAuthError;
class GURL;

enum class FetchErrorType {
  kAuthError = 0,
  kNetError = 1,
  kResultParseError = 2,
};

struct EndpointResponse {
  std::string response;
  int http_status_code{-1};
  std::optional<FetchErrorType> error_type;
};

using EndpointFetcherCallback =
    base::OnceCallback<void(std::unique_ptr<EndpointResponse>)>;

// TODO(crbug.com/284531303) EndpointFetcher would benefit from
// re-design/rethinking the APIs.
// EndpointFetcher calls an endpoint and returns
// the response. EndpointFetcher is not thread safe and it is up to the caller
// to wait until the callback function passed to Fetch() completes
// before invoking Fetch() again.
// Destroying an EndpointFetcher will result in the in-flight request being
// cancelled.
// EndpointFetcher performs authentication via the signed in user to
// Chrome.
// If the request times out an empty response will be returned. There will also
// be an error code indicating timeout once more detailed error messaging is
// added TODO(crbug.com/40640190).
class EndpointFetcher {
 public:
  // Parameters the client can configure for the request. This is part of our
  // long term plan to move request parameters (e.g. URL, headers) to one
  // centralized struct as adding additional parameters to the EndpointFetcher
  // constructor does/will not scale. New parameters will be added here and
  // existing parameters will be migrated (crbug.com/357567879).
  struct RequestParams {
    RequestParams() = default;
    ~RequestParams() = default;

    std::optional<CredentialsMode> credentials_mode;
    std::optional<int> max_retries;

    class Builder final {
     public:
      Builder();

      Builder(const Builder&) = delete;
      Builder& operator=(const Builder&) = delete;

      ~Builder();

      RequestParams Build();

      Builder& SetCredentialsMode(const CredentialsMode& mode) {
        request_params_->credentials_mode = mode;
        return *this;
      }

      Builder& SetMaxRetries(const int retries) {
        request_params_->max_retries = retries;
        return *this;
      }

     private:
      std::unique_ptr<RequestParams> request_params_;
    };
  };

  // Preferred constructor - forms identity_manager and url_loader_factory.
  // OAUTH authentication is used for this constructor.
  EndpointFetcher(
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      const std::string& oauth_consumer_name,
      const GURL& url,
      const std::string& http_method,
      const std::string& content_type,
      const std::vector<std::string>& scopes,
      const base::TimeDelta& timeout,
      const std::string& post_data,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      signin::IdentityManager* identity_manager,
      signin::ConsentLevel consent_level);

  // Constructor if Chrome API Key is used for authentication
  EndpointFetcher(
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      const GURL& url,
      const std::string& http_method,
      const std::string& content_type,
      const base::TimeDelta& timeout,
      const std::string& post_data,
      const std::vector<std::string>& headers,
      const std::vector<std::string>& cors_exempt_headers,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      version_info::Channel channel,
      const std::optional<RequestParams> request_params = std::nullopt);

  // Constructor if no authentication is needed.
  EndpointFetcher(
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      const GURL& url,
      const net::NetworkTrafficAnnotationTag& annotation_tag);

  // Used for tests. Can be used if caller constructs their own
  // url_loader_factory and identity_manager.
  EndpointFetcher(
      const std::string& oauth_consumer_name,
      const GURL& url,
      const std::string& http_method,
      const std::string& content_type,
      const std::vector<std::string>& scopes,
      const base::TimeDelta& timeout,
      const std::string& post_data,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      signin::IdentityManager* identity_manager,
      signin::ConsentLevel consent_level);

  // This Constructor can be used in a background thread.
  EndpointFetcher(
      const GURL& url,
      const std::string& http_method,
      const std::string& content_type,
      const base::TimeDelta& timeout,
      const std::string& post_data,
      const std::vector<std::string>& headers,
      const std::vector<std::string>& cors_exempt_headers,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      bool is_oauth_fetch);

  EndpointFetcher(const EndpointFetcher& endpoint_fetcher) = delete;

  EndpointFetcher& operator=(const EndpointFetcher& endpoint_fetcher) = delete;

  virtual ~EndpointFetcher();

  // TODO(crbug.com/40642723) enable cancellation support
  virtual void Fetch(EndpointFetcherCallback callback);
  virtual void PerformRequest(EndpointFetcherCallback endpoint_fetcher_callback,
                              const char* key);

  std::string GetUrlForTesting();

 protected:
  // Used for Mock only. see MockEndpointFetcher class.
  explicit EndpointFetcher(
      const net::NetworkTrafficAnnotationTag& annotation_tag);

 private:
  friend class ::EndpointFetcherTest;
  void OnAuthTokenFetched(EndpointFetcherCallback callback,
                          GoogleServiceAuthError error,
                          signin::AccessTokenInfo access_token_info);
  void OnResponseFetched(EndpointFetcherCallback callback,
                         std::unique_ptr<std::string> response_body);
  void OnSanitizationResult(std::unique_ptr<EndpointResponse> response,
                            EndpointFetcherCallback endpoint_fetcher_callback,
                            data_decoder::JsonSanitizer::Result result);

  network::mojom::CredentialsMode GetCredentialsMode();
  int GetMaxRetries();

  enum AuthType { CHROME_API_KEY, OAUTH, NO_AUTH };
  AuthType auth_type_;

  // Members set in constructor to be passed to network::ResourceRequest or
  // network::SimpleURLLoader.
  const std::string oauth_consumer_name_;
  const GURL url_;
  const std::string http_method_;
  const std::string content_type_;
  base::TimeDelta timeout_;
  const std::string post_data_;
  const std::vector<std::string> headers_;
  const std::vector<std::string> cors_exempt_headers_;
  const net::NetworkTrafficAnnotationTag annotation_tag_;
  signin::ScopeSet oauth_scopes_;

  // Members set in constructor
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // `identity_manager_` can be null if it is not needed for authentication (in
  // this case, callers should invoke `PerformRequest` directly).
  const raw_ptr<signin::IdentityManager, AcrossTasksDanglingUntriaged>
      identity_manager_;
  // `consent_level_` is used together with `identity_manager_`, so it can be
  // null if `identity_manager_` is null.
  const std::optional<signin::ConsentLevel> consent_level_;
  bool sanitize_response_;
  version_info::Channel channel_;

  const std::optional<RequestParams> request_params_;

  // Members set in Fetch
  std::unique_ptr<const signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  base::WeakPtrFactory<EndpointFetcher> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_ENDPOINT_FETCHER_ENDPOINT_FETCHER_H_
