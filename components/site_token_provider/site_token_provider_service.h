// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_SERVICE_H_
#define COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/wall_clock_timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_token_provider/site_token_provider.h"

namespace site_token_provider {

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

  // Fetches fresh tokens and arms the next background refresh if signed in;
  // otherwise clears the cache and cancels any pending refresh.
  void UpdateState();

  // Returns the site token for `domain` if one exists.
  virtual std::string GetTokenForDomain(std::string_view domain) const;

  // Returns true if `domain` is in the allowlist for header injection.
  bool IsDomainAllowlisted(std::string_view domain) const;

  // Populates the internal token cache directly for testing purposes.
  void SetTokenForTesting(std::string_view domain, std::string token);

  base::WeakPtr<SiteTokenProviderService> GetWeakPtr();

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

 private:
  void OnTokensUpdated(
      std::optional<std::map<std::string, std::string>> tokens);
  void ScheduleNextRefresh();

  std::unique_ptr<SiteTokenProvider> provider_;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  base::flat_set<std::string> allowed_domains_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  std::map<std::string, std::string> token_cache_;
  base::WallClockTimer refresh_timer_;

  base::WeakPtrFactory<SiteTokenProviderService> weak_ptr_factory_{this};
};

}  // namespace site_token_provider

#endif  // COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_SERVICE_H_
