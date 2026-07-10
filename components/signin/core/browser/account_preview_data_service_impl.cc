// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_service_impl.h"

#include <absl/container/flat_hash_set.h>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/core/browser/account_preview_data_fetcher.h"
#include "components/signin/core/browser/account_preview_metrics_recorder.h"
#include "components/signin/public/base/persistent_repeating_timer.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {

AccountPreviewDataServiceImpl::AccountPreviewDataServiceImpl(
    IdentityManager* identity_manager,
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<WaitForNetworkCallbackHelper> network_delay_helper,
    version_info::Channel channel,
    const metrics::ProfileMetricsService* profile_metrics_service)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      network_delay_helper_(std::move(network_delay_helper)),
      channel_(channel),
      metrics_recorder_(*pref_service,
                        *identity_manager,
                        *profile_metrics_service) {
  CHECK(network_delay_helper_);
  identity_manager_observation_.Observe(identity_manager_);

  repeating_timer_ = std::make_unique<PersistentRepeatingTimer>(
      pref_service, prefs::kAccountPreviewDataLastUpdatePref, base::Hours(24),
      base::BindRepeating(
          &AccountPreviewDataServiceImpl::RefreshAllAccountPreviewData,
          weak_ptr_factory_.GetWeakPtr()));
  repeating_timer_->Start();
}

AccountPreviewDataServiceImpl::~AccountPreviewDataServiceImpl() = default;

AccountPreviewDataService::AccountPreviewPreference
AccountPreviewDataServiceImpl::GetPreferredAccountForPromo() const {
  AccountPreviewPreference preference;
  // TODO(crbug.com/530144650): By default we will choose the first account in
  // the cookie jar, with no preferred_data_type (this aligns with the current
  // behavior), until the heuristic is properly defined, which will end up using
  // the information from `GetAccountPreviewData()`.
  preference.preferred_data_type = std::nullopt;

  AccountsInCookieJarInfo cookie_info =
      identity_manager_->GetAccountsInCookieJar();
  if (cookie_info.AreAccountsFresh()) {
    const std::vector<gaia::ListedAccount>& accounts =
        cookie_info.GetValidSignedInAccounts();
    if (!accounts.empty()) {
      preference.gaia_id = accounts[0].gaia_id;
    }
  }
  return preference;
}

std::optional<AccountPreviewData>
AccountPreviewDataServiceImpl::GetAccountPreviewData(
    const GaiaId& gaia_id) const {
  auto it = cached_data_.find(gaia_id);
  if (it != cached_data_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void AccountPreviewDataServiceImpl::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  account_id_to_gaia_id_[account_info.account_id] = account_info.gaia;
  // This prevents startup refresh token updates from triggering unexpected
  // fetching requests. Startup should only rely on the repeating timer and
  // refresh all accounts preview data.
  if (identity_manager_->AreRefreshTokensLoaded()) {
    EnsureAllAccountsFetched();
  }
}

void AccountPreviewDataServiceImpl::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  auto it = account_id_to_gaia_id_.find(account_id);
  if (it == account_id_to_gaia_id_.end()) {
    return;
  }

  GaiaId gaia_id = it->second;
  account_id_to_gaia_id_.erase(it);

  cached_data_.erase(gaia_id);
  active_fetchers_.erase(gaia_id);

  // TODO(crbug.com/532419984): Restrict this computation if the removed account
  // is the current preferred account.
  EnsureAllAccountsFetched();
}

void AccountPreviewDataServiceImpl::SetFetchCompleteCallbackForTesting(
    base::OnceClosure callback) {
  fetch_complete_callback_for_testing_ = std::move(callback);
}

void AccountPreviewDataServiceImpl::OnFetchCompleted(
    const GaiaId& gaia_id,
    std::optional<AccountPreviewData> data) {
  bool loaded = data.has_value();
  if (loaded) {
    auto [it, inserted] =
        cached_data_.insert_or_assign(gaia_id, std::move(*data));
    metrics_recorder_.RecordMetrics(gaia_id, it->second);
  }
  active_fetchers_.erase(gaia_id);
  // `gaia_id` is owned by the fetcher and should not be used beyond this point.

  if (fetch_complete_callback_for_testing_) {
    std::move(fetch_complete_callback_for_testing_).Run();
  }
}

void AccountPreviewDataServiceImpl::OnRefreshTokensLoaded() {
  if (deferred_refresh_pending_) {
    EnsureAllAccountsFetched();
  }
}



void AccountPreviewDataServiceImpl::OnIdentityManagerShutdown(
    IdentityManager* identity_manager) {
  CHECK_EQ(identity_manager_, identity_manager);
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
}

void AccountPreviewDataServiceImpl::RefreshAllAccountPreviewData() {
  cached_data_.clear();
  EnsureAllAccountsFetched();
}

void AccountPreviewDataServiceImpl::EnsureAllAccountsFetched() {
  CHECK(identity_manager_);
  if (!identity_manager_->AreRefreshTokensLoaded()) {
    deferred_refresh_pending_ = true;
    return;
  }

  deferred_refresh_pending_ = false;
  for (const auto& account :
       identity_manager_->GetAccountsWithRefreshTokens()) {
    account_id_to_gaia_id_[account.account_id] = account.gaia;
    if (!cached_data_.contains(account.gaia)) {
      FetchAccountPreviewData(account.gaia);
    }
  }
}

void AccountPreviewDataServiceImpl::FetchAccountPreviewData(
    const GaiaId& gaia_id) {
  CHECK(identity_manager_);
  CHECK(identity_manager_->AreRefreshTokensLoaded());

  // TODO(crbug.com/510760810): Consider adding the retry logic while an active
  // fetch is already in flight and the connection is lost.
  network_delay_helper_->DelayNetworkCall(
      base::BindOnce(&AccountPreviewDataServiceImpl::StartFetch,
                     weak_ptr_factory_.GetWeakPtr(), gaia_id));
}

void AccountPreviewDataServiceImpl::StartFetch(const GaiaId& gaia_id) {
  if (active_fetchers_.contains(gaia_id)) {
    return;
  }

  CHECK(!network_delay_helper_->AreNetworkCallsDelayed());
  active_fetchers_[gaia_id] = std::make_unique<AccountPreviewDataFetcher>(
      gaia_id, identity_manager_, url_loader_factory_, channel_,
      base::BindOnce(&AccountPreviewDataServiceImpl::OnFetchCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace signin
