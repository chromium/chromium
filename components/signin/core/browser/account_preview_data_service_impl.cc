// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_preview_data_service_impl.h"

#include <absl/container/flat_hash_set.h>

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/core/browser/account_preview_data_fetcher.h"
#include "components/signin/core/browser/account_preview_heuristic.h"
#include "components/signin/core/browser/account_preview_metrics_recorder.h"
#include "components/signin/public/base/persistent_repeating_timer.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {

namespace {
constexpr char kPreferredAccountDictGaiaIdKey[] = "gaia_id";
constexpr char kPreferredAccountDictDataTypesKey[] = "data_types";
constexpr char kPreferredAccountDictDataTypeKey[] = "data_type";
constexpr char kPreferredAccountDictQuartileKey[] = "quartile";
constexpr char kPreferredAccountDictOtherDeviceFormFactorKey[] =
    "other_device_form_factor";
#if BUILDFLAG(IS_ANDROID)
constexpr char kExternalAppAccountDictGaiaIdKey[] = "gaia_id";
constexpr char kExternalAppAccountDictTimestampKey[] = "timestamp";
#endif

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
    case AccountPreviewDataServiceImpl::FetchTriggerCause::kRefreshTokenRemoved:
    case AccountPreviewDataServiceImpl::FetchTriggerCause::
        kRefreshTokenInvalidated:
    case AccountPreviewDataServiceImpl::FetchTriggerCause::
        kExternalAppAccountUpdated: {
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
    syncer::SyncService* sync_service,
    PrefService* local_state,
    PrefService* profile_prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<WaitForNetworkCallbackHelper> network_delay_helper,
    version_info::Channel channel,
    const metrics::ProfileMetricsService* profile_metrics_service)
    : identity_manager_(identity_manager),
      sync_service_(sync_service),
      local_state_(local_state),
      profile_prefs_(profile_prefs),
      url_loader_factory_(std::move(url_loader_factory)),
      network_delay_helper_(std::move(network_delay_helper)),
      channel_(channel),
      metrics_recorder_(*profile_prefs,
                        *identity_manager,
                        *profile_metrics_service) {
  CHECK(network_delay_helper_);
  pref_change_registrar_.Init(profile_prefs_);
  pref_change_registrar_.Add(
      prefs::kSigninAllowed,
      base::BindRepeating(
          &AccountPreviewDataServiceImpl::OnSigninAllowedPrefChanged,
          base::Unretained(this)));

  OnSigninAllowedPrefChanged();
}

AccountPreviewDataServiceImpl::~AccountPreviewDataServiceImpl() = default;

bool AccountPreviewDataServiceImpl::IsRateLimited() const {
  base::TimeDelta rate_limit_duration =
      switches::kAccountPreviewData429RateLimitDuration.Get();
  if (!rate_limit_duration.is_positive()) {
    return false;
  }

  base::Time last_429_time =
      profile_prefs_->GetTime(prefs::kAccountPreviewDataLast429TimePref);
  if (last_429_time.is_null()) {
    return false;
  }

  base::Time now = base::Time::Now();
  if (now < last_429_time) {
    return false;
  }

  return (now - last_429_time) < rate_limit_duration;
}

std::optional<AccountPreviewDataService::AccountPreviewPreference>
AccountPreviewDataServiceImpl::GetPreferredAccountForPromo() const {
  return ReadPreferredAccountFromPrefs();
}

void AccountPreviewDataServiceImpl::GetPreviewPreferenceForAccount(
    const GaiaId& gaia_id,
    base::OnceCallback<void(std::optional<AccountPreviewPreference>)>
        callback) {
  if (!base::FeatureList::IsEnabled(
          switches::kEnableAccountPreviewPreferredAccount)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto it = cached_data_.find(gaia_id);
  if (it != cached_data_.end()) {
    std::move(callback).Run(
        ComputeAccountPreviewPreference(gaia_id, it->second));
    return;
  }

  if (!identity_manager_ || !identity_manager_->AreRefreshTokensLoaded()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (IsRateLimited()) {
    base::UmaHistogramBoolean("Signin.AccountPreview.SingleRequestRateLimited",
                              true);
    std::move(callback).Run(std::nullopt);
    return;
  }

  single_pending_requests_[gaia_id].push_back(std::move(callback));

  if (!active_fetchers_.contains(gaia_id)) {
    FetchAccountPreviewData(gaia_id);
  }
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

#if BUILDFLAG(IS_ANDROID)
void AccountPreviewDataServiceImpl::UpdateExternalAppAccount(
    const std::optional<std::string>& email) {
  if (!base::FeatureList::IsEnabled(
          switches::kEnableAccountPreviewUseAppAccount)) {
    ClearExternalAppAccount();
    return;
  }

  if (!profile_prefs_->GetBoolean(prefs::kSigninAllowed)) {
    ClearExternalAppAccount();
    return;
  }

  std::optional<GaiaId> current_external_account =
      ReadExternalAppAccountFromPrefs();
  std::optional<GaiaId> new_external_account;

  if (email.has_value() && !email->empty() && identity_manager_) {
    AccountInfo account_info =
        identity_manager_->FindExtendedAccountInfoByEmailAddress(*email);
    if (!account_info.IsEmpty() && !account_info.GetGaiaId().empty()) {
      new_external_account = account_info.GetGaiaId();
    }
  }

  if (current_external_account == new_external_account) {
    if (new_external_account.has_value()) {
      // Refresh the timestamp for the existing account without re-triggering
      // preferred account computation.
      WriteExternalAppAccountToPrefs(*new_external_account, base::Time::Now());
    }
    return;
  }

  if (new_external_account.has_value()) {
    WriteExternalAppAccountToPrefs(*new_external_account, base::Time::Now());
  } else {
    ClearExternalAppAccount();
  }

  // TODO(crbug.com/547785656): Consider triggering fetches for less accounts,
  // as this account may have priority over other accounts, regardless of their
  // sync preview data.
  EnsureAllAccountsFetched(FetchTriggerCause::kExternalAppAccountUpdated);
}

std::optional<GaiaId>
AccountPreviewDataServiceImpl::GetExternalAppAccountForTesting() const {
  return ReadExternalAppAccountFromPrefs();
}
#endif

void AccountPreviewDataServiceImpl::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  // This prevents startup refresh token updates from triggering unexpected
  // fetching requests. Startup should only rely on the repeating timer and
  // refresh all accounts preview data.
  if (!identity_manager_->AreRefreshTokensLoaded()) {
    return;
  }

  if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id)) {
    // Treated in `OnErrorStateOfRefreshTokenUpdatedForAccount()`.
    return;
  }

  EnsureAllAccountsFetched(FetchTriggerCause::kRefreshTokenUpdated);
}

void AccountPreviewDataServiceImpl::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  auto it = account_id_to_gaia_id_.find(account_id);
  if (it == account_id_to_gaia_id_.end()) {
    return;
  }

