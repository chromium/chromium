// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_service_impl.h"

#include <absl/container/flat_hash_set.h>

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/core/browser/account_preview_data_fetcher.h"
#include "components/signin/core/browser/account_preview_metrics_recorder.h"
#include "components/signin/public/base/persistent_repeating_timer.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/sync/base/data_type.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {

namespace {
constexpr char kPreferredAccountDictGaiaIdKey[] = "gaia_id";
constexpr char kPreferredAccountDictDataTypesKey[] = "data_types";

constexpr base::TimeDelta kMinPeriodicRefreshInterval = base::Hours(12);

void RecordNonPeriodicFetchesUntilNextPeriodicRefresh(int count) {
  base::UmaHistogramCounts100(
      "Signin.AccountPreview.NonPeriodicFetchesUntilNextPeriodicRefresh",
      count);
}

void RecordSuccessfulFetchingMetrics(
    PrefService* pref_service,
    size_t account_count_total,
    size_t account_count_to_fetch,
    AccountPreviewDataServiceImpl::FetchTriggerCause cause) {
  base::UmaHistogramEnumeration(
      "Signin.AccountPreview.SuccessfulFetchTriggerCause", cause);

  CHECK_NE(account_count_total, 0u);
  CHECK_NE(account_count_to_fetch, 0u);

  int percent = (account_count_to_fetch * 100) / account_count_total;
  base::UmaHistogramPercentage("Signin.AccountPreview.PercentAccountsToFetch",
                               percent);

  switch (cause) {
    case AccountPreviewDataServiceImpl::FetchTriggerCause::kRefreshTokenUpdated:
    case AccountPreviewDataServiceImpl::FetchTriggerCause::
        kRefreshTokenRemoved: {
      int count = pref_service->GetInteger(
          prefs::kAccountPreviewNonPeriodicFetchCountPref);
      pref_service->SetInteger(prefs::kAccountPreviewNonPeriodicFetchCountPref,
                               count + 1);
      break;
    }
    case AccountPreviewDataServiceImpl::FetchTriggerCause::kPeriodicRefresh: {
      int count = pref_service->GetInteger(
          prefs::kAccountPreviewNonPeriodicFetchCountPref);
      RecordNonPeriodicFetchesUntilNextPeriodicRefresh(count);
      pref_service->ClearPref(prefs::kAccountPreviewNonPeriodicFetchCountPref);
      break;
    }
  }
}

}  // namespace

AccountPreviewDataServiceImpl::AccountPreviewDataServiceImpl(
    IdentityManager* identity_manager,
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<WaitForNetworkCallbackHelper> network_delay_helper,
    version_info::Channel channel,
    const metrics::ProfileMetricsService* profile_metrics_service)
    : identity_manager_(identity_manager),
      pref_service_(pref_service),
      url_loader_factory_(std::move(url_loader_factory)),
      network_delay_helper_(std::move(network_delay_helper)),
      channel_(channel),
      metrics_recorder_(*pref_service,
                        *identity_manager,
                        *profile_metrics_service) {
  CHECK(network_delay_helper_);
  identity_manager_observation_.Observe(identity_manager_);

  CreateAndStartRepeatingTimer();
}

AccountPreviewDataServiceImpl::~AccountPreviewDataServiceImpl() = default;

std::optional<AccountPreviewDataService::AccountPreviewPreference>
AccountPreviewDataServiceImpl::GetPreferredAccountForPromo() const {
  return ReadPreviewPreferenceFromPrefs();
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
    EnsureAllAccountsFetched(FetchTriggerCause::kRefreshTokenUpdated);
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
  if (active_fetchers_.contains(gaia_id)) {
    // `all_accounts_fetched_barrier_` relies on fecher results, so it should be
    // called before clearing the active fetcher, if available.
    if (all_accounts_fetched_barrier_) {
      all_accounts_fetched_barrier_.Run();
    }
    active_fetchers_.erase(gaia_id);
  }

  auto preferred_account = GetPreferredAccountForPromo();
  if (preferred_account && preferred_account->gaia_id == gaia_id) {
    EnsureAllAccountsFetched(FetchTriggerCause::kRefreshTokenRemoved);
  }
}

