// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_SNAPPER_PROVIDER_H_
#define COMPONENTS_MANTA_SNAPPER_PROVIDER_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/base_provider.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/provider_params.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace manta {

// The Snapper provider for the Manta project. Provides a method for clients to
// call the relevant google API, handling OAuth and http fetching.
//
// IMPORTANT: This class depends on `IdentityManager`.
// `SnapperProvider::Call` will return an empty response after `IdentityManager`
// destruction.
class COMPONENT_EXPORT(MANTA) SnapperProvider : virtual public BaseProvider {
 public:
  // Returns a `SnapperProvider` instance tied to the profile of the passed
  // arguments.
  SnapperProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      const ProviderParams& provider_params);

  SnapperProvider(const SnapperProvider&) = delete;
  SnapperProvider& operator=(const SnapperProvider&) = delete;

  ~SnapperProvider() override;

  // Adds some additional metadata to the mutable request and calls the google
  // service endpoint with it as the http POST `request` and the specified
  // `traffic_annotation`. The fetched response is returned to the caller via
  // `done_callback. `done_callback` will be called with nullptr if
  // `IdentityManager` is no longer valid.
  virtual void Call(manta::proto::Request& request,
                    net::NetworkTrafficAnnotationTag traffic_annotation,
                    MantaProtoResponseCallback done_callback);

 protected:
  SnapperProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

 private:
  friend class FakeSnapperProvider;

  base::WeakPtrFactory<SnapperProvider> weak_ptr_factory_{this};
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_SNAPPER_PROVIDER_H_