  GaiaId gaia_id = it->second;
  ProcessAccountRemoval(account_id, gaia_id,
                        FetchTriggerCause::kRefreshTokenRemoved);
}

void AccountPreviewDataServiceImpl::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (error.IsPersistentError()) {
    // An account with persistent error / refresh token errors is considered a
    // removed account.
    ProcessAccountRemoval(account_info.account_id, account_info.gaia,
                          FetchTriggerCause::kRefreshTokenInvalidated);
  }
}

bool AccountPreviewDataServiceImpl::HasActiveFetcherForTesting(
    const GaiaId& gaia_id) const {
  AccountPreviewDataFetcher* fetcher =
      GetFetcherForTesting(gaia_id);  // IN-TEST
  return fetcher && fetcher->is_started();
}

AccountPreviewDataFetcher* AccountPreviewDataServiceImpl::GetFetcherForTesting(
    const GaiaId& gaia_id) const {
  auto it = active_fetchers_.find(gaia_id);
  return it != active_fetchers_.end() ? it->second.get() : nullptr;
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
    std::optional<AccountPreviewData> data,
    bool hit_429) {
  if (hit_429) {
    profile_prefs_->SetTime(prefs::kAccountPreviewDataLast429TimePref,
                            base::Time::Now());
  }

  if (data.has_value()) {
    auto [it, inserted] =
        cached_data_.insert_or_assign(gaia_id, std::move(*data));
    metrics_recorder_.RecordMetrics(gaia_id, it->second);
  }

  MaybeNotifySinglePendingRequests(gaia_id);
  NotifyBatchBarrierOnFetchCompleted(gaia_id);

  active_fetchers_.erase(gaia_id);
  // `gaia_id` is owned by the fetcher and should not be used beyond this point.

  if (fetch_complete_callback_for_testing_) {
    std::move(fetch_complete_callback_for_testing_).Run();
  }
}

