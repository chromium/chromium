// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/browser/signin/dice_signed_in_profile_creator.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper_delegate_impl.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/browser/storage_partition.h"

namespace {

const void* const kCurrentDiceTurnSyncOnHelperKey =
    &kCurrentDiceTurnSyncOnHelperKey;

// A helper class to watch profile lifetime.
class DiceTurnSyncOnHelperShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static DiceTurnSyncOnHelperShutdownNotifierFactory* GetInstance() {
    static base::NoDestructor<DiceTurnSyncOnHelperShutdownNotifierFactory>
        factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<DiceTurnSyncOnHelperShutdownNotifierFactory>;

  DiceTurnSyncOnHelperShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "DiceTurnSyncOnHelperShutdownNotifier") {
    DependsOn(IdentityManagerFactory::GetInstance());
    DependsOn(ProfileSyncServiceFactory::GetInstance());
    DependsOn(UnifiedConsentServiceFactory::GetInstance());
    DependsOn(policy::UserPolicySigninServiceFactory::GetInstance());
  }
  ~DiceTurnSyncOnHelperShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(DiceTurnSyncOnHelperShutdownNotifierFactory);
};

AccountInfo GetAccountInfo(signin::IdentityManager* identity_manager,
                           const CoreAccountId& account_id) {
  auto maybe_account_info =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id);
  return maybe_account_info.has_value() ? maybe_account_info.value()
                                        : AccountInfo();
}


struct CurrentDiceTurnSyncOnHelperUserData
    : public base::SupportsUserData::Data {
  DiceTurnSyncOnHelper* current_helper = nullptr;
};

DiceTurnSyncOnHelper* GetCurrentDiceTurnSyncOnHelper(Profile* profile) {
  base::SupportsUserData::Data* data =
      profile->GetUserData(kCurrentDiceTurnSyncOnHelperKey);
  if (!data)
    return nullptr;
  CurrentDiceTurnSyncOnHelperUserData* wrapper =
      static_cast<CurrentDiceTurnSyncOnHelperUserData*>(data);
  DiceTurnSyncOnHelper* helper = wrapper->current_helper;
  DCHECK(helper);
  return helper;
}

void SetCurrentDiceTurnSyncOnHelper(Profile* profile,
                                    DiceTurnSyncOnHelper* helper) {
  if (!helper) {
    DCHECK(profile->GetUserData(kCurrentDiceTurnSyncOnHelperKey));
    profile->RemoveUserData(kCurrentDiceTurnSyncOnHelperKey);
    return;
  }

  DCHECK(!profile->GetUserData(kCurrentDiceTurnSyncOnHelperKey));
  std::unique_ptr<CurrentDiceTurnSyncOnHelperUserData> wrapper =
      std::make_unique<CurrentDiceTurnSyncOnHelperUserData>();
  wrapper->current_helper = helper;
  profile->SetUserData(kCurrentDiceTurnSyncOnHelperKey, std::move(wrapper));
}

}  // namespace

