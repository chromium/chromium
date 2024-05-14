// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/turn_sync_on_helper_policy_fetch_tracker.h"

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_service.h"
#include "content/public/browser/storage_partition.h"

namespace {

constexpr base::TimeDelta kPolicyUpdateTimeout = base::Seconds(3);

class PolicyFetchTracker
    : public TurnSyncOnHelperPolicyFetchTracker,
      public policy::PolicyService::ProviderUpdateObserver {
 public:
  PolicyFetchTracker(Profile* profile, const AccountInfo& account_info)
      : profile_(profile), account_info_(account_info) {}
  ~PolicyFetchTracker() override = default;

  void SwitchToProfile(Profile* new_profile) override {
    profile_ = new_profile;
  }

  void RegisterForPolicy(base::OnceCallback<void(bool)> callback) override {
    policy::UserPolicySigninService* policy_service =
        policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
    policy_service->RegisterForPolicyWithAccountId(
        account_info_.email, account_info_.account_id,
        base::BindOnce(&PolicyFetchTracker::OnRegisteredForPolicy,
                       weak_pointer_factory_.GetWeakPtr(),
                       std::move(callback)));
  }

  bool FetchPolicy(base::OnceClosure callback) override {
    if (dm_token_.empty() || client_id_.empty()) {
      std::move(callback).Run();
      return false;
    }

    policy::UserPolicySigninService* policy_service =
        policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
    policy_service->FetchPolicyForSignedInUser(
        AccountIdFromAccountInfo(account_info_), dm_token_, client_id_,
        user_affiliation_ids_,
        profile_->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess(),
        base::BindOnce(&PolicyFetchTracker::OnPolicyFetchComplete,
                       weak_pointer_factory_.GetWeakPtr(),
                       std::move(callback)));
    return true;
  }

  // policy::PolicyService::ProviderUpdateObserver
  void OnProviderUpdatePropagated(
      policy::ConfigurationPolicyProvider* provider) override {
    if (provider != profile_->GetUserCloudPolicyManager())
      return;
    VLOG(2) << "Policies after sign in:";
    VLOG(2) << policy::PolicyConversions(
                   std::make_unique<policy::ChromePolicyConversionsClient>(
                       profile_))
                   .ToJSON();
    scoped_policy_update_observer_.Reset();
    policy_update_timeout_timer_.Reset();
    if (on_policy_updated_callback_)
      std::move(on_policy_updated_callback_).Run();
  }

  void OnProviderUpdateTimedOut() {
    DVLOG(1) << "Waiting for policies update propagated timed out";
    scoped_policy_update_observer_.Reset();
    if (on_policy_updated_callback_)
      std::move(on_policy_updated_callback_).Run();
  }

 private:
  void OnRegisteredForPolicy(
      base::OnceCallback<void(bool)> callback,
      const std::string& dm_token,
      const std::string& client_id,
      const std::vector<std::string>& user_affiliation_ids) {
    // Indicates that the account isn't managed OR there is an error during the
    // registration
    if (dm_token.empty()) {
      std::move(callback).Run(/*is_managed_account=*/false);
      return;
    }

    DVLOG(1) << "Policy registration succeeded: dm_token=" << dm_token;

    DCHECK(dm_token_.empty());
    DCHECK(client_id_.empty());
    dm_token_ = dm_token;
    client_id_ = client_id;
    user_affiliation_ids_ = user_affiliation_ids;
    std::move(callback).Run(/*is_managed_account=*/true);
  }

  void OnPolicyFetchComplete(base::OnceClosure callback, bool success) {
    DLOG_IF(ERROR, !success) << "Error fetching policy for user";
    DVLOG_IF(1, success) << "Policy fetch successful - completing signin";
    if (!success) {
      // For now, we allow signin to complete even if the policy fetch fails. If
      // we ever want to change this behavior, we could call
      // PrimaryAccountMutator::ClearPrimaryAccount() here instead.
      std::move(callback).Run();
      return;
    }

    // User cloud policies have been successfully fetched from the server. Wait
    // until these new policies are merged.
    on_policy_updated_callback_ = std::move(callback);
    scoped_policy_update_observer_.Observe(
        profile_->GetProfilePolicyConnector()->policy_service());
    policy_update_timeout_timer_.Start(
        FROM_HERE, kPolicyUpdateTimeout, this,
        &PolicyFetchTracker::OnProviderUpdateTimedOut);
  }

  raw_ptr<Profile> profile_;
  const AccountInfo account_info_;

  // Policy credentials we keep while determining whether to create
  // a new profile for an enterprise user or not.
  std::string dm_token_;
  std::string client_id_;
  std::vector<std::string> user_affiliation_ids_;

  base::OnceClosure on_policy_updated_callback_;
  base::OneShotTimer policy_update_timeout_timer_;
  base::ScopedObservation<policy::PolicyService,
                          policy::PolicyService::ProviderUpdateObserver>
      scoped_policy_update_observer_{this};

  base::WeakPtrFactory<PolicyFetchTracker> weak_pointer_factory_{this};
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class LacrosPrimaryProfilePolicyFetchTracker
    : public TurnSyncOnHelperPolicyFetchTracker {
 public:
  explicit LacrosPrimaryProfilePolicyFetchTracker(Profile* profile)
      : profile_(profile) {}
  ~LacrosPrimaryProfilePolicyFetchTracker() override = default;

  void SwitchToProfile(Profile* new_profile) override {
    // Sign in intercept and syncing with a different account are not supported
    // use cases for the Lacros primary profile.
    NOTREACHED_IN_MIGRATION();
  }

  void RegisterForPolicy(
      base::OnceCallback<void(bool is_managed)> registered_callback) override {
    // Policies for the Lacros main profile are provided by Ash on start, so
    // there is no need to register to anything to fetch them. See
    // crsrc.org/c/chromeos/crosapi/mojom/crosapi.mojom?q=device_account_policy.
    std::move(registered_callback).Run(IsManagedProfile());
  }

  bool FetchPolicy(base::OnceClosure callback) override {
    // Policies are populated via Ash at Lacros startup time, nothing to do
    // besides running the callback.
    // We post it to match the behaviour of other policy fetch trackers and
    // because `callback` can trigger the deletion of this object.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return IsManagedProfile();
  }

 private:
  bool IsManagedProfile() {
    return profile_->GetProfilePolicyConnector()->IsManaged();
  }

  raw_ptr<Profile> profile_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}  // namespace

std::unique_ptr<TurnSyncOnHelperPolicyFetchTracker>
TurnSyncOnHelperPolicyFetchTracker::CreateInstance(
    Profile* profile,
    const AccountInfo& account_info) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (profile->IsMainProfile()) {
    return std::make_unique<LacrosPrimaryProfilePolicyFetchTracker>(profile);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<PolicyFetchTracker>(profile, account_info);
}