void AccountPreviewDataServiceImpl::OnRefreshTokensLoaded() {
  RefreshAccountIdToGaiaIdMapping();
#if BUILDFLAG(IS_ANDROID)
  CleanUpExternalAppAccountIfExpired();
  CleanUpExternalAppAccountIfNotOnDevice();
#endif
  if (deferred_fetch_on_loaded_tokens_callback_) {
    std::move(deferred_fetch_on_loaded_tokens_callback_).Run();
  }
}

void AccountPreviewDataServiceImpl::OnIdentityManagerShutdown(
    IdentityManager* identity_manager) {
  CHECK_EQ(identity_manager_, identity_manager);
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
  repeating_timer_.reset();
  ClearMemoryData();
}

void AccountPreviewDataServiceImpl::RefreshAllAccountPreviewData() {
  // Clear data to ensure a new fresh fetch and preferred data computation is
  // performed.
  ClearAllDataAndResults();
#if BUILDFLAG(IS_ANDROID)
  CleanUpExternalAppAccountIfExpired();
  CleanUpExternalAppAccountIfNotOnDevice();
#endif
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

  if (IsRateLimited()) {
    base::UmaHistogramEnumeration(
        "Signin.AccountPreview.TriggerCauseRateLimited", cause);
    if (all_data_available_callback_for_testing_) {
      std::move(all_data_available_callback_for_testing_).Run();
    }
    return;
  }

  const std::vector<CoreAccountInfo> accounts =
      GetAccountsWithValidRefreshTokens();
  // If there are no accounts, there is no need to fetch any data.
  if (accounts.empty()) {
    ClearAllDataAndResults();
    if (cause == FetchTriggerCause::kPeriodicRefresh) {
      // Treat `prefs::kAccountPreviewNonPeriodicFetchCountPref` pref.
      int count = profile_prefs_->GetInteger(
          prefs::kAccountPreviewNonPeriodicFetchCountPref);
      // Only record when previous non-periodic fetches occurred (meaning there
      // were some valid accounts) to ensure we do not record when a profile
      // remains with no accounts for a long time.
      if (count > 0) {
        RecordNonPeriodicFetchesUntilNextPeriodicRefresh(count);
      }
      profile_prefs_->ClearPref(
          prefs::kAccountPreviewNonPeriodicFetchCountPref);
    }
    if (all_data_available_callback_for_testing_) {
      std::move(all_data_available_callback_for_testing_).Run();
    }
    return;
  }

  base::UmaHistogramEnumeration("Signin.AccountPreview.AllFetchTriggerCause",
                                cause);

  RefreshAccountIdToGaiaIdMapping();

  // Do not perform any fetch in case the previous list used to compute the
  // preferred data is exactly equivalent to the current list of accounts. This
  // will directly be false for all periodic refreshes since the previous list
  // and results are cleared during periodic refreshes.
  // In case the external app account was updated, we want to trigger a new
  // preferred account computation, so we need to bypass this optimization.
  if (cause != FetchTriggerCause::kExternalAppAccountUpdated &&
      switches::kAccountPreviewDataPersistAccounts.Get() &&
      !HaveAccountsMutatedSinceLastFetch(accounts)) {
    base::UmaHistogramEnumeration(
        "Signin.AccountPreview.TriggerCauseAccountsUnchangedSinceLastFetch",
        cause);

    all_accounts_fetched_barrier_.Reset();
    if (all_data_available_callback_for_testing_) {
      std::move(all_data_available_callback_for_testing_).Run();
    }
    return;
  }

  std::vector<GaiaId> gaia_ids_to_fetch;
  for (const auto& account : accounts) {
    if (!cached_data_.contains(account.gaia)) {
      gaia_ids_to_fetch.push_back(account.gaia);
    }
  }

  if (gaia_ids_to_fetch.empty()) {
    // This scenario may happen if an account was removed/invalidated, which
    // would cause changes to the account list used (bypassing the optimization
    // above in `HaveAccountsMutatedSinceLastFetch()`) while potentially still
    // having the previous cached used in the initial computation. So a
    // preferred account computation is needed.

    base::UmaHistogramEnumeration(
        "Signin.AccountPreview.TriggerCauseWithAllCachesAvailable", cause);

    // If there are on-going active fetches, they will complete the barrier
    // and finalize the batch when done. Otherwise, all caches are already
    // available, so immediately finalize and recompute the preferred account.
    if (active_fetchers_.empty()) {
      OnAllFetchesCompleted(/*should_reset_periodic_timer=*/false);
    }
    return;
  }

  RecordSuccessfulFetchingMetrics(profile_prefs_, accounts.size(),
                                  gaia_ids_to_fetch.size(), cause);

  // Reset the periodic timer if all data was fetched and this refresh was not
  // triggered by the periodic timer.
  bool should_reset_periodic_timer =
      (cause != FetchTriggerCause::kPeriodicRefresh) &&
      (gaia_ids_to_fetch.size() == accounts.size());

  // The barrier count must match the total number of active fetchers that will
  // exist in `active_fetchers_` (including pre-existing in-flight fetchers that
  // are not in `gaia_ids_to_fetch`), ensuring `all_accounts_fetched_barrier_`
  // remains valid until all active fetchers complete or are removed.
  size_t target_fetcher_count = active_fetchers_.size();
  for (const auto& gaia_id : gaia_ids_to_fetch) {
    if (!active_fetchers_.contains(gaia_id)) {
      target_fetcher_count++;
    }
  }

  CHECK_GT(target_fetcher_count, 0u);
  batch_gaia_ids_.clear();
  for (const auto& [id, fetcher] : active_fetchers_) {
    batch_gaia_ids_.insert(id);
  }
  for (const auto& id : gaia_ids_to_fetch) {
    batch_gaia_ids_.insert(id);
  }

  all_accounts_fetched_barrier_ = base::BarrierClosure(
      target_fetcher_count,
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

  if (active_fetchers_.contains(gaia_id)) {
    return;
  }

  active_fetchers_[gaia_id] = std::make_unique<AccountPreviewDataFetcher>(
      gaia_id, identity_manager_, url_loader_factory_, channel_,
      sync_service_ ? sync_service_->GetCurrentDeviceCacheGuidsForAllGaiaIds()
                    : base::flat_set<std::string>(),
      base::BindOnce(&AccountPreviewDataServiceImpl::OnSingleFetchCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  // TODO(crbug.com/510760810): Consider adding the retry logic while an active
  // fetch is already in flight and the connection is lost.
  network_delay_helper_->DelayNetworkCall(
      base::BindOnce(&AccountPreviewDataServiceImpl::StartFetch,
                     weak_ptr_factory_.GetWeakPtr(), gaia_id));
}

void AccountPreviewDataServiceImpl::StartFetch(const GaiaId& gaia_id) {
  // If the account's refresh token was removed while waiting for network delay,
  // `OnRefreshTokenRemovedForAccount()` ran synchronously, notified the
  // barrier, and erased the fetcher from `active_fetchers_`.
  auto it = active_fetchers_.find(gaia_id);
  if (it == active_fetchers_.end()) {
    return;
  }

  if (IsRateLimited()) {
    OnSingleFetchCompleted(gaia_id, /*data=*/std::nullopt, /*hit_429=*/false);
    return;
  }

  it->second->Start();
}

std::vector<AccountPreviewHeuristicContext>
AccountPreviewDataServiceImpl::GetHeuristicContexts() const {
  // Get candidate accounts in platform display priority order (where index 0 is
  // the platform's default account for promos).
  std::vector<AccountInfo> ordered_accounts =
      GetOrderedAccountsForDisplay(identity_manager_, local_state_);

#if BUILDFLAG(IS_ANDROID)
  std::optional<GaiaId> external_app_account =
      ReadExternalAppAccountFromPrefs();
#endif
  std::vector<AccountPreviewHeuristicContext> contexts;
  for (const AccountInfo& account : ordered_accounts) {
    auto cache_it = cached_data_.find(account.GetGaiaId());
    if (cache_it == cached_data_.end()) {
      continue;
    }

    contexts.push_back(AccountPreviewHeuristicContext{
        .gaia_id = account.GetGaiaId(),
        .preview_data = raw_ref(cache_it->second),
        .is_managed = account.IsManaged() == signin::Tribool::kTrue,
        .is_child = account.IsChildAccount() == signin::Tribool::kTrue,
#if BUILDFLAG(IS_ANDROID)
        .is_external_app_primary = external_app_account.has_value() &&
                                   *external_app_account == account.GetGaiaId(),
#else
        .is_external_app_primary = false,
#endif
    });
  }
  return contexts;
}

void AccountPreviewDataServiceImpl::ComputeAndStorePreferredAccount() {
  if (base::FeatureList::IsEnabled(
          switches::kEnableAccountPreviewPreferredAccount)) {
    std::vector<AccountPreviewHeuristicContext> contexts =
        GetHeuristicContexts();
    AccountPreviewSelectionResult result =
        ComputePreferredAccountForPromo(contexts);
    WritePreferredAccountToPrefs(result.preference);
    metrics_recorder_.RecordSelectionHeuristicResult(contexts, result);
  }
}

std::vector<CoreAccountInfo>
AccountPreviewDataServiceImpl::GetAccountsWithValidRefreshTokens() const {
  CHECK(identity_manager_);
  std::vector<CoreAccountInfo> accounts;
  for (const auto& account :
       identity_manager_->GetAccountsWithRefreshTokens()) {
    if (!identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            account.account_id)) {
      accounts.push_back(account);
    }
  }
  return accounts;
}

void AccountPreviewDataServiceImpl::RefreshAccountIdToGaiaIdMapping() {
  account_id_to_gaia_id_.clear();
  for (const auto& account : GetAccountsWithValidRefreshTokens()) {
    account_id_to_gaia_id_[account.account_id] = account.gaia;
  }
}

bool AccountPreviewDataServiceImpl::HaveAccountsMutatedSinceLastFetch(
    const std::vector<CoreAccountInfo>& accounts) const {
  absl::flat_hash_set<std::string> last_used_gaia_ids;
  for (const auto& val :
       profile_prefs_->GetList(prefs::kAccountPreviewDataLastFetchAccounts)) {
    if (const std::string* str = val.GetIfString()) {
      last_used_gaia_ids.insert(*str);
    }
  }
  if (accounts.size() != last_used_gaia_ids.size()) {
    return true;
  }
  for (const auto& account : accounts) {
    if (!last_used_gaia_ids.contains(account.gaia.ToString())) {
      return true;
    }
  }
  return false;
}

void AccountPreviewDataServiceImpl::RecordAccountsUsedForLastFetch() {
  if (switches::kAccountPreviewDataPersistAccounts.Get()) {
    base::ListValue account_list;
    for (const auto& [account_id, gaia_id] : account_id_to_gaia_id_) {
      account_list.Append(gaia_id.ToString());
    }
    profile_prefs_->SetList(prefs::kAccountPreviewDataLastFetchAccounts,
                            std::move(account_list));
  } else {
    profile_prefs_->ClearPref(prefs::kAccountPreviewDataLastFetchAccounts);
  }
}

void AccountPreviewDataServiceImpl::OnAllFetchesCompleted(
    bool should_reset_periodic_timer) {
  all_accounts_fetched_barrier_.Reset();
  batch_gaia_ids_.clear();

  RecordAccountsUsedForLastFetch();

  ComputeAndStorePreferredAccount();

  if (should_reset_periodic_timer) {
    ResetTimer();
  }

  if (all_data_available_callback_for_testing_) {
    std::move(all_data_available_callback_for_testing_).Run();
  }
}

std::optional<AccountPreviewDataService::AccountPreviewPreference>
AccountPreviewDataServiceImpl::ReadPreferredAccountFromPrefs() const {
  const base::DictValue& dict =
      profile_prefs_->GetDict(prefs::kAccountPreviewPreference);
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
      if (val.is_dict()) {
        const base::DictValue& info_dict = val.GetDict();
        const std::optional<int> dt_int =
            info_dict.FindInt(kPreferredAccountDictDataTypeKey);
        const std::optional<int> q_int =
            info_dict.FindInt(kPreferredAccountDictQuartileKey);
        if (dt_int.has_value() && q_int.has_value()) {
          syncer::DataType data_type =
              syncer::GetDataTypeFromStableIdentifier(*dt_int);
          std::optional<SyncDataQuartile> quartile =
              SyncDataQuartileFromValue(*q_int);
          if (syncer::IsRealDataType(data_type) && quartile.has_value()) {
            preference.preferred_data_types.push_back({
                .data_type = data_type,
                .quartile = *quartile,
            });
          }
        }
      }
    }
  }

  std::optional<int> form_factor_int =
      dict.FindInt(kPreferredAccountDictOtherDeviceFormFactorKey);
  if (form_factor_int.has_value() &&
      sync_pb::SyncEnums::DeviceFormFactor_IsValid(*form_factor_int)) {
    preference.other_device_form_factor =
        static_cast<sync_pb::SyncEnums_DeviceFormFactor>(*form_factor_int);
  }

  return preference;
}

