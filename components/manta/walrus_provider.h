// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_WALRUS_PROVIDER_H_
#define COMPONENTS_MANTA_WALRUS_PROVIDER_H_

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

// The Walrus provider for the Manta project. Provides a method for clients to
// call the relevant google API, handling OAuth and http fetching.
// IMPORTANT: This class depends on `IdentityManager`.
// `WalrusProvider::Filter` will return an empty response after
// `IdentityManager` destruction.
class COMPONENT_EXPORT(MANTA) WalrusProvider : virtual public BaseProvider {
 public:
  // Returns a `WalrusProvider` instance tied to the profile of the passed
  // arguments.
  WalrusProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      const ProviderParams& provider_params);

  WalrusProvider(const WalrusProvider&) = delete;
  WalrusProvider& operator=(const WalrusProvider&) = delete;

  ~WalrusProvider() override;

  // Filters the given `text_prompt` and `images` by calling the google service
  // endpoint with the http POST request payload populated with the `input`. The
  // fetched response is processed and returned to the caller via an
  // `MantaGenericCallback` callback.
  // Will give an empty response if `IdentityManager` is no longer valid.
  virtual void Filter(const std::optional<std::string>& text_prompt,
                      const std::vector<std::vector<uint8_t>>& images,
                      MantaGenericCallback done_callback);

  // Filters the given `text_prompt`.
  virtual void Filter(const std::string text_prompt,
                      MantaGenericCallback done_callback);

 protected:
  WalrusProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

 private:
  friend class FakeWalrusProvider;

  base::WeakPtrFactory<WalrusProvider> weak_ptr_factory_{this};
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_WALRUS_PROVIDER_H_