void AccountPreviewDataServiceImpl::SetFetchCompleteCallbackForTesting(
    base::OnceClosure callback) {
  fetch_complete_callback_for_testing_ = std::move(callback);
}

void AccountPreviewDataServiceImpl::SetAllDataAvailableCallbackForTesting(
    base::OnceClosure callback) {
  all_data_available_callback_for_testing_ = std::move(callback);
}

void AccountPreviewDataServiceImpl::OnSingleFetchCompleted(
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

  CHECK(!all_accounts_fetched_barrier_.is_null());
  all_accounts_fetched_barrier_.Run();

  if (fetch_complete_callback_for_testing_) {
    std::move(fetch_complete_callback_for_testing_).Run();
  }
}

void AccountPreviewDataServiceImpl::OnRefreshTokensLoaded() {
  if (deferred_fetch_on_loaded_tokens_callback_) {
    std::move(deferred_fetch_on_loaded_tokens_callback_).Run();
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
  EnsureAllAccountsFetched(FetchTriggerCause::kPeriodicRefresh);
}

void AccountPreviewDataServiceImpl::EnsureAllAccountsFetched(
    FetchTriggerCause cause) {
  CHECK(identity_manager_);
  if (!identity_manager_->AreRefreshTokensLoaded()) {
    deferred_fetch_on_loaded_tokens_callback_ =
        base::BindOnce(&AccountPreviewDataServiceImpl::EnsureAllAccountsFetched,
                       weak_ptr_factory_.GetWeakPtr(), cause);
    return;
  }

  auto accounts = identity_manager_->GetAccountsWithRefreshTokens();
  // If there are no accounts, there is no need to fetch any data.
  if (accounts.empty()) {
    if (cause == FetchTriggerCause::kPeriodicRefresh) {
      // Treat `prefs::kAccountPreviewNonPeriodicFetchCountPref` pref.
      int count = pref_service_->GetInteger(
          prefs::kAccountPreviewNonPeriodicFetchCountPref);
      // Only record when previous non-periodic fetches occurred (meaning there
      // were some valid accounts) to ensure we do not record when a profile
      // remains with no accounts for a long time.
      if (count > 0) {
        RecordNonPeriodicFetchesUntilNextPeriodicRefresh(count);
      }
      pref_service_->ClearPref(prefs::kAccountPreviewNonPeriodicFetchCountPref);
    }
    return;
  }

  base::UmaHistogramEnumeration("Signin.AccountPreview.AllFetchTriggerCause",
                                cause);

  std::vector<GaiaId> gaia_ids_to_fetch;
  for (const auto& account : accounts) {
    account_id_to_gaia_id_[account.account_id] = account.gaia;
    if (!cached_data_.contains(account.gaia)) {
      gaia_ids_to_fetch.push_back(account.gaia);
    }
  }

  if (gaia_ids_to_fetch.empty()) {
    base::UmaHistogramEnumeration(
        "Signin.AccountPreview.TriggerCauseWithAllCachesAvailable", cause);

    all_accounts_fetched_barrier_.Reset();
    OnAllFetchesCompleted(/*should_reset_periodic_timer=*/false);
    return;
  }

  RecordSuccessfulFetchingMetrics(pref_service_, accounts.size(),
                                  gaia_ids_to_fetch.size(), cause);

  // Reset the periodic timer if all data was fetched and this refresh was not
  // triggered by the periodic timer.
  bool should_reset_periodic_timer =
      (cause != FetchTriggerCause::kPeriodicRefresh) &&
      (gaia_ids_to_fetch.size() == accounts.size());

  all_accounts_fetched_barrier_ = base::BarrierClosure(
      gaia_ids_to_fetch.size(),
      base::BindOnce(&AccountPreviewDataServiceImpl::OnAllFetchesCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     should_reset_periodic_timer));

  for (const auto& gaia_id : gaia_ids_to_fetch) {
    FetchAccountPreviewData(gaia_id);
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
  // Existing fetchers will still call `all_accounts_fetched_barrier_` as
  // expected. It is safe to just ignore the request.
  if (active_fetchers_.contains(gaia_id)) {
    return;
  }

  // Ensures that the account was not removed while waiting for the network. If
  // so, do not start the fetch.
  if (!identity_manager_->HasAccountWithRefreshToken(
          CoreAccountId::FromGaiaId(gaia_id))) {
    return;
  }

  CHECK(!network_delay_helper_->AreNetworkCallsDelayed());
  active_fetchers_[gaia_id] = std::make_unique<AccountPreviewDataFetcher>(
      gaia_id, identity_manager_, url_loader_factory_, channel_,
      base::BindOnce(&AccountPreviewDataServiceImpl::OnSingleFetchCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::optional<AccountPreviewDataService::AccountPreviewPreference>
AccountPreviewDataServiceImpl::ComputePreferredAccount() const {
  CHECK(base::FeatureList::IsEnabled(
      switches::kEnableAccountPreviewPreferredAccount));

  // TODO(crbug.com/530144650): Implement heuristic to compute the preferred
  // account and preferred data types.
  return std::nullopt;
}

void AccountPreviewDataServiceImpl::OnAllFetchesCompleted(
    bool should_reset_periodic_timer) {
  all_accounts_fetched_barrier_.Reset();

  if (base::FeatureList::IsEnabled(
          switches::kEnableAccountPreviewPreferredAccount)) {
    std::optional<AccountPreviewPreference> preferred_account =
        ComputePreferredAccount();
    if (preferred_account) {
      WritePreviewPreferenceToPrefs(*preferred_account);
    } else {
      pref_service_->ClearPref(prefs::kAccountPreviewPreference);
    }
  }

  if (should_reset_periodic_timer) {
    ResetTimer();
  }

  if (all_data_available_callback_for_testing_) {
    std::move(all_data_available_callback_for_testing_).Run();
  }
}

std::optional<AccountPreviewDataService::AccountPreviewPreference>
AccountPreviewDataServiceImpl::ReadPreviewPreferenceFromPrefs() const {
  const base::DictValue& dict =
      pref_service_->GetDict(prefs::kAccountPreviewPreference);
  const std::string* gaia_id_str =
      dict.FindString(kPreferredAccountDictGaiaIdKey);
  if (!gaia_id_str || gaia_id_str->empty()) {
    return std::nullopt;
  }

  AccountPreviewPreference preference;
  preference.gaia_id = GaiaId(*gaia_id_str);

  const base::ListValue* data_types_list =
      dict.FindList(kPreferredAccountDictDataTypesKey);
  if (data_types_list) {
    for (const base::Value& val : *data_types_list) {
      if (val.is_int()) {
        syncer::DataType data_type =
            syncer::GetDataTypeFromStableIdentifier(val.GetInt());
        if (syncer::IsRealDataType(data_type)) {
          preference.preferred_data_types.push_back(data_type);
        }
      }
    }
  }
  return preference;
}

void AccountPreviewDataServiceImpl::WritePreviewPreferenceToPrefs(
    const AccountPreviewPreference& preference) {
  base::DictValue dict;
  dict.Set(kPreferredAccountDictGaiaIdKey, preference.gaia_id.ToString());
  base::ListValue data_types_list;
  for (syncer::DataType data_type : preference.preferred_data_types) {
    data_types_list.Append(syncer::DataTypeToStableIdentifier(data_type));
  }
  dict.Set(kPreferredAccountDictDataTypesKey, std::move(data_types_list));
  pref_service_->SetDict(prefs::kAccountPreviewPreference, std::move(dict));
}

void AccountPreviewDataServiceImpl::ResetTimer() {
  pref_service_->SetTime(prefs::kAccountPreviewDataLastUpdatePref,
                         base::Time::Now());
  CreateAndStartRepeatingTimer();
}

void AccountPreviewDataServiceImpl::CreateAndStartRepeatingTimer() {
  repeating_timer_ = std::make_unique<PersistentRepeatingTimer>(
      pref_service_, prefs::kAccountPreviewDataLastUpdatePref,
      std::max(switches::kAccountPreviewDataPeriodicRefreshTiming.Get(),
               kMinPeriodicRefreshInterval),
      base::BindRepeating(
          &AccountPreviewDataServiceImpl::RefreshAllAccountPreviewData,
          weak_ptr_factory_.GetWeakPtr()));
  repeating_timer_->Start();
}

}  // namespace signin
