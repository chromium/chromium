// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BROWSER_WEB_SIGNIN_TRACKER_H_
#define COMPONENTS_SIGNIN_PUBLIC_BROWSER_WEB_SIGNIN_TRACKER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {

// Observes cookies and the state of `AccountReconcilor` and notifies the
// callback with the result of a pending web sign-in.
class WebSigninTracker : public IdentityManager::Observer,
                         public AccountReconcilor::Observer {
 public:
  // The outcome of the sign-in.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.browser
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: WebSigninTrackerResult
  enum class Result {
    // The web sign-in succeeded and the primary account is now available in
    // cookies.
    kSuccess = 0,
    // Auth error occurred.
    kAuthError = 1,
    // Other error occurred, most likely - the connection timed out.
    kOtherError = 2,
    // A timeout occurred.
    kTimeout = 3,
  };

  WebSigninTracker(IdentityManager* identity_manager,
                   AccountReconcilor* account_reconcilor,
                   CoreAccountId signin_account,
                   base::OnceCallback<void(Result)> callback,
                   std::optional<base::TimeDelta> timeout = std::nullopt);

  WebSigninTracker(const WebSigninTracker&) = delete;
  WebSigninTracker& operator=(const WebSigninTracker&) = delete;

  ~WebSigninTracker() override;

  // IdentityManager::Observer:
  void OnAccountsInCookieUpdated(
      const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnIdentityManagerShutdown(IdentityManager* identity_manager) override;

  // AccountReconcilor::Observer:
  void OnStateChanged(signin_metrics::AccountReconcilorState state) override;

 private:
  void OnTimeoutReached();
  void FinishWithResult(WebSigninTracker::Result result);

  const raw_ptr<IdentityManager> identity_manager_;
  CoreAccountId signin_account_;
  base::OnceCallback<void(Result)> callback_;
  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      identity_manager_observation_{this};
  base::ScopedObservation<AccountReconcilor, AccountReconcilor::Observer>
      account_reconcilor_observation_{this};
  base::OneShotTimer timeout_timer_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BROWSER_WEB_SIGNIN_TRACKER_H_
