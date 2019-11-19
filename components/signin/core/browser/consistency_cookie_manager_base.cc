// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/consistency_cookie_manager_base.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "components/signin/public/base/signin_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace signin {

const char kCookieName[] = "CHROME_ID_CONSISTENCY_STATE";
const char ConsistencyCookieManagerBase::kStateConsistent[] = "Consistent";
const char ConsistencyCookieManagerBase::kStateInconsistent[] = "Inconsistent";
const char ConsistencyCookieManagerBase::kStateUpdating[] = "Updating";

ConsistencyCookieManagerBase::ConsistencyCookieManagerBase(
    SigninClient* signin_client,
    AccountReconcilor* reconcilor)
    : account_reconcilor_state_(reconcilor->GetState()),
      signin_client_(signin_client),
      account_reconcilor_observer_(this) {
  DCHECK(signin_client_);
  DCHECK(reconcilor);

  account_reconcilor_observer_.Add(reconcilor);
}

ConsistencyCookieManagerBase::~ConsistencyCookieManagerBase() = default;

void ConsistencyCookieManagerBase::OnStateChanged(
    signin_metrics::AccountReconcilorState state) {
  if (state == account_reconcilor_state_)
    return;
  account_reconcilor_state_ = state;
  UpdateCookie();
}

std::string ConsistencyCookieManagerBase::CalculateCookieValue() {
  switch (account_reconcilor_state_) {
    case signin_metrics::ACCOUNT_RECONCILOR_OK:
      return kStateConsistent;
    case signin_metrics::ACCOUNT_RECONCILOR_RUNNING:
    case signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED:
      return kStateUpdating;
    case signin_metrics::ACCOUNT_RECONCILOR_ERROR:
      return kStateInconsistent;
    case signin_metrics::ACCOUNT_RECONCILOR_HISTOGRAM_COUNT:
      NOTREACHED();
      return {};
  }
}

void ConsistencyCookieManagerBase::UpdateCookie() {
  std::string cookie_value = CalculateCookieValue();
  DCHECK(!cookie_value.empty());

  // Update the cookie with the new value.
  network::mojom::CookieManager* cookie_manager =
      signin_client_->GetCookieManager();
  base::Time now = base::Time::Now();
  base::Time expiry = now + base::TimeDelta::FromDays(2 * 365);  // Two years.
  net::CanonicalCookie cookie(
      kCookieName, cookie_value, GaiaUrls::GetInstance()->gaia_url().host(),
      /*path=*/"/", /*creation=*/now, /*expiration=*/expiry,
      /*last_access=*/now, /*secure=*/true, /*httponly=*/false,
      net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_DEFAULT);
  net::CookieOptions cookie_options;
  // Permit to set SameSite cookies.
  cookie_options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
  cookie_manager->SetCanonicalCookie(
      cookie, "https", cookie_options,
      network::mojom::CookieManager::SetCanonicalCookieCallback());
}

}  // namespace signin
