// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_FOOTPRINTS_PUBLIC_FPOP_SERVICE_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_FOOTPRINTS_PUBLIC_FPOP_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/contextual_search/footprints/public/proto/footprints_oneplatform.pb.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace contextual_search {

// Service interface for querying and modifying Footprints One Platform (FPOP)
// settings and activity control bits.
class FpopService {
 public:
  virtual ~FpopService() = default;

  // Factory method to create an instance of FpopService.
  static std::unique_ptr<FpopService> Create(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Fetches the user's current Activity Controls settings asynchronously.
  // The callee must fully populate the caller identity via
  // `request.header.application_id`.
  virtual void GetFacs(
      const footprints::oneplatform::GetFacsRequest& request,
      base::OnceCallback<void(bool success,
                              const footprints::oneplatform::GetFacsResponse&
                                  response)> callback) = 0;

  // Modifies the user's Activity Controls settings asynchronously.
  virtual void UpdateActivityControlsSettings(
      const footprints::oneplatform::UpdateActivityControlsSettingsRequest&
          request,
      base::OnceCallback<void(
          bool success,
          const footprints::oneplatform::UpdateActivityControlsSettingsResponse&
              response)> callback) = 0;
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_FOOTPRINTS_PUBLIC_FPOP_SERVICE_H_
