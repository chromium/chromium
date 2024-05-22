// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/anchovy_provider.h"

#include <algorithm>
#include <vector>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/base_provider.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/proto/manta.pb.h"

namespace manta {

AnchovyProvider::AnchovyProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    bool is_otr_profile_,
    const std::string& chrome_version,
    const std::string& locale)
    : BaseProvider(url_loader_factory,
                   identity_manager,
                   /*use_api_key=*/is_otr_profile_,
                   chrome_version,
                   locale) {
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
}

AnchovyProvider::~AnchovyProvider() = default;

void AnchovyProvider::GetImageDescription(ImageDescriptionRequest& request,
                                          MantaGenericCallback done_callback) {
  // TODO (b/340320912): Implement.
}

void AnchovyProvider::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (identity_manager_observation_.IsObservingSource(identity_manager)) {
    identity_manager_observation_.Reset();
  }
}

}  // namespace manta