void AccountPreviewDataServiceImpl::WritePreferredAccountToPrefs(
    std::optional<AccountPreviewPreference> preference) {
  if (!preference.has_value()) {
    profile_prefs_->ClearPref(prefs::kAccountPreviewPreference);
    return;
  }

  base::DictValue dict;
  dict.Set(kPreferredAccountDictGaiaIdKey, preference->gaia_id.ToString());
  base::ListValue data_types_list;
  for (const PreferredDataTypeInfo& info : preference->preferred_data_types) {
    base::DictValue info_dict;
    info_dict.Set(kPreferredAccountDictDataTypeKey,
                  syncer::DataTypeToStableIdentifier(info.data_type));
    info_dict.Set(kPreferredAccountDictQuartileKey,
                  SyncDataQuartileToValue(info.quartile));
    data_types_list.Append(std::move(info_dict));
  }
  dict.Set(kPreferredAccountDictDataTypesKey, std::move(data_types_list));
  dict.Set(kPreferredAccountDictOtherDeviceFormFactorKey,
           static_cast<int>(preference->other_device_form_factor));
  profile_prefs_->SetDict(prefs::kAccountPreviewPreference, std::move(dict));
}

void AccountPreviewDataServiceImpl::ResetTimer() {
  profile_prefs_->SetTime(prefs::kAccountPreviewDataLastUpdatePref,
                          base::Time::Now());
  CreateAndStartRepeatingTimer();
}

