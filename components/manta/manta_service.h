// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_MANTA_SERVICE_H_
#define COMPONENTS_MANTA_MANTA_SERVICE_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace manta {

class OrcaProvider;
class SnapperProvider;

// The MantaService class is a KeyedService for the Chrome/ChromeOS Manta
// project. It serves two main functions:
// 1. It hands clients instances to specific providers for calling and
// interacting with google services relevant to the Manta project.
// 2. It provides utility methods for clients to query specific information
// relevant to the Manta project.
class COMPONENT_EXPORT(MANTA) MantaService : public KeyedService {
 public:
  MantaService(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      signin::IdentityManager* identity_manager);

  MantaService(const MantaService&) = delete;
  MantaService& operator=(const MantaService&) = delete;

  ~MantaService() override;

  // Returns a unique pointer to an instance of the Providers for the
  // profile associated with the MantaService instance from which this method
  // is called.
  std::unique_ptr<OrcaProvider> CreateOrcaProvider();
  virtual std::unique_ptr<SnapperProvider> CreateSnapperProvider();

  // KeyedService:
  void Shutdown() override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  raw_ptr<signin::IdentityManager> identity_manager_;
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_MANTA_SERVICE_H_
