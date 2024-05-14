// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/consistency_cookie_manager.h"

#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
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
  consistency_cookie_manager_->UpdateCookieIfNeeded(/*force_creation=*/false);
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
  bool force_creation = consistency_cookie_manager_->scoped_update_count_ == 1;
  consistency_cookie_manager_->UpdateCookieIfNeeded(force_creation);
}

ConsistencyCookieManager::ConsistencyCookieManager(
    SigninClient* signin_client,
    AccountReconcilor* reconcilor)
    : signin_client_(signin_client),
      account_reconcilor_(reconcilor),
      account_reconcilor_state_(account_reconcilor_->GetState()) {
  DCHECK(signin_client_);
  DCHECK(account_reconcilor_);
  account_reconcilor_observation_.Observe(account_reconcilor_.get());
  UpdateCookieIfNeeded(/*force_creation=*/false);
}

ConsistencyCookieManager::~ConsistencyCookieManager() {
  DCHECK(extra_cookie_managers_.empty());
}

ConsistencyCookieManager::ScopedAccountUpdate
ConsistencyCookieManager::CreateScopedAccountUpdate() {
  return ScopedAccountUpdate(weak_factory_.GetWeakPtr());
}

void ConsistencyCookieManager::AddExtraCookieManager(
    network::mojom::CookieManager* manager) {
  DCHECK(manager);
  DCHECK(!base::Contains(extra_cookie_managers_, manager));
  extra_cookie_managers_.push_back(manager);
  if (cookie_value_ && cookie_value_ != CookieValue::kInvalid)
    SetCookieValue(manager, cookie_value_.value());
}

void ConsistencyCookieManager::RemoveExtraCookieManager(
    network::mojom::CookieManager* manager) {
  DCHECK(manager);
  DCHECK(base::Contains(extra_cookie_managers_, manager));
  std::erase(extra_cookie_managers_, manager);
}

// static
std::unique_ptr<net::CanonicalCookie>
ConsistencyCookieManager::CreateConsistencyCookie(const std::string& value) {
  DCHECK(!value.empty());
  base::Time now = base::Time::Now();
  base::Time expiry = now + base::Days(2 * 365);  // Two years.
  return net::CanonicalCookie::CreateSanitizedCookie(
      GaiaUrls::GetInstance()->gaia_url(), kCookieName, value,
      GaiaUrls::GetInstance()->gaia_url().host(),
      /*path=*/"/", /*creation=*/now, /*expiration=*/expiry,
      /*last_access=*/now, /*secure=*/true, /*httponly=*/false,
      net::CookieSameSite::STRICT_MODE, net::COOKIE_PRIORITY_DEFAULT,
      /*partition_key=*/std::nullopt, /*status=*/nullptr);
}

// static
bool ConsistencyCookieManager::IsConsistencyCookie(
    const net::CanonicalCookie& cookie) {
  return cookie.SecureAttribute() && cookie.Path() == "/" &&
         cookie.DomainWithoutDot() ==
             GaiaUrls::GetInstance()->gaia_url().host() &&
         cookie.Name() == kCookieName;
}

// static
ConsistencyCookieManager::CookieValue
ConsistencyCookieManager::ParseCookieValue(const std::string& value) {
  if (base::EqualsCaseInsensitiveASCII(value, kCookieValueStringConsistent))
    return CookieValue::kConsistent;
  if (base::EqualsCaseInsensitiveASCII(value, kCookieValueStringInconsistent))
    return CookieValue::kInconsistent;
  if (base::EqualsCaseInsensitiveASCII(value, kCookieValueStringUpdating))
    return CookieValue::kUpdating;
  return CookieValue::kInvalid;
}

// static
void ConsistencyCookieManager::SetCookieValue(
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
    case CookieValue::kInvalid:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  DCHECK(!cookie_value_string.empty());

  // Update the cookie with the new value.
  std::unique_ptr<net::CanonicalCookie> cookie =
      CreateConsistencyCookie(cookie_value_string);
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
  DCHECK_NE(state, account_reconcilor_state_);
  // If a `ScopedAccountUpdate` was created while the reconcilor was inactive,
  // it was ignored and creation was not forced. Force the creation when the
  // reconcilor becomes active.
  bool force_creation = scoped_update_count_ > 0 &&
                        account_reconcilor_state_ ==
                            signin_metrics::AccountReconcilorState::kInactive;
  account_reconcilor_state_ = state;
  UpdateCookieIfNeeded(force_creation);
}

