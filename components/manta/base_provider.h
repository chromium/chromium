// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_BASE_PROVIDER_H_
#define COMPONENTS_MANTA_BASE_PROVIDER_H_

#include <string>

#include "base/version_info/channel.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/provider_params.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace manta {

// Base on `use_prod`, returns either the prod or autopush endpoint.
std::string GetProviderEndpoint(bool use_prod);

// BaseProvider abstracts common attributes and functions, mainly about endpoint
// fetcher and authorization, to avoid duplication in particular providers.
class COMPONENT_EXPORT(MANTA) BaseProvider
    : public signin::IdentityManager::Observer {
 public:
  BaseProvider();
  BaseProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  BaseProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      const ProviderParams& provider_params);

  BaseProvider(const BaseProvider&) = delete;
  BaseProvider& operator=(const BaseProvider&) = delete;

  ~BaseProvider() override;

  // signin::IdentityManager::Observer:
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 protected:
  // Receives a request proto, adds additional info (e.g. chrome version) to it,
  // makes calls to the server side, and invokes the `done_callback` with
  // server response.
  // Virtual to allow overriding in tests.
  virtual void RequestInternal(
      const GURL& url,
      const std::string& oauth_consumer_name,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      manta::proto::Request& request,
      const MantaMetricType metric_type,
      MantaProtoResponseCallback done_callback,
      const base::TimeDelta timeout);

  // TODO(b:333459167): they are protected because FakeBaseProvider needs to
  // access them. Try to make them private.
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

 private:
  // Creates and returns unique pointer to an `EndpointFetcher` initialized with
  // the provided parameters and defaults relevant to Manta providers.

  // Creates an EndpointFetcher with oauth-based auth.
  std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const GURL& url,
      const std::string& oauth_consumer_name,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      const std::string& post_data,
      const base::TimeDelta timeout);
  // Creates an EndpointFetcher with default API key auth.
  // If an EndpointFetcher is obtained with this function, call its
  // `PerformRequest` directly instead of `Fetch`.
  std::unique_ptr<EndpointFetcher> CreateEndpointFetcherForDemoMode(
      const GURL& url,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      const std::string& post_data,
      const base::TimeDelta timeout);

  // Useful client info for particular providers.
  const ProviderParams provider_params_;
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_BASE_PROVIDER_H_