DiceTurnSyncOnHelper::DiceTurnSyncOnHelper(
    Profile* profile,
    signin_metrics::AccessPoint signin_access_point,
    signin_metrics::PromoAction signin_promo_action,
    signin_metrics::Reason signin_reason,
    const CoreAccountId& account_id,
    SigninAbortedMode signin_aborted_mode,
    std::unique_ptr<Delegate> delegate,
    base::OnceClosure callback)
    : delegate_(std::move(delegate)),
      profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      signin_access_point_(signin_access_point),
      signin_promo_action_(signin_promo_action),
      signin_reason_(signin_reason),
      signin_aborted_mode_(signin_aborted_mode),
      account_info_(GetAccountInfo(identity_manager_, account_id)),
      scoped_callback_runner_(std::move(callback)),
      shutdown_subscription_(
          DiceTurnSyncOnHelperShutdownNotifierFactory::GetInstance()
              ->Get(profile)
              ->Subscribe(base::Bind(&DiceTurnSyncOnHelper::AbortAndDelete,
                                     base::Unretained(this)))) {
  DCHECK(delegate_);
  DCHECK(profile_);
  // Should not start syncing if the profile is already authenticated
  DCHECK(!identity_manager_->HasPrimaryAccount());

  // Cancel any existing helper.
  AttachToProfile();

  if (account_info_.gaia.empty() || account_info_.email.empty()) {
    LOG(ERROR) << "Cannot turn Sync On for invalid account.";
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    return;
  }

  DCHECK(!account_info_.gaia.empty());
  DCHECK(!account_info_.email.empty());

  if (HasCanOfferSigninError()) {
    // Do not self-destruct synchronously in the constructor.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DiceTurnSyncOnHelper::AbortAndDelete,
                                  weak_pointer_factory_.GetWeakPtr()));
    return;
  }

  if (!IsCrossAccountError(profile_, account_info_.email, account_info_.gaia)) {
    TurnSyncOnWithProfileMode(ProfileMode::CURRENT_PROFILE);
    return;
  }

  // Handles cross account sign in error. If |account_info_| does not match the
  // last authenticated account of the current profile, then Chrome will show a
  // confirmation dialog before starting sync.
  // TODO(skym): Warn for high risk upgrade scenario (https://crbug.com/572754).
  std::string last_email =
      profile_->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);
  delegate_->ShowMergeSyncDataConfirmation(
      last_email, account_info_.email,
      base::BindOnce(&DiceTurnSyncOnHelper::OnMergeAccountConfirmation,
                     weak_pointer_factory_.GetWeakPtr()));
}

DiceTurnSyncOnHelper::DiceTurnSyncOnHelper(
    Profile* profile,
    Browser* browser,
    signin_metrics::AccessPoint signin_access_point,
    signin_metrics::PromoAction signin_promo_action,
    signin_metrics::Reason signin_reason,
    const CoreAccountId& account_id,
    SigninAbortedMode signin_aborted_mode)
    : DiceTurnSyncOnHelper(
          profile,
          signin_access_point,
          signin_promo_action,
          signin_reason,
          account_id,
          signin_aborted_mode,
          std::make_unique<DiceTurnSyncOnHelperDelegateImpl>(browser),
          base::OnceClosure()) {}

DiceTurnSyncOnHelper::~DiceTurnSyncOnHelper() {
  DCHECK_EQ(this, GetCurrentDiceTurnSyncOnHelper(profile_));
  SetCurrentDiceTurnSyncOnHelper(profile_, nullptr);
}

bool DiceTurnSyncOnHelper::HasCanOfferSigninError() {
  std::string error_msg;
  bool can_offer =
      CanOfferSignin(profile_, CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS,
                     account_info_.gaia, account_info_.email, &error_msg);
  if (can_offer)
    return false;

  // Display the error message
  delegate_->ShowLoginError(account_info_.email, error_msg);
  return true;
}

void DiceTurnSyncOnHelper::OnMergeAccountConfirmation(SigninChoice choice) {
  switch (choice) {
    case SIGNIN_CHOICE_NEW_PROFILE:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_DontImport"));
      TurnSyncOnWithProfileMode(ProfileMode::NEW_PROFILE);
      break;
    case SIGNIN_CHOICE_CONTINUE:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_ImportData"));
      TurnSyncOnWithProfileMode(ProfileMode::CURRENT_PROFILE);
      break;
    case SIGNIN_CHOICE_CANCEL:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));
      AbortAndDelete();
      break;
    case SIGNIN_CHOICE_SIZE:
      NOTREACHED();
      AbortAndDelete();
      break;
  }
}