std::optional<ConsistencyCookieManager::CookieValue>
ConsistencyCookieManager::CalculateCookieValue() const {
  // Only update the cookie when the reconcilor is active.
  if (account_reconcilor_state_ ==
      signin_metrics::AccountReconcilorState::kInactive) {
    return std::nullopt;
  }

  // If there is a live `ScopedAccountUpdate`, return `kStateUpdating`.
  DCHECK_GE(scoped_update_count_, 0);
  if (scoped_update_count_ > 0)
    return CookieValue::kUpdating;

  // Otherwise compute the cookie based on the reconcilor state.
  switch (account_reconcilor_state_) {
    case signin_metrics::AccountReconcilorState::kOk:
      return CookieValue::kConsistent;
    case signin_metrics::AccountReconcilorState::kRunning:
    case signin_metrics::AccountReconcilorState::kScheduled:
      return CookieValue::kUpdating;
    case signin_metrics::AccountReconcilorState::kError:
      return CookieValue::kInconsistent;
    case signin_metrics::AccountReconcilorState::kInactive:
      // This case is already handled at the top of the function.
      NOTREACHED_IN_MIGRATION();
      return std::nullopt;
  }
}

void ConsistencyCookieManager::UpdateCookieIfNeeded(bool force_creation) {
  std::optional<CookieValue> cookie_value = CalculateCookieValue();
  if (!cookie_value.has_value())
    return;

  DCHECK_NE(cookie_value.value(), CookieValue::kInvalid);
  if (force_creation) {
    cookie_value_ = cookie_value;
    // Cancel any ongoing operation and set the cookie immediately.
    pending_cookie_update_ = std::nullopt;
    SetCookieValue(signin_client_->GetCookieManager(), cookie_value_.value());
    for (network::mojom::CookieManager* extra_manager :
         extra_cookie_managers_) {
      SetCookieValue(extra_manager, cookie_value_.value());
    }
    return;
  }

  // When creation is not forced, skip the update if the cookie already has the
  // desired value or if it is missing, based on the last-known value. This is
  // an optimisation to avoid querying the cookie repeatedly.
  if (!cookie_value_ || cookie_value_ == cookie_value) {
    pending_cookie_update_ = std::nullopt;
    return;
  }

  // Query the cookie, and set it only if it exists and has a different value.
  // Override the pending value if there is already a query in progress,
  // otherwise start a new query.
  bool has_pending_update = pending_cookie_update_.has_value();
  pending_cookie_update_ = cookie_value;

  if (has_pending_update)
    return;

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  options.set_exclude_httponly();
  signin_client_->GetCookieManager()->GetCookieList(
      GaiaUrls::GetInstance()->gaia_url(), options,
      net::CookiePartitionKeyCollection(),
      base::BindOnce(&ConsistencyCookieManager::UpdateCookieIfExists,
                     weak_factory_.GetWeakPtr()));
}

void ConsistencyCookieManager::UpdateCookieIfExists(
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& /*excluded_cookies*/) {
  // If the current operation was canceled, return immediately. `cookie_value_`
  // must not be updated now, as there is a possible race condition between
  // `GetCookieList()` and `SetCanonicalCookie()`.
  if (!pending_cookie_update_)
    return;

  // Compute the current value of the cookie.
  auto it = base::ranges::find_if(
      cookie_list, [](const net::CookieWithAccessResult& result) {
        return IsConsistencyCookie(result.cookie);
      });
  std::optional<CookieValue> current_value =
      (it == cookie_list.cend())
          ? std::nullopt
          : std::make_optional(ParseCookieValue(it->cookie.Value()));

  CookieValue target_value = pending_cookie_update_.value();
  DCHECK_NE(target_value, CookieValue::kInvalid);
  pending_cookie_update_ = std::nullopt;
  if (!current_value || current_value.value() == target_value) {
    // The cookie does not exist or already matches.
    cookie_value_ = current_value;
    return;
  }
  cookie_value_ = target_value;
  SetCookieValue(signin_client_->GetCookieManager(), target_value);
  for (network::mojom::CookieManager* extra_manager : extra_cookie_managers_) {
    SetCookieValue(extra_manager, target_value);
  }
}

}  // namespace signin
