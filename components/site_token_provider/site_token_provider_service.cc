// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_token_provider/site_token_provider_service.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
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

  identity_manager_->AddObserver(this);

  provider_->SetTokenUpdateCallback(
      base::BindRepeating(&SiteTokenProviderService::OnTokensUpdated,
                          weak_ptr_factory_.GetWeakPtr()));

  // Trigger update on startup if already signed in.
  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    UpdateState();
  }
}

SiteTokenProviderService::~SiteTokenProviderService() = default;

void SiteTokenProviderService::Shutdown() {
  identity_manager_->RemoveObserver(this);
  identity_manager_ = nullptr;
}

void SiteTokenProviderService::UpdateState() {
  provider_->UpdateState();
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
      UpdateState();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      token_cache_.clear();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void SiteTokenProviderService::OnTokensUpdated(
    std::map<std::string, std::string> tokens) {
  token_cache_.clear();
  for (auto const& [domain, token] : tokens) {
    token_cache_[NormalizeDomain(domain)] = token;
  }
}

}  // namespace site_token_provider
