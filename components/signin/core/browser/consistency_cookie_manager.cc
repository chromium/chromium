// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/consistency_cookie_manager.h"

#include "base/check.h"
#include "base/time/time.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace signin {

const char ConsistencyCookieManager::kCookieName[] =
    "CHROME_ID_CONSISTENCY_STATE";
const char ConsistencyCookieManager::kCookieValueStringConsistent[] =
    "Consistent";
const char ConsistencyCookieManager::kCookieValueStringInconsistent[] =
    "Inconsistent";
const char ConsistencyCookieManager::kCookieValueStringUpdating[] = "Updating";

ConsistencyCookieManager::ScopedAccountUpdate::~ScopedAccountUpdate() {
  if (!consistency_cookie_manager_)
    return;

  DCHECK_GT(consistency_cookie_manager_->scoped_update_count_, 0);
  --consistency_cookie_manager_->scoped_update_count_;
  consistency_cookie_manager_->UpdateCookieIfNeeded();
}

ConsistencyCookieManager::ScopedAccountUpdate::ScopedAccountUpdate(
    ScopedAccountUpdate&& other) {
  consistency_cookie_manager_ = std::move(other.consistency_cookie_manager_);
  // Explicitly reset the weak pointer, in case the move operation did not.
  other.consistency_cookie_manager_.reset();
}

ConsistencyCookieManager::ScopedAccountUpdate&
ConsistencyCookieManager::ScopedAccountUpdate::operator=(
    ScopedAccountUpdate&& other) {
  if (this != &other) {
    consistency_cookie_manager_ = std::move(other.consistency_cookie_manager_);
    // Explicitly reset the weak pointer, in case the move operation did not.
    other.consistency_cookie_manager_.reset();
  }
  return *this;
}

ConsistencyCookieManager::ScopedAccountUpdate::ScopedAccountUpdate(
    base::WeakPtr<ConsistencyCookieManager> manager)
    : consistency_cookie_manager_(manager) {
  DCHECK(consistency_cookie_manager_);
  ++consistency_cookie_manager_->scoped_update_count_;
  DCHECK_GT(consistency_cookie_manager_->scoped_update_count_, 0);
  consistency_cookie_manager_->UpdateCookieIfNeeded();
}

ConsistencyCookieManager::ConsistencyCookieManager(
    SigninClient* signin_client,
    AccountReconcilor* reconcilor)
    : signin_client_(signin_client), account_reconcilor_(reconcilor) {
  DCHECK(signin_client_);
  DCHECK(account_reconcilor_);
  account_reconcilor_observation_.Observe(account_reconcilor_);
  UpdateCookieIfNeeded();
}

ConsistencyCookieManager::~ConsistencyCookieManager() = default;

ConsistencyCookieManager::ScopedAccountUpdate
ConsistencyCookieManager::CreateScopedAccountUpdate() {
  return ScopedAccountUpdate(weak_factory_.GetWeakPtr());
}

// static
void ConsistencyCookieManager::UpdateCookie(
    network::mojom::CookieManager* cookie_manager,
    CookieValue value) {
  std::string cookie_value_string;
  switch (value) {
    case CookieValue::kConsistent:
      cookie_value_string = kCookieValueStringConsistent;
      break;
    case CookieValue::kInconsistent:
      cookie_value_string = kCookieValueStringInconsistent;
      break;
    case CookieValue::kUpdating:
      cookie_value_string = kCookieValueStringUpdating;
      break;
  }
  DCHECK(!cookie_value_string.empty());

  // Update the cookie with the new value.
  base::Time now = base::Time::Now();
  base::Time expiry = now + base::Days(2 * 365);  // Two years.
  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateSanitizedCookie(
          GaiaUrls::GetInstance()->gaia_url(), kCookieName, cookie_value_string,
          GaiaUrls::GetInstance()->gaia_url().host(),
          /*path=*/"/", /*creation=*/now, /*expiration=*/expiry,
          /*last_access=*/now, /*secure=*/true, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_DEFAULT,
          /*same_party=*/false, /*partition_key=*/absl::nullopt);
  net::CookieOptions cookie_options;
  // Permit to set SameSite cookies.
  cookie_options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  cookie_manager->SetCanonicalCookie(
      *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
      cookie_options,
      network::mojom::CookieManager::SetCanonicalCookieCallback());
}

void ConsistencyCookieManager::OnStateChanged(
    signin_metrics::AccountReconcilorState state) {
  UpdateCookieIfNeeded();
}

absl::optional<ConsistencyCookieManager::CookieValue>
ConsistencyCookieManager::CalculateCookieValue() const {
  const signin_metrics::AccountReconcilorState reconcilor_state =
      account_reconcilor_->GetState();

  // Only update the cookie when the reconcilor is active.
  if (reconcilor_state == signin_metrics::ACCOUNT_RECONCILOR_INACTIVE)
    return absl::nullopt;

  // If there is a live `ScopedAccountUpdate`, return `kStateUpdating`.
  DCHECK_GE(scoped_update_count_, 0);
  if (scoped_update_count_ > 0)
    return CookieValue::kUpdating;

  // Otherwise compute the cookie based on the reconcilor state.
  switch (reconcilor_state) {
    case signin_metrics::ACCOUNT_RECONCILOR_OK:
      return CookieValue::kConsistent;
    case signin_metrics::ACCOUNT_RECONCILOR_RUNNING:
    case signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED:
      return CookieValue::kUpdating;
    case signin_metrics::ACCOUNT_RECONCILOR_ERROR:
      return CookieValue::kInconsistent;
    case signin_metrics::ACCOUNT_RECONCILOR_INACTIVE:
      // This case is already handled at the top of the function.
      NOTREACHED();
      return absl::nullopt;
  }
}

void ConsistencyCookieManager::UpdateCookieIfNeeded() {
  absl::optional<CookieValue> cookie_value = CalculateCookieValue();
  if (!cookie_value.has_value() || cookie_value == cookie_value_)
    return;
  cookie_value_ = cookie_value;
  UpdateCookie(signin_client_->GetCookieManager(), cookie_value_.value());
}

}  // namespace signin