void DiceTurnSyncOnHelper::OnEnterpriseAccountConfirmation(
    SigninChoice choice) {
  UMA_HISTOGRAM_ENUMERATION("Enterprise.UserSigninChoice", choice,
                            DiceTurnSyncOnHelper::SIGNIN_CHOICE_SIZE);
  switch (choice) {
    case SIGNIN_CHOICE_CANCEL:
      base::RecordAction(
          base::UserMetricsAction("Signin_EnterpriseAccountPrompt_Cancel"));
      AbortAndDelete();
      break;
    case SIGNIN_CHOICE_CONTINUE:
      base::RecordAction(
          base::UserMetricsAction("Signin_EnterpriseAccountPrompt_ImportData"));
      LoadPolicyWithCachedCredentials();
      break;
    case SIGNIN_CHOICE_NEW_PROFILE:
      base::RecordAction(base::UserMetricsAction(
          "Signin_EnterpriseAccountPrompt_DontImportData"));
      CreateNewSignedInProfile();
      break;
    case SIGNIN_CHOICE_SIZE:
      NOTREACHED();
      AbortAndDelete();
      break;
  }
}

void DiceTurnSyncOnHelper::TurnSyncOnWithProfileMode(ProfileMode profile_mode) {
  switch (profile_mode) {
    case ProfileMode::CURRENT_PROFILE: {
      // If this is a new signin (no account authenticated yet) try loading
      // policy for this user now, before any signed in services are
      // initialized.
      policy::UserPolicySigninService* policy_service =
          policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
      policy_service->RegisterForPolicyWithAccountId(
          account_info_.email, account_info_.account_id,
          base::BindOnce(&DiceTurnSyncOnHelper::OnRegisteredForPolicy,
                         weak_pointer_factory_.GetWeakPtr()));
      break;
    }
    case ProfileMode::NEW_PROFILE:
      // If this is a new signin (no account authenticated yet) in a new
      // profile, then just create the new signed-in profile and skip loading
      // the policy as there is no need to ask the user again if they should be
      // signed in to a new profile. Note that in this case the policy will be
      // applied after the new profile is signed in.
      CreateNewSignedInProfile();
      break;
  }
}

void DiceTurnSyncOnHelper::OnRegisteredForPolicy(const std::string& dm_token,
                                                 const std::string& client_id) {
  // If there's no token for the user (policy registration did not succeed) just
  // finish signing in.
  if (dm_token.empty()) {
    DVLOG(1) << "Policy registration failed";
    SigninAndShowSyncConfirmationUI();
    return;
  }

  DVLOG(1) << "Policy registration succeeded: dm_token=" << dm_token;

  DCHECK(dm_token_.empty());
  DCHECK(client_id_.empty());
  dm_token_ = dm_token;
  client_id_ = client_id;

  // Allow user to create a new profile before continuing with sign-in.
  delegate_->ShowEnterpriseAccountConfirmation(
      account_info_.email,
      base::BindOnce(&DiceTurnSyncOnHelper::OnEnterpriseAccountConfirmation,
                     weak_pointer_factory_.GetWeakPtr()));
}

void DiceTurnSyncOnHelper::LoadPolicyWithCachedCredentials() {
  DCHECK(!dm_token_.empty());
  DCHECK(!client_id_.empty());
  policy::UserPolicySigninService* policy_service =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
  policy_service->FetchPolicyForSignedInUser(
      AccountIdFromAccountInfo(account_info_), dm_token_, client_id_,
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess(),
      base::BindOnce(&DiceTurnSyncOnHelper::OnPolicyFetchComplete,
                     weak_pointer_factory_.GetWeakPtr()));
}

void DiceTurnSyncOnHelper::OnPolicyFetchComplete(bool success) {
  // For now, we allow signin to complete even if the policy fetch fails. If
  // we ever want to change this behavior, we could call
  // PrimaryAccountMutator::ClearPrimaryAccount() here instead.
  DLOG_IF(ERROR, !success) << "Error fetching policy for user";
  DVLOG_IF(1, success) << "Policy fetch successful - completing signin";
  if (VLOG_IS_ON(2)) {
    // User cloud policies have been fetched from the server. Dump all policy
    // values into log once these new policies are merged.
    profile_->GetProfilePolicyConnector()
        ->policy_service()
        ->AddProviderUpdateObserver(this);
  }
  SigninAndShowSyncConfirmationUI();
}

