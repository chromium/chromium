// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_token_provider/site_token_provider_service.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/site_token_provider.h"

namespace site_token_provider {

SiteTokenProviderService::SiteTokenProviderService(
    signin::IdentityManager* identity_manager,
    std::unique_ptr<SiteTokenProvider> provider)
    : provider_(std::move(provider)),
      identity_manager_(identity_manager),
      allowed_domains_(
          ParseAllowlistedDomains(features::kSiteTokenAllowlist.Get())) {
  CHECK(identity_manager_);
  CHECK(provider_);

  identity_manager_observation_.Observe(identity_manager_);

  provider_->SetTokenUpdateCallback(
      base::BindRepeating(&SiteTokenProviderService::OnTokensUpdated,
                          weak_ptr_factory_.GetWeakPtr()));

  UpdateState();
}

SiteTokenProviderService::~SiteTokenProviderService() = default;

void SiteTokenProviderService::Shutdown() {
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
  refresh_timer_.Stop();
  token_cache_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SiteTokenProviderService::UpdateState() {
  refresh_timer_.Stop();

  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    token_cache_.clear();
    return;
  }

  provider_->UpdateState();
  ScheduleNextRefresh();
}

std::string SiteTokenProviderService::GetTokenForDomain(
    std::string_view domain) const {
  auto it = token_cache_.find(NormalizeDomain(domain));
  return it != token_cache_.end() ? it->second : "";
}

bool SiteTokenProviderService::IsDomainAllowlisted(
    std::string_view domain) const {
  return allowed_domains_.contains(NormalizeDomain(domain));
}

void SiteTokenProviderService::SetTokenForTesting(  // IN-TEST
    std::string_view domain,
    std::string token) {
  token_cache_[NormalizeDomain(domain)] = std::move(token);
}

base::WeakPtr<SiteTokenProviderService> SiteTokenProviderService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SiteTokenProviderService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      // Don't serve the old account's tokens while the new ones are fetched.
      token_cache_.clear();
      UpdateState();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      // Stops the refresh timer and clears the cache.
      UpdateState();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void SiteTokenProviderService::OnTokensUpdated(
    std::optional<std::map<std::string, std::string>> tokens) {
  if (!tokens || !identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return;
  }

  token_cache_.clear();
  for (const auto& [domain, token] : *tokens) {
    token_cache_[NormalizeDomain(domain)] = token;
  }
}

void SiteTokenProviderService::ScheduleNextRefresh() {
  base::TimeDelta interval = features::kSiteTokenRefreshInterval.Get();
  if (!interval.is_positive()) {
    return;
  }
  refresh_timer_.Start(FROM_HERE, base::Time::Now() + interval,
                       base::BindOnce(&SiteTokenProviderService::UpdateState,
                                      weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace site_token_provider
