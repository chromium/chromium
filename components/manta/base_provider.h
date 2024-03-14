// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_BASE_PROVIDER_H_
#define COMPONENTS_MANTA_BASE_PROVIDER_H_

#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace manta {

// BaseProvider abstracts common attributes and functions, mainly about endpoint
// fetcher and authorization, to avoid duplication in particular providers.
class COMPONENT_EXPORT(MANTA) BaseProvider
    : public signin::IdentityManager::Observer {
 public:
  BaseProvider();
  BaseProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

  BaseProvider(const BaseProvider&) = delete;
  BaseProvider& operator=(const BaseProvider&) = delete;

  ~BaseProvider() override;

  // signin::IdentityManager::Observer:
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 protected:
  // Creates and returns unique pointer to an `EndpointFetcher` initialized with
  // the provided parameters and defaults relevant to Manta providers. Virtual
  // to allow overriding in tests.
  virtual std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const GURL& url,
      const std::string& oauth_consumer_name,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      const std::string& post_data);

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_BASE_PROVIDER_H_
