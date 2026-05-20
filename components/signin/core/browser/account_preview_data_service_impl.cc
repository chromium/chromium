// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_service_impl.h"

#include <absl/container/flat_hash_set.h>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/core/browser/account_preview_data_fetcher.h"
#include "components/signin/public/base/persistent_repeating_timer.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_utils.h"

namespace signin {

AccountPreviewDataServiceImpl::AccountPreviewDataServiceImpl(
    IdentityManager* identity_manager,
    PrefService* pref_service)
    : identity_manager_(identity_manager),
      pref_service_(CHECK_DEREF(pref_service)) {
  identity_manager_observation_.Observe(identity_manager_);

  // Load cached data from prefs at startup.
  const auto& dict = pref_service_->GetDict(prefs::kAccountPreviewDataDict);
  for (auto item : dict) {
    if (std::optional<AccountPreviewData> data =
            AccountPreviewData::Deserialize(item.second)) {
      cached_data_[GaiaId(item.first)] = std::move(data.value());
    }
  }

  repeating_timer_ = std::make_unique<PersistentRepeatingTimer>(
      &*pref_service_, prefs::kAccountPreviewDataLastUpdatePref,
      base::Hours(24),
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
  FetchAccountPreviewData(account_info.gaia);
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

  // Delete data from prefs.
  ScopedDictPrefUpdate update(&*pref_service_, prefs::kAccountPreviewDataDict);
  update->Remove(gaia_id.ToString());
}

void AccountPreviewDataServiceImpl::OnFetchCompleted(
    const GaiaId& gaia_id,
    std::optional<AccountPreviewData> data) {
  active_fetchers_.erase(gaia_id);
  if (data.has_value()) {
    cached_data_[gaia_id] = std::move(data).value();
    SaveToPrefs(gaia_id, cached_data_[gaia_id]);
  }
}

void AccountPreviewDataServiceImpl::SaveToPrefs(
    const GaiaId& gaia_id,
    const AccountPreviewData& data) {
  ScopedDictPrefUpdate update(&*pref_service_, prefs::kAccountPreviewDataDict);
  update->Set(gaia_id.ToString(), AccountPreviewData::Serialize(data));
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
  for (const std::pair<const std::string&, const base::Value&> account_prefs :
       pref_service_->GetDict(prefs::kAccountPreviewDataDict)) {
    GaiaId gaia_id(account_prefs.first);
    if (!gaia_ids_to_keep.contains(gaia_id)) {
      accounts_prefs_to_remove.push_back(std::move(gaia_id));
    }
  }

  // Remove the account prefs/data that should not be kept.
  ScopedDictPrefUpdate scoped_update(pref_service_.get(),
                                     prefs::kAccountPreviewDataDict);
  for (const GaiaId& account_prefs_to_remove : accounts_prefs_to_remove) {
    scoped_update->Remove(account_prefs_to_remove.ToString());
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

  if (active_fetchers_.find(gaia_id) != active_fetchers_.end()) {
    return;
  }

  active_fetchers_[gaia_id] = std::make_unique<AccountPreviewDataFetcher>(
      gaia_id, identity_manager_,
      base::BindOnce(&AccountPreviewDataServiceImpl::OnFetchCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace signin
