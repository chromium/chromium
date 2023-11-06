// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/manta_service.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/manta/orca_provider.h"
#include "components/manta/snapper_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace manta {

MantaService::MantaService(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    signin::IdentityManager* identity_manager)
    : shared_url_loader_factory_(shared_url_loader_factory),
      identity_manager_(identity_manager) {}

MantaService::~MantaService() = default;

std::unique_ptr<OrcaProvider> MantaService::CreateOrcaProvider() {
  if (!identity_manager_) {
    return nullptr;
  }
  return std::make_unique<OrcaProvider>(shared_url_loader_factory_,
                                        identity_manager_);
}

std::unique_ptr<SnapperProvider> MantaService::CreateSnapperProvider() {
  if (!identity_manager_) {
    return nullptr;
  }
  return std::make_unique<SnapperProvider>(shared_url_loader_factory_,
                                           identity_manager_);
}

void MantaService::Shutdown() {
  identity_manager_ = nullptr;
}

}  // namespace manta
