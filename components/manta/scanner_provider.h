// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_SCANNER_PROVIDER_H_
#define COMPONENTS_MANTA_SCANNER_PROVIDER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/manta/base_provider.h"
#include "components/manta/proto/scanner.pb.h"
#include "components/manta/provider_params.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace manta {

// The Scanner provider for the Manta project. Provides a method for clients to
// call the relevant google API, handling OAuth and http fetching.
// IMPORTANT: This class depends on `IdentityManager`.
// `ScannerProvider::Call` will return an empty response after `IdentityManager`
// destruction.
class COMPONENT_EXPORT(MANTA) ScannerProvider : virtual public BaseProvider {
 public:
  // Returns a `ScannerProvider` instance tied to the profile of the
  // passed arguments.
  ScannerProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      const ProviderParams& provider_params);

  ScannerProvider(const ScannerProvider&) = delete;
  ScannerProvider& operator=(const ScannerProvider&) = delete;

  ~ScannerProvider() override;

  using ScannerProtoResponseCallback =
      base::OnceCallback<void(std::unique_ptr<manta::proto::ScannerResponse>,
                              MantaStatus)>;

  // Calls the google service endpoint with the http POST request payload.
  // The fetched response is processed and returned to the caller via a
  // `MantaProtoResponseCallback` callback.
  // In demo mode, it uses the Google API key for authentication, otherwise uses
  // `IdentityManager`, in this case it will give an empty response if
  // `IdentityManager` is no longer valid.
  // TODO: b/363101024 - Determine the params to send.
  void Call(ScannerProtoResponseCallback done_callback);

 private:
  friend class FakeScannerProvider;

  base::WeakPtrFactory<ScannerProvider> weak_ptr_factory_{this};
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_SCANNER_PROVIDER_H_
