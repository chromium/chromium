// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_cookie_provider/site_cookie_provider_service.h"

#include "base/logging.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/site_cookie_provider/site_cookie_provider.h"

namespace site_cookie_provider {

SiteCookieProviderService::SiteCookieProviderService(
    signin::IdentityManager* identity_manager,
    std::unique_ptr<SiteCookieProvider> provider)
    : provider_(std::move(provider)), identity_manager_(identity_manager) {
  if (identity_manager_) {
    identity_manager_->AddObserver(this);
  }

  // Trigger update on startup if already signed in.
  if (identity_manager_ &&
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    UpdateState();
  }
}

SiteCookieProviderService::~SiteCookieProviderService() = default;

void SiteCookieProviderService::Shutdown() {
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
    identity_manager_ = nullptr;
  }
}

void SiteCookieProviderService::UpdateState() {
  if (provider_) {
    provider_->UpdateState();
  }
}

void SiteCookieProviderService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      UpdateState();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      // TODO(crbug.com/494305108): Handle sign-out state.
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

}  // namespace site_cookie_provider
