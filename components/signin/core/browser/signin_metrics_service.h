// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_management_type_metrics_recorder.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefService;
class PrefRegistrySimple;

namespace signin {
class ActivePrimaryAccountsMetricsRecorder;
}

// This class should be used to records metrics related to sign in events.
// Some metrics might not be session bound, needing some information to be
// stored through prefs.
// Listens to the `signin::IdentityManager` for sign in events/updates.
class SigninMetricsService : public KeyedService,
                             public signin::IdentityManager::Observer {
 public:
  // `active_primary_accounts_metrics_recorder` may be null (this should happen
  // only in tests).
  explicit SigninMetricsService(signin::IdentityManager& identity_manager,
                                PrefService& pref_service,
                                signin::ActivePrimaryAccountsMetricsRecorder*
                                    active_primary_accounts_metrics_recorder);
  ~SigninMetricsService() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;

 private:
  // KeyedService.
  void Shutdown() override;

  // Helper handling functions for error analysis for different sign in states.
  void HandleSyncErrors(
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source);
  void HandleSigninErrors(
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

  // Returns the time of the web signin for the given account, or
  // `std::nullopt` if the `account_id` was not previously signed in on the web.
  std::optional<base::Time> GetTimeOfWebSignin(
      const CoreAccountId& account_id) const;
  void MaybeRecordMetricsForPromoShowCountAtSignin(
      const CoreAccountInfo& account_info,
      signin_metrics::AccessPoint access_point);
  void MaybeRecordWebSigninToChromeSigninMetrics(
      const CoreAccountId& account_id,
      signin_metrics::AccessPoint access_point);
  void RecordSigninInterceptionMetrics(const GaiaId& gaia_id);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  void UpdateIsManagedForAllAccounts();

  const raw_ref<signin::IdentityManager> identity_manager_;
  const raw_ref<PrefService> pref_service_;

  const raw_ptr<signin::ActivePrimaryAccountsMetricsRecorder>
      active_primary_accounts_metrics_recorder_;

  signin::AccountManagementTypeMetricsRecorder management_type_recorder_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_scoped_observation_{this};
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_SERVICE_H_
