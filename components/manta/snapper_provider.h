// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_SNAPPER_PROVIDER_H_
#define COMPONENTS_MANTA_SNAPPER_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/proto/manta.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace manta {

// The Snapper provider for the Manta project. Provides a method for clients to
// call the relevant google API, handling OAuth and http fetching.
//
// IMPORTANT: This class depends on `IdentityManager`.
// `SnapperProvider::Call` will return an empty response after `IdentityManager`
// destruction.
class COMPONENT_EXPORT(MANTA) SnapperProvider
    : public signin::IdentityManager::Observer {
 public:
  // Returns a `SnapperProvider` instance tied to the profile of the passed
  // arguments.
  SnapperProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

  SnapperProvider(const SnapperProvider&) = delete;
  SnapperProvider& operator=(const SnapperProvider&) = delete;

  ~SnapperProvider() override;

  // Calls the google service endpoint with the provided request as the http
  // POST request payload. The fetched response is returned to the caller via a
  // `MantaProtoResponseCallback` callback.
  // `done_callback` will be called with nullptr if `IdentityManager` is no
  // longer valid.
  virtual void Call(const manta::proto::Request& request,
                    MantaProtoResponseCallback done_callback);

  // signin::IdentityManager::Observer:
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  friend class FakeSnapperProvider;

  // Creates and returns unique pointer to an `EndpointFetcher` initialized with
  // the provided parameters and defaults relevant to `SnapperProvider`. Virtual
  // to allow overriding in tests.
  virtual std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const GURL& url,
      const std::vector<std::string>& scopes,
      const std::string& post_data);

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<SnapperProvider> weak_ptr_factory_{this};
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_SNAPPER_PROVIDER_H_
