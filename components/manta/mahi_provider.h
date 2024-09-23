// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_MAHI_PROVIDER_H_
#define COMPONENTS_MANTA_MAHI_PROVIDER_H_

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

// The Mahi provider for the Manta project. Provides a method for clients to
// call the relevant google API, handling OAuth and http fetching.
// IMPORTANT: This class depends on `IdentityManager`.
// `MahiProvider::Call` will return an empty response after `IdentityManager`
// destruction.
class COMPONENT_EXPORT(MANTA) MahiProvider : public BaseProvider {
 public:
  // Returns a `MahiProvider` instance tied to the profile of the passed
  // arguments.
  MahiProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      const ProviderParams& provider_params);

  MahiProvider(const MahiProvider&) = delete;
  MahiProvider& operator=(const MahiProvider&) = delete;

  ~MahiProvider() override;

  // Summarizes the given `input` by calling the google service endpoint with
  // the http POST request payload populated with the `input`. The fetched
  // response is processed and returned to the caller via an
  // `MantaGenericCallback` callback.
  // Will give an empty response if `IdentityManager` is no longer valid.
  virtual void Summarize(const std::string& input,
                         const std::string& title,
                         const std::optional<std::string>& url,
                         MantaGenericCallback done_callback);

  // Similar to `Summarize` but outlines the `input`.
  void Outline(const std::string& input,
               const std::string& title,
               const std::optional<std::string>& url,
               MantaGenericCallback done_callback);

  // Virtual for testing.
  using MahiQAPair = std::pair<std::string, std::string>;
  virtual void QuestionAndAnswer(const std::string& content,
                                 const std::string& title,
                                 const std::optional<std::string>& url,
                                 const std::vector<MahiQAPair> QAHistory,
                                 const std::string& question,
                                 MantaGenericCallback done_callback);

 protected:
  MahiProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

 private:
  friend class FakeMahiProvider;

  base::WeakPtrFactory<MahiProvider> weak_ptr_factory_{this};
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_MAHI_PROVIDER_H_
