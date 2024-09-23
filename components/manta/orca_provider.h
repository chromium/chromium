// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_ORCA_PROVIDER_H_
#define COMPONENTS_MANTA_ORCA_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/base_provider.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/provider_params.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace manta {

// The Orca provider for the Manta project. Provides a method for clients to
// call the relevant google API, handling OAuth and http fetching.
// IMPORTANT: This class depends on `IdentityManager`.
// `OrcaProvider::Call` will return an empty response after `IdentityManager`
// destruction.
class COMPONENT_EXPORT(MANTA) OrcaProvider : virtual public BaseProvider {
 public:
  // Returns a `OrcaProvider` instance tied to the profile of the passed
  // arguments.
  OrcaProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      const ProviderParams& provider_params);

  OrcaProvider(const OrcaProvider&) = delete;
  OrcaProvider& operator=(const OrcaProvider&) = delete;

  ~OrcaProvider() override;

  // Calls the google service endpoint with the http POST request payload
  // populated with the `input` parameters.
  // The fetched response is processed and returned to the caller via an
  // `MantaGenericCallback` callback.
  // In demo mode, it uses the Google API key for authentication, otherwise uses
  // `IdentityManager`, in this case it will give an empty response if
  // `IdentityManager` is no longer valid.
  void Call(const std::map<std::string, std::string>& input,
            MantaGenericCallback done_callback);

 private:
  friend class FakeOrcaProvider;

  base::WeakPtrFactory<OrcaProvider> weak_ptr_factory_{this};
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_ORCA_PROVIDER_H_
