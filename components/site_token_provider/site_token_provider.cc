// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_token_provider/site_token_provider.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/proto/site_token_data.pb.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace site_token_provider {

std::string NormalizeDomain(std::string_view domain) {
  std::string normalized = base::ToLowerASCII(domain);
  if (base::StartsWith(normalized, "www.", base::CompareCase::SENSITIVE)) {
    return normalized.substr(4);
  }
  return normalized;
}

base::flat_set<std::string> ParseAllowlistedDomains(
    std::string_view allowlist) {
  std::vector<std::string_view> raw_domains = base::SplitStringPiece(
      allowlist, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<std::string> normalized_domains;
  normalized_domains.reserve(raw_domains.size());
  for (std::string_view domain : raw_domains) {
    normalized_domains.push_back(NormalizeDomain(domain));
  }
  return base::flat_set<std::string>(std::move(normalized_domains));
}

namespace {

constexpr char kUsersMeResourceName[] = "users/me";
constexpr base::TimeDelta kFetchTimeout = base::Seconds(30);

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("site_token_fetcher", R"(
      semantics {
        sender: "Site Token Provider"
        description:
          "Fetches site-specific tokens to enable feature functionality on"
          " participating domains."
        trigger:
          "Triggered on startup and when primary account sign-in changes."
        data:
          "No user identifier is sent directly in the body. The request is "
          "authenticated with the user's Google OAuth credentials."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            owners: "//components/site_token_provider/OWNERS"
          }
        }
        user_data {
          type: ACCESS_TOKEN
        }
        last_reviewed: "2026-08-10"
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature is disabled by default and can be controlled via Finch "
          "or the kSiteTokenProviderEnabled feature flag."
        policy_exception_justification:
          "This feature is disabled by default and configured via Finch. "
          "No enterprise policy is planned as the feature is flag-gated and "
          "only active for authenticated signed-in users."
      }
    )");

}  // namespace

SiteTokenProvider::~SiteTokenProvider() = default;

class SiteTokenProviderImpl : public SiteTokenProvider {
 public:
  SiteTokenProviderImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : identity_manager_(identity_manager),
        url_loader_factory_(url_loader_factory) {
    CHECK(identity_manager_);
    CHECK(url_loader_factory_);
  }
  ~SiteTokenProviderImpl() override = default;

  // SiteTokenProvider:
  void SetTokenUpdateCallback(TokenUpdateCallback callback) override {
    callback_ = std::move(callback);
  }

  void UpdateState() override {
    CHECK(callback_);
    proto::GeneratePlatformSiteTokensRequest request;
    request.set_name(kUsersMeResourceName);
    std::string post_data;
    if (!request.SerializeToString(&post_data)) {
      callback_.Run({});
      return;
    }

    GURL url(features::kSiteTokenEndpointUrl.Get());
    if (!url.is_valid() || url.is_empty()) {
      callback_.Run({});
      return;
    }

    endpoint_fetcher::EndpointFetcher::RequestParams::Builder builder(
        endpoint_fetcher::HttpMethod::kPost, kTrafficAnnotation);
    builder.SetUrl(url)
        .SetContentType("application/x-protobuf")
        .SetTimeout(kFetchTimeout)
        .SetPostData(post_data)
        .SetAuthType(endpoint_fetcher::AuthType::OAUTH)
        .SetOAuthConsumerId(signin::OAuthConsumerId::kSiteTokenProvider)
        .SetConsentLevel(signin::ConsentLevel::kSignin);

    endpoint_fetcher_ = std::make_unique<endpoint_fetcher::EndpointFetcher>(
        url_loader_factory_, identity_manager_, builder.Build());

    endpoint_fetcher_->Fetch(
        base::BindOnce(&SiteTokenProviderImpl::OnFetchComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnFetchComplete(
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
    CHECK(callback_);
    endpoint_fetcher_.reset();

    std::map<std::string, std::string> tokens;
    // TODO(b/534404774): Add retry logic for 5xx errors and preserve the cache
    // on transient failures.
    if (response && response->http_status_code == net::HTTP_OK) {
      proto::GeneratePlatformSiteTokensResponse response_proto;
      if (response_proto.ParseFromString(response->response)) {
        for (const auto& token_data : response_proto.site_tokens()) {
          tokens[token_data.domain()] = token_data.token();
        }
      }
    }

    callback_.Run(std::move(tokens));
  }

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  TokenUpdateCallback callback_;
  std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher_;

  base::WeakPtrFactory<SiteTokenProviderImpl> weak_ptr_factory_{this};
};

// static
std::unique_ptr<SiteTokenProvider> SiteTokenProvider::Create(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<SiteTokenProviderImpl>(identity_manager,
                                                 url_loader_factory);
}

}  // namespace site_token_provider
