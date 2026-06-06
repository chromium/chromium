// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_IMPL_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_IMPL_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/version_info/channel.h"
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
  std::optional<AccountPreviewData> GetAccountPreviewData(
      const GaiaId& gaia_id) override;

  bool HasActiveFetcherForTesting(const GaiaId& gaia_id) const {
    return active_fetchers_.contains(gaia_id);
  }

  void SetFetchCompleteCallbackForTesting(base::OnceClosure callback);

  // IdentityManager::Observer implementation:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;
  void OnPrimaryAccountChanged(const PrimaryAccountChangeEvent& event) override;
  void OnAccountsInCookieUpdated(
      const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnRefreshTokensLoaded() override;
  void OnIdentityManagerShutdown(IdentityManager* identity_manager) override;

 private:
  void RefreshAllAccountPreviewData();
  void FetchAccountPreviewData(const GaiaId& gaia_id);
  void StartFetch(const GaiaId& gaia_id);
  void OnFetchCompleted(const GaiaId& gaia_id,
                        std::optional<AccountPreviewData> data);
  void MaybeClearInvalidAccountPreviewData(
      const AccountsInCookieJarInfo& accounts_in_cookie_jar_info);

  raw_ptr<IdentityManager> identity_manager_ = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<WaitForNetworkCallbackHelper> network_delay_helper_;
  const version_info::Channel channel_;
  AccountPreviewMetricsRecorder metrics_recorder_;

  std::unique_ptr<PersistentRepeatingTimer> repeating_timer_;
  bool deferred_refresh_pending_ = false;

  base::OnceClosure fetch_complete_callback_for_testing_;

  absl::flat_hash_map<GaiaId, AccountPreviewData, GaiaId::Hash> cached_data_;
  absl::flat_hash_map<GaiaId,
                      std::unique_ptr<AccountPreviewDataFetcher>,
                      GaiaId::Hash>
      active_fetchers_;

  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<AccountPreviewDataServiceImpl> weak_ptr_factory_{this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_SERVICE_IMPL_H_