void AccountPreviewDataServiceImpl::OnSigninAllowedPrefChanged() {
  if (profile_prefs_->GetBoolean(prefs::kSigninAllowed)) {
    if (!identity_manager_observation_.IsObserving()) {
      identity_manager_observation_.Observe(identity_manager_);
      CreateAndStartRepeatingTimer();
#if BUILDFLAG(IS_ANDROID)
      CleanUpExternalAppAccountIfExpired();
#endif
      if (identity_manager_->AreRefreshTokensLoaded()) {
        RefreshAccountIdToGaiaIdMapping();
#if BUILDFLAG(IS_ANDROID)
        CleanUpExternalAppAccountIfNotOnDevice();
#endif
      }
    }
    return;
  }

  identity_manager_observation_.Reset();
  repeating_timer_.reset();
#if BUILDFLAG(IS_ANDROID)
  ClearExternalAppAccount();
#endif
  ClearAllDataAndResults();
}

void AccountPreviewDataServiceImpl::CreateAndStartRepeatingTimer() {
  repeating_timer_ = std::make_unique<PersistentRepeatingTimer>(
      profile_prefs_, prefs::kAccountPreviewDataLastUpdatePref,
      std::max(switches::kAccountPreviewDataPeriodicRefreshTiming.Get(),
               kMinPeriodicRefreshInterval),
      base::BindRepeating(
          &AccountPreviewDataServiceImpl::RefreshAllAccountPreviewData,
          weak_ptr_factory_.GetWeakPtr()));
  repeating_timer_->Start();
}