void DiceTurnSyncOnHelper::OnProviderUpdatePropagated(
    policy::ConfigurationPolicyProvider* provider) {
  if (provider != profile_->GetUserCloudPolicyManager())
    return;
  VLOG(2) << "Policies after sign in:";
  VLOG(2) << policy::DictionaryPolicyConversions(
                 std::make_unique<policy::ChromePolicyConversionsClient>(
                     profile_))
                 .ToJSON();
  profile_->GetProfilePolicyConnector()
      ->policy_service()
      ->RemoveProviderUpdateObserver(this);
}

void DiceTurnSyncOnHelper::CreateNewSignedInProfile() {
  DCHECK(!dice_signed_in_profile_creator_);
  // Unretained is fine because the profile creator is owned by this.
  dice_signed_in_profile_creator_ =
      std::make_unique<DiceSignedInProfileCreator>(
          profile_, account_info_.account_id,
          /*local_profile_name=*/base::string16(), /*icon_index=*/base::nullopt,
          base::BindOnce(&DiceTurnSyncOnHelper::OnNewSignedInProfileCreated,
                         base::Unretained(this)));
}

syncer::SyncService* DiceTurnSyncOnHelper::GetSyncService() {
  return ProfileSyncServiceFactory::IsSyncAllowed(profile_)
             ? ProfileSyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}

void DiceTurnSyncOnHelper::OnNewSignedInProfileCreated(Profile* new_profile) {
  DCHECK(dice_signed_in_profile_creator_);
  dice_signed_in_profile_creator_.reset();
  ProfileMetrics::LogProfileAddNewUser(ProfileMetrics::ADD_NEW_USER_SYNC_FLOW);

  if (!new_profile) {
    // TODO(atwilson): On error, unregister the client to release the DMToken
    // and surface a better error for the user.
    AbortAndDelete();
    return;
  }

  DCHECK_NE(profile_, new_profile);
  SwitchToProfile(new_profile);
  DCHECK_EQ(profile_, new_profile);

  if (!dm_token_.empty()) {
    // Load policy for the just-created profile - once policy has finished
    // loading the signin process will complete.
    DCHECK(!client_id_.empty());
    LoadPolicyWithCachedCredentials();
  } else {
    // No policy to load - simply complete the signin process.
    SigninAndShowSyncConfirmationUI();
  }
}

void DiceTurnSyncOnHelper::SigninAndShowSyncConfirmationUI() {
  // Signin.
  auto* primary_account_mutator = identity_manager_->GetPrimaryAccountMutator();
  primary_account_mutator->SetPrimaryAccount(account_info_.account_id);
  signin_metrics::LogSigninAccessPointCompleted(signin_access_point_,
                                                signin_promo_action_);
  signin_metrics::LogSigninReason(signin_reason_);
  base::RecordAction(base::UserMetricsAction("Signin_Signin_Succeed"));

  syncer::SyncService* sync_service = GetSyncService();
  if (sync_service) {
    // Take a SyncSetupInProgressHandle, so that the UI code can use
    // IsFirstSyncSetupInProgress() as a way to know if there is a signin in
    // progress.
    // TODO(https://crbug.com/811211): Remove this handle.
    sync_blocker_ = sync_service->GetSetupInProgressHandle();
    sync_service->GetUserSettings()->SetSyncRequested(true);
    bool is_enterprise_user =
        !policy::BrowserPolicyConnector::IsNonEnterpriseUser(
            account_info_.email);
    if (is_enterprise_user &&
        SyncStartupTracker::GetSyncServiceState(sync_service) ==
            SyncStartupTracker::SYNC_STARTUP_PENDING) {
      // For enterprise users it is important to wait until sync is initialized
      // so that the confirmation UI can be aware of startup errors. This is
      // needed to make sure that the sync confirmation dialog is shown only
      // after the sync service had a chance to check whether sync was disabled
      // by admin.
      // See http://crbug.com/812546
      sync_startup_tracker_ =
          std::make_unique<SyncStartupTracker>(sync_service, this);
      return;
    }
  }

  ShowSyncConfirmationUI();
}

