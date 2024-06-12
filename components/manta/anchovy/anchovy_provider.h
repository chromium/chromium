// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_ANCHOVY_ANCHOVY_PROVIDER_H_
#define COMPONENTS_MANTA_ANCHOVY_ANCHOVY_PROVIDER_H_

#include <cstdint>
#include <vector>

#include "base/component_export.h"
#include "base/version_info/channel.h"
#include "components/manta/anchovy/anchovy_requests.h"
#include "components/manta/base_provider.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace manta {

// The Anchovy provider for the Manta project. Provides a method for clients to
// call the relevant google API, handling OAuth and http fetching.
class COMPONENT_EXPORT(MANTA) AnchovyProvider : public BaseProvider {
 public:
  AnchovyProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      const ProviderParams& provider_params);
  AnchovyProvider(const AnchovyProvider&) = delete;
  AnchovyProvider& operator=(const AnchovyProvider&) = delete;
  ~AnchovyProvider() override;

  virtual void GetImageDescription(
      anchovy::ImageDescriptionRequest& request,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      MantaGenericCallback callback);

 private:
  base::WeakPtrFactory<AnchovyProvider> weak_ptr_factory_{this};
};
}  // namespace manta

#endif  // COMPONENTS_MANTA_ANCHOVY_ANCHOVY_PROVIDER_H_
