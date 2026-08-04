// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_IMPL_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_IMPL_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/core/browser/account_preview_metrics_recorder.h"
#include "components/signin/public/base/wait_for_network_callback_helper.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {

class PersistentRepeatingTimer;
class AccountPreviewDataFetcher;

// Concrete implementation of AccountPreviewDataService.
class AccountPreviewDataServiceImpl : public AccountPreviewDataService,
                                      public IdentityManager::Observer {
 public:
  // LINT.IfChange(FetchTriggerCause)
  enum class FetchTriggerCause {
    kPeriodicRefresh = 0,
    kRefreshTokenUpdated = 1,
    kRefreshTokenRemoved = 2,
    kMaxValue = kRefreshTokenRemoved,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AccountPreviewFetchTriggerCause)

  AccountPreviewDataServiceImpl(
      IdentityManager* identity_manager,
      PrefService* pref_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<WaitForNetworkCallbackHelper> network_delay_helper,
      version_info::Channel channel,
      const metrics::ProfileMetricsService* profile_metrics_service);

  AccountPreviewDataServiceImpl(const AccountPreviewDataServiceImpl&) = delete;
  AccountPreviewDataServiceImpl& operator=(
      const AccountPreviewDataServiceImpl&) = delete;

  ~AccountPreviewDataServiceImpl() override;

  // AccountPreviewDataService implementation:
  std::optional<AccountPreviewPreference> GetPreferredAccountForPromo()
      const override;

  // Retrieves the cached preview data. Exposed specifically for testing.
  // Note: This may not be available if the browser restarted and no fetch has
  // happened, which may wait until the timer is activated.
  std::optional<AccountPreviewData> GetAccountPreviewData(
      const GaiaId& gaia_id) const;

#if BUILDFLAG(IS_ANDROID)
  void UpdateExternalAppAccount(
      const std::optional<std::string>& email) override;
#endif

  bool HasActiveFetcherForTesting(const GaiaId& gaia_id) const;

  void SetFetchCompleteCallbackForTesting(base::OnceClosure callback);
  void SetAllDataAvailableCallbackForTesting(base::OnceClosure callback);

  // IdentityManager::Observer implementation:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;
  void OnIdentityManagerShutdown(IdentityManager* identity_manager) override;

 private:
  void RefreshAllAccountPreviewData();
  void EnsureAllAccountsFetched(FetchTriggerCause cause);
  void FetchAccountPreviewData(const GaiaId& gaia_id);
  void StartFetch(const GaiaId& gaia_id);
  void OnSingleFetchCompleted(const GaiaId& gaia_id,
                              std::optional<AccountPreviewData> data);
  bool HaveAccountsMutatedSinceLastFetch(
      const std::vector<CoreAccountInfo>& accounts) const;
  void RecordAccountsUsedForLastFetch();
  void OnAllFetchesCompleted(bool should_reset_periodic_timer);
  void OnSigninAllowedPrefChanged();
  void CreateAndStartRepeatingTimer();
  void ResetTimer();
  std::optional<AccountPreviewPreference> ComputePreferredAccount() const;

  void ClearMemoryData();
  void ClearStoredResults();
  void ClearAllDataAndResults();

  std::optional<AccountPreviewPreference> ReadPreferredAccountFromPrefs() const;
  // Writing `std::nullopt` as `preference` clears the pref.
  void WritePreferredAccountToPrefs(
      std::optional<AccountPreviewPreference> preference);

  raw_ptr<IdentityManager> identity_manager_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<WaitForNetworkCallbackHelper> network_delay_helper_;
  const version_info::Channel channel_;
  AccountPreviewMetricsRecorder metrics_recorder_;

  std::unique_ptr<PersistentRepeatingTimer> repeating_timer_;
  base::OnceClosure deferred_fetch_on_loaded_tokens_callback_;

  base::OnceClosure fetch_complete_callback_for_testing_;
  base::OnceClosure all_data_available_callback_for_testing_;

  absl::flat_hash_map<GaiaId, AccountPreviewData, GaiaId::Hash> cached_data_;
  absl::flat_hash_map<GaiaId,
                      std::unique_ptr<AccountPreviewDataFetcher>,
                      GaiaId::Hash>
      active_fetchers_;
  base::RepeatingClosure all_accounts_fetched_barrier_;

  // Mapping used to look up gaia_id based on account_id, used when an account
  // is removed.
  absl::flat_hash_map<CoreAccountId, GaiaId> account_id_to_gaia_id_;

  PrefChangeRegistrar pref_change_registrar_;
  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<AccountPreviewDataServiceImpl> weak_ptr_factory_{this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_IMPL_H_
