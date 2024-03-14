// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/base_provider.h"

namespace manta {
namespace {
constexpr char kHttpMethod[] = "POST";
constexpr char kHttpContentType[] = "application/x-protobuf";
constexpr char kOAuthScope[] = "https://www.googleapis.com/auth/mdi.aratea";
constexpr base::TimeDelta kTimeout = base::Seconds(30);

}  // namespace

BaseProvider::BaseProvider() = default;
BaseProvider::BaseProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(url_loader_factory) {
  CHECK(identity_manager);
  identity_manager_observation_.Observe(identity_manager);
}

BaseProvider::~BaseProvider() = default;

void BaseProvider::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (identity_manager_observation_.IsObservingSource(identity_manager)) {
    identity_manager_observation_.Reset();
  }
}

std::unique_ptr<EndpointFetcher> BaseProvider::CreateEndpointFetcher(
    const GURL& url,
    const std::string& oauth_consumer_name,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const std::string& post_data) {
  CHECK(identity_manager_observation_.IsObserving());
  const std::vector<std::string>& scopes{kOAuthScope};
  return std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*oauth_consumer_name=*/oauth_consumer_name,
      /*url=*/url,
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*scopes=*/scopes,
      /*timeout=*/kTimeout,
      /*post_data=*/post_data,
      /*annotation_tag=*/annotation_tag,
      /*identity_manager=*/identity_manager_observation_.GetSource(),
      /*consent_level=*/signin::ConsentLevel::kSignin);
}

}  // namespace manta
