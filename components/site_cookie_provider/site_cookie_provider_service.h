// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_COOKIE_PROVIDER_SITE_COOKIE_PROVIDER_SERVICE_H_
#define COMPONENTS_SITE_COOKIE_PROVIDER_SITE_COOKIE_PROVIDER_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace site_cookie_provider {

class SiteCookieProvider;

// A KeyedService that manages the lifecycle of the SiteCookieProvider.
class SiteCookieProviderService : public KeyedService,
                                  public signin::IdentityManager::Observer {
 public:
  SiteCookieProviderService(signin::IdentityManager* identity_manager,
                            std::unique_ptr<SiteCookieProvider> provider);
  ~SiteCookieProviderService() override;

  SiteCookieProviderService(const SiteCookieProviderService&) = delete;
  SiteCookieProviderService& operator=(const SiteCookieProviderService&) =
      delete;

  // KeyedService:
  void Shutdown() override;

  // Triggers local state synchronization updates.
  void UpdateState();

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

 private:
  std::unique_ptr<SiteCookieProvider> provider_;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
};

}  // namespace site_cookie_provider

#endif  // COMPONENTS_SITE_COOKIE_PROVIDER_SITE_COOKIE_PROVIDER_SERVICE_H_
