// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_SERVICE_H_
#define COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace site_token_provider {

class SiteTokenProvider;

// A KeyedService that manages the lifecycle of the SiteTokenProvider.
class SiteTokenProviderService : public KeyedService,
                                 public signin::IdentityManager::Observer {
 public:
  SiteTokenProviderService(signin::IdentityManager* identity_manager,
                           std::unique_ptr<SiteTokenProvider> provider);
  ~SiteTokenProviderService() override;

  SiteTokenProviderService(const SiteTokenProviderService&) = delete;
  SiteTokenProviderService& operator=(const SiteTokenProviderService&) = delete;

  // KeyedService:
  void Shutdown() override;

  // Triggers local state synchronization updates.
  void UpdateState();

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

 private:
  std::unique_ptr<SiteTokenProvider> provider_;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
};

}  // namespace site_token_provider

#endif  // COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_SERVICE_H_