void AccountPreviewDataServiceImpl::NotifyBatchBarrierOnFetchCompleted(
    const GaiaId& gaia_id) {
  if (batch_gaia_ids_.erase(gaia_id)) {
    CHECK(all_accounts_fetched_barrier_);
    all_accounts_fetched_barrier_.Run();
  }
}

void AccountPreviewDataServiceImpl::MaybeNotifySinglePendingRequests(
    const GaiaId& gaia_id) {
  auto it = single_pending_requests_.find(gaia_id);
  if (it == single_pending_requests_.end() || it->second.empty()) {
    return;
  }

  std::optional<AccountPreviewPreference> preference;
  auto cache_it = cached_data_.find(gaia_id);
  if (cache_it != cached_data_.end()) {
    preference = ComputeAccountPreviewPreference(gaia_id, cache_it->second);
  }

  auto callbacks = std::move(it->second);
  single_pending_requests_.erase(it);
  for (auto& cb : callbacks) {
    std::move(cb).Run(preference);
  }
}

void AccountPreviewDataServiceImpl::ClearAllSinglePendingRequests() {
  for (auto& [gaia_id, callbacks] : single_pending_requests_) {
    for (auto& cb : callbacks) {
      std::move(cb).Run(std::nullopt);
    }
  }
  single_pending_requests_.clear();
}