void DiceTurnSyncOnHelper::SyncStartupCompleted() {
  DCHECK(sync_startup_tracker_);
  sync_startup_tracker_.reset();
  ShowSyncConfirmationUI();
}

void DiceTurnSyncOnHelper::SyncStartupFailed() {
  DCHECK(sync_startup_tracker_);
  sync_startup_tracker_.reset();
  ShowSyncConfirmationUI();
}

void DiceTurnSyncOnHelper::ShowSyncConfirmationUI() {
  delegate_->ShowSyncConfirmation(
      base::BindOnce(&DiceTurnSyncOnHelper::FinishSyncSetupAndDelete,
                     weak_pointer_factory_.GetWeakPtr()));
}

void DiceTurnSyncOnHelper::FinishSyncSetupAndDelete(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(profile_);

  switch (result) {
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      if (consent_service)
        consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
      delegate_->ShowSyncSettings();
      break;
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS: {
      syncer::SyncService* sync_service = GetSyncService();
      if (sync_service)
        sync_service->GetUserSettings()->SetFirstSetupComplete(
            syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
      if (consent_service)
        consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
      break;
    }
    case LoginUIService::ABORT_SIGNIN:
      auto* primary_account_mutator =
          identity_manager_->GetPrimaryAccountMutator();
      DCHECK(primary_account_mutator);
      primary_account_mutator->ClearPrimaryAccount(
          signin::PrimaryAccountMutator::ClearAccountsAction::kKeepAll,
          signin_metrics::ABORT_SIGNIN,
          signin_metrics::SignoutDelete::IGNORE_METRIC);
      AbortAndDelete();
      return;
  }
  delete this;
}

void DiceTurnSyncOnHelper::SwitchToProfile(Profile* new_profile) {
  DCHECK(!sync_blocker_);
  DCHECK(!sync_startup_tracker_);

  policy::UserPolicySigninServiceFactory::GetForProfile(profile_)
      ->ShutdownUserCloudPolicyManager();
  SetCurrentDiceTurnSyncOnHelper(profile_, nullptr);  // Detach from old profile
  profile_ = new_profile;
  AttachToProfile();

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
  shutdown_subscription_ =
      DiceTurnSyncOnHelperShutdownNotifierFactory::GetInstance()
          ->Get(profile_)
          ->Subscribe(base::Bind(&DiceTurnSyncOnHelper::AbortAndDelete,
                                 base::Unretained(this)));
  delegate_->SwitchToProfile(new_profile);
  // Since this is a fresh profile, it's better to remove the token if the user
  // aborts the signin.
  signin_aborted_mode_ = SigninAbortedMode::REMOVE_ACCOUNT;
}

void DiceTurnSyncOnHelper::AttachToProfile() {
  // Delete any current helper.
  DiceTurnSyncOnHelper* current_helper =
      GetCurrentDiceTurnSyncOnHelper(profile_);
  if (current_helper) {
    // If the existing flow was using the same account, keep the account.
    if (current_helper->account_info_.account_id == account_info_.account_id)
      current_helper->signin_aborted_mode_ = SigninAbortedMode::KEEP_ACCOUNT;
    current_helper->AbortAndDelete();
  }
  DCHECK(!GetCurrentDiceTurnSyncOnHelper(profile_));

  // Set this as the current helper.
  SetCurrentDiceTurnSyncOnHelper(profile_, this);
}

void DiceTurnSyncOnHelper::AbortAndDelete() {
  policy::UserPolicySigninServiceFactory::GetForProfile(profile_)
      ->ShutdownUserCloudPolicyManager();
  if (signin_aborted_mode_ == SigninAbortedMode::REMOVE_ACCOUNT) {
    // Revoke the token, and the AccountReconcilor and/or the Gaia server will
    // take care of invalidating the cookies.
    auto* accounts_mutator = identity_manager_->GetAccountsMutator();
    accounts_mutator->RemoveAccount(
        account_info_.account_id,
        signin_metrics::SourceForRefreshTokenOperation::
            kDiceTurnOnSyncHelper_Abort);
  }

  delete this;
}
