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
#include "components/signin/public/base/persistent_repeating_timer.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {

AccountPreviewDataServiceImpl::AccountPreviewDataServiceImpl(
    IdentityManager* identity_manager,
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<WaitForNetworkCallbackHelper> network_delay_helper,
    version_info::Channel channel)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      network_delay_helper_(std::move(network_delay_helper)),
      channel_(channel) {
  CHECK(network_delay_helper_);
  identity_manager_observation_.Observe(identity_manager_);

  repeating_timer_ = std::make_unique<PersistentRepeatingTimer>(
      pref_service, prefs::kAccountPreviewDataLastUpdatePref, base::Hours(24),
      base::BindRepeating(
          &AccountPreviewDataServiceImpl::RefreshAllAccountPreviewData,
          weak_ptr_factory_.GetWeakPtr()));
  repeating_timer_->Start();

  if (identity_manager_->AreRefreshTokensLoaded()) {
    OnRefreshTokensLoaded();
  }
}

AccountPreviewDataServiceImpl::~AccountPreviewDataServiceImpl() = default;

std::optional<AccountPreviewData>
AccountPreviewDataServiceImpl::GetAccountPreviewData(const GaiaId& gaia_id) {
  auto it = cached_data_.find(gaia_id);
  if (it != cached_data_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void AccountPreviewDataServiceImpl::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // This prevents startup refresh token updates from triggering unexpected
  // fetching requests. Startup should only rely on the repeating timer and
  // refresh all accounts preview data.
  if (identity_manager_->AreRefreshTokensLoaded()) {
    FetchAccountPreviewData(account_info.gaia);
  }
}

void AccountPreviewDataServiceImpl::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  AccountInfo info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  if (info.IsEmpty()) {
    return;
  }

  GaiaId gaia_id = info.gaia;
  cached_data_.erase(gaia_id);
  active_fetchers_.erase(gaia_id);
}

void AccountPreviewDataServiceImpl::SetFetchCompleteCallbackForTesting(
    base::OnceClosure callback) {
  fetch_complete_callback_for_testing_ = std::move(callback);
}

void AccountPreviewDataServiceImpl::OnFetchCompleted(
    const GaiaId& gaia_id,
    std::optional<AccountPreviewData> data) {
  if (data.has_value()) {
    // TODO(crbug.com/510760810): Metrics logging can happen here for data type
    // counts of interest.

    cached_data_[gaia_id] = std::move(data).value();
  }

  // `gaia_id` is owned by the fetcher and should not be used beyond this point.
  active_fetchers_.erase(gaia_id);

  if (fetch_complete_callback_for_testing_) {
    std::move(fetch_complete_callback_for_testing_).Run();
  }
}

void AccountPreviewDataServiceImpl::OnRefreshTokensLoaded() {
  if (deferred_refresh_pending_) {
    RefreshAllAccountPreviewData();
  }
}

void AccountPreviewDataServiceImpl::MaybeClearInvalidAccountPreviewData(
    const AccountsInCookieJarInfo& accounts_in_cookie_jar_info) {
  if (!accounts_in_cookie_jar_info.AreAccountsFresh()) {
    return;
  }

  // All accounts that have valid cookies. For those accounts, we will keep
  // their corresponding AccountPreviewData.
  const base::flat_set<GaiaId> gaia_ids_to_keep =
      GetAllGaiaIdsForKeyedPreferences(identity_manager_.get(),
                                       accounts_in_cookie_jar_info);

  // Gather all gaia_id keys that do not have valid cookies, those will have
  // their data removed in the next step.
  std::vector<GaiaId> accounts_prefs_to_remove;
  for (const auto& [gaia_id, data] : cached_data_) {
    if (!gaia_ids_to_keep.contains(gaia_id)) {
      accounts_prefs_to_remove.push_back(gaia_id);
    }
  }

  // Remove the account prefs/data that should not be kept.
  for (const GaiaId& account_prefs_to_remove : accounts_prefs_to_remove) {
    cached_data_.erase(account_prefs_to_remove);
  }
}

void AccountPreviewDataServiceImpl::OnPrimaryAccountChanged(
    const PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(ConsentLevel::kSignin)) {
    case PrimaryAccountChangeEvent::Type::kSet:
    case PrimaryAccountChangeEvent::Type::kNone:
      break;
    case PrimaryAccountChangeEvent::Type::kCleared:
      // When clearing the primary account, if the account is already removed
      // from the cookie jar, we should remove the prefs as well.
      MaybeClearInvalidAccountPreviewData(
          identity_manager_->GetAccountsInCookieJar());
      break;
  }
}

void AccountPreviewDataServiceImpl::OnAccountsInCookieUpdated(
    const AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  MaybeClearInvalidAccountPreviewData(accounts_in_cookie_jar_info);
}

void AccountPreviewDataServiceImpl::OnIdentityManagerShutdown(
    IdentityManager* identity_manager) {
  CHECK_EQ(identity_manager_, identity_manager);
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
}

void AccountPreviewDataServiceImpl::RefreshAllAccountPreviewData() {
  CHECK(identity_manager_);

  if (!identity_manager_->AreRefreshTokensLoaded()) {
    deferred_refresh_pending_ = true;
    return;
  }

  deferred_refresh_pending_ = false;
  for (const auto& account :
       identity_manager_->GetAccountsWithRefreshTokens()) {
    FetchAccountPreviewData(account.gaia);
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