void AccountPreviewDataServiceImpl::ProcessAccountRemoval(
    const CoreAccountId& account_id,
    const GaiaId& gaia_id,
    FetchTriggerCause trigger_cause) {
  account_id_to_gaia_id_.erase(account_id);
  if (account_id_to_gaia_id_.empty()) {
    profile_prefs_->ClearPref(prefs::kAccountPreviewDataLastFetchAccounts);
  }

#if BUILDFLAG(IS_ANDROID)
  std::optional<GaiaId> external_app_account_gaia_id =
      ReadExternalAppAccountFromPrefs();
  if (external_app_account_gaia_id.has_value() &&
      *external_app_account_gaia_id == gaia_id) {
    ClearExternalAppAccount();
  }
#endif

  cached_data_.erase(gaia_id);
  if (active_fetchers_.contains(gaia_id)) {
    MaybeNotifySinglePendingRequests(gaia_id);
    // `all_accounts_fetched_barrier_` relies on fetcher results, so it should
    // be called before clearing the active fetcher.
    NotifyBatchBarrierOnFetchCompleted(gaia_id);
    active_fetchers_.erase(gaia_id);
  }

  auto preferred_account = GetPreferredAccountForPromo();
  if (preferred_account && preferred_account->gaia_id == gaia_id) {
    WritePreferredAccountToPrefs(/*preference=*/std::nullopt);
    EnsureAllAccountsFetched(trigger_cause);
  }
}

