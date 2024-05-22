// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefService;
class PrefRegistrySimple;

// This class should be used to records metrics related to sign in events.
// Some metrics might not be session bound, needing some information to be
// stored through prefs.
// Listens to the `signin::IdentityManager` for sign in events/updates.
class SigninMetricsService : public KeyedService,
                             public signin::IdentityManager::Observer {
 public:
  explicit SigninMetricsService(signin::IdentityManager& identity_manager,
                                PrefService& pref_service);
  ~SigninMetricsService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // This access point will be used to resolve the source of the reauth when
  // resolving the Sign in Pending state. It will only be set if the
  // `account_id` was in Signin Pending state, or if the state update has not
  // yet reached `SigninMetricsService` through the `signin::IdentityManager`
  // notifications. This class uses a cached value of the Signin Paused state.
  // TODO(b/341260149): This is a temporary solution, to be revisited with a
  // cleaner architecture.
  void SetReauthAccessPointIfInSigninPending(
      CoreAccountId account_id,
      signin_metrics::AccessPoint access_point);

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

 private:
  raw_ref<signin::IdentityManager> identity_manager_;
  const raw_ref<PrefService> pref_service_;

  signin_metrics::AccessPoint last_reauth_access_point_ =
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_scoped_observation_{this};
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_SERVICE_H_
