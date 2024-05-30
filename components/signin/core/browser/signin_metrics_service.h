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

 private:
  raw_ref<signin::IdentityManager> identity_manager_;
  const raw_ref<PrefService> pref_service_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_scoped_observation_{this};
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_METRICS_SERVICE_H_