void AccountPreviewDataServiceImpl::ClearMemoryData() {
  cached_data_.clear();
  active_fetchers_.clear();
  ClearAllSinglePendingRequests();
  all_accounts_fetched_barrier_.Reset();
  batch_gaia_ids_.clear();
  account_id_to_gaia_id_.clear();
  deferred_fetch_on_loaded_tokens_callback_.Reset();
}

void AccountPreviewDataServiceImpl::ClearStoredResults() {
  profile_prefs_->ClearPref(prefs::kAccountPreviewDataLastFetchAccounts);
  WritePreferredAccountToPrefs(/*preference=*/std::nullopt);
}

void AccountPreviewDataServiceImpl::ClearAllDataAndResults() {
  ClearMemoryData();
  ClearStoredResults();
}

#if BUILDFLAG(IS_ANDROID)
std::optional<GaiaId>
AccountPreviewDataServiceImpl::ReadExternalAppAccountFromPrefs() const {
  if (!base::FeatureList::IsEnabled(
          switches::kEnableAccountPreviewUseAppAccount)) {
    return std::nullopt;
  }

  const base::DictValue& dict =
      profile_prefs_->GetDict(prefs::kAccountPreviewExternalAppAccount);
  const std::string* gaia_id_str =
      dict.FindString(kExternalAppAccountDictGaiaIdKey);
  if (!gaia_id_str || gaia_id_str->empty()) {
    return std::nullopt;
  }

  const base::Value* time_val = dict.Find(kExternalAppAccountDictTimestampKey);
  if (!time_val) {
    return std::nullopt;
  }

  std::optional<base::Time> last_update = base::ValueToTime(time_val);
  if (!last_update.has_value()) {
    return std::nullopt;
  }

  if (base::Time::Now() - *last_update >
      switches::kAccountPreviewAppAccountExpirationDuration.Get()) {
    return std::nullopt;
  }

  return GaiaId(*gaia_id_str);
}

void AccountPreviewDataServiceImpl::WriteExternalAppAccountToPrefs(
    const GaiaId& gaia_id,
    base::Time timestamp) {
  CHECK(!gaia_id.empty());
  base::DictValue dict;
  dict.Set(kExternalAppAccountDictGaiaIdKey, gaia_id.ToString());
  dict.Set(kExternalAppAccountDictTimestampKey, base::TimeToValue(timestamp));
  profile_prefs_->SetDict(prefs::kAccountPreviewExternalAppAccount,
                          std::move(dict));
}

void AccountPreviewDataServiceImpl::ClearExternalAppAccount() {
  profile_prefs_->ClearPref(prefs::kAccountPreviewExternalAppAccount);
}

void AccountPreviewDataServiceImpl::CleanUpExternalAppAccountIfExpired() {
  const base::DictValue& dict =
      profile_prefs_->GetDict(prefs::kAccountPreviewExternalAppAccount);
  if (dict.empty()) {
    return;
  }

  // Reading the pref contains the check for the expiration time. This avoid
  // creating a timer specifically for this purpose. Calling
  // `CleanUpExternalAppAccountIfExpired()` periodically/when appriorate
  // should allow to clear this pref based on expiry date accurately enough.
  if (!ReadExternalAppAccountFromPrefs().has_value()) {
    ClearExternalAppAccount();
  }
}

void AccountPreviewDataServiceImpl::CleanUpExternalAppAccountIfNotOnDevice() {
  std::optional<GaiaId> stored_gaia_id = ReadExternalAppAccountFromPrefs();
  if (!stored_gaia_id.has_value()) {
    return;
  }

  for (const CoreAccountInfo& account :
       identity_manager_->GetAccountsWithRefreshTokens()) {
    if (account.gaia == *stored_gaia_id) {
      return;
    }
  }

  ClearExternalAppAccount();
}
#endif

}  // namespace signin
