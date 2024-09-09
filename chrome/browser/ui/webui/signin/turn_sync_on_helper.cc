// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper_delegate_impl.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper_policy_fetch_tracker.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/unified_consent/unified_consent_service.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/dice_signed_in_profile_creator.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/signin/profile_picker_lacros_sign_in_provider.h"
#endif

namespace {

const void* const kCurrentTurnSyncOnHelperKey = &kCurrentTurnSyncOnHelperKey;
bool g_show_sync_enabled_ui_for_testing_ = false;

// A helper class to watch profile lifetime.
class TurnSyncOnHelperShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  TurnSyncOnHelperShutdownNotifierFactory(
      const TurnSyncOnHelperShutdownNotifierFactory&) = delete;
  TurnSyncOnHelperShutdownNotifierFactory& operator=(
      const TurnSyncOnHelperShutdownNotifierFactory&) = delete;

  static TurnSyncOnHelperShutdownNotifierFactory* GetInstance() {
    static base::NoDestructor<TurnSyncOnHelperShutdownNotifierFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<TurnSyncOnHelperShutdownNotifierFactory>;

  TurnSyncOnHelperShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "TurnSyncOnHelperShutdownNotifier") {
    DependsOn(IdentityManagerFactory::GetInstance());
    DependsOn(SyncServiceFactory::GetInstance());
    DependsOn(UnifiedConsentServiceFactory::GetInstance());
    DependsOn(policy::UserPolicySigninServiceFactory::GetInstance());
  }
  ~TurnSyncOnHelperShutdownNotifierFactory() override {}
};

// User input handler for the signin confirmation dialog.
class SigninDialogDelegate : public ui::ProfileSigninConfirmationDelegate {
 public:
  explicit SigninDialogDelegate(signin::SigninChoiceCallback callback)
      : callback_(std::move(callback)) {
    DCHECK(callback_);
  }
  SigninDialogDelegate(const SigninDialogDelegate&) = delete;
  SigninDialogDelegate& operator=(const SigninDialogDelegate&) = delete;
  ~SigninDialogDelegate() override = default;

  void OnCancelSignin() override {
    DCHECK(callback_);
    std::move(callback_).Run(signin::SIGNIN_CHOICE_CANCEL);
  }

  void OnContinueSignin() override {
    DCHECK(callback_);
    std::move(callback_).Run(signin::SIGNIN_CHOICE_CONTINUE);
  }

  void OnSigninWithNewProfile() override {
    DCHECK(callback_);
    std::move(callback_).Run(signin::SIGNIN_CHOICE_NEW_PROFILE);
  }

 private:
  signin::SigninChoiceCallback callback_;
};

struct CurrentTurnSyncOnHelperUserData : public base::SupportsUserData::Data {
  raw_ptr<TurnSyncOnHelper> current_helper = nullptr;
};

TurnSyncOnHelper* GetCurrentTurnSyncOnHelper(Profile* profile) {
  base::SupportsUserData::Data* data =
      profile->GetUserData(kCurrentTurnSyncOnHelperKey);
  if (!data) {
    return nullptr;
  }
  CurrentTurnSyncOnHelperUserData* wrapper =
      static_cast<CurrentTurnSyncOnHelperUserData*>(data);
  TurnSyncOnHelper* helper = wrapper->current_helper;
  DCHECK(helper);
  return helper;
}

void SetCurrentTurnSyncOnHelper(Profile* profile, TurnSyncOnHelper* helper) {
  if (!helper) {
    DCHECK(profile->GetUserData(kCurrentTurnSyncOnHelperKey));
    profile->RemoveUserData(kCurrentTurnSyncOnHelperKey);
    return;
  }

  DCHECK(!profile->GetUserData(kCurrentTurnSyncOnHelperKey));
  std::unique_ptr<CurrentTurnSyncOnHelperUserData> wrapper =
      std::make_unique<CurrentTurnSyncOnHelperUserData>();
  wrapper->current_helper = helper;
  profile->SetUserData(kCurrentTurnSyncOnHelperKey, std::move(wrapper));
}

}  // namespace

bool TurnSyncOnHelper::Delegate::
    ShouldAbortBeforeShowSyncDisabledConfirmation() {
  return false;
}

// static
void TurnSyncOnHelper::Delegate::ShowLoginErrorForBrowser(
    const SigninUIError& error,
    Browser* browser) {
  if (!browser) {
    // TODO(crbug.com/40242414): Make sure we do something or log an error if
    // opening a browser window was not possible.
    return;
  }
  LoginUIServiceFactory::GetForProfile(browser->profile())
      ->DisplayLoginResult(browser, error, /*from_profile_picker=*/false);
}

TurnSyncOnHelper::TurnSyncOnHelper(
    Profile* profile,
    signin_metrics::AccessPoint signin_access_point,
    signin_metrics::PromoAction signin_promo_action,
    const CoreAccountId& account_id,
    SigninAbortedMode signin_aborted_mode,
    std::unique_ptr<Delegate> delegate,
    base::OnceClosure callback)
    : delegate_(std::move(delegate)),
      profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      signin_access_point_(signin_access_point),
      signin_promo_action_(signin_promo_action),
      signin_aborted_mode_(signin_aborted_mode),
      account_info_(
          identity_manager_->FindExtendedAccountInfoByAccountId(account_id)),
      scoped_callback_runner_(std::move(callback)),
      initial_primary_account_(identity_manager_->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin)),
      shutdown_subscription_(
          TurnSyncOnHelperShutdownNotifierFactory::GetInstance()
              ->Get(profile)
              ->Subscribe(base::BindOnce(&TurnSyncOnHelper::AbortAndDelete,
                                         base::Unretained(this)))) {
  DCHECK(delegate_);
  DCHECK(profile_);
  // Should not start syncing if the profile is already authenticated
  DCHECK(!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync));

  // Cancel any existing helper.
  AttachToProfile();

  // Trigger the start of the flow via a posted task. Starting the flow could
  // result in the deletion of this object and the deletion of the host, which
  // should not be done synchronously. See crbug.com/1367078 for example.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TurnSyncOnHelper::TurnSyncOnInternal,
                                weak_pointer_factory_.GetWeakPtr()));
}

TurnSyncOnHelper::TurnSyncOnHelper(
    Profile* profile,
    Browser* browser,
    signin_metrics::AccessPoint signin_access_point,
    signin_metrics::PromoAction signin_promo_action,
    const CoreAccountId& account_id,
    SigninAbortedMode signin_aborted_mode,
    bool is_sync_promo)
    : TurnSyncOnHelper(
          profile,
          signin_access_point,
          signin_promo_action,
          account_id,
          signin_aborted_mode,
          std::make_unique<TurnSyncOnHelperDelegateImpl>(browser,
                                                         is_sync_promo),
          base::OnceClosure()) {
  // If this is a promo, the account should not be removed on abort.
  CHECK(!is_sync_promo ||
        signin_aborted_mode == SigninAbortedMode::KEEP_ACCOUNT);
}

TurnSyncOnHelper::~TurnSyncOnHelper() {
  DCHECK_EQ(this, GetCurrentTurnSyncOnHelper(profile_));
  SetCurrentTurnSyncOnHelper(profile_, nullptr);
}

void TurnSyncOnHelper::TurnSyncOnInternal() {
  if (account_info_.gaia.empty() || account_info_.email.empty()) {
    LOG(ERROR) << "Cannot turn Sync On for invalid account.";
    delete this;
    return;
  }

  DCHECK(!account_info_.gaia.empty());
  DCHECK(!account_info_.email.empty());

  DCHECK(!user_input_complete_timer_);
  user_input_complete_timer_ = base::ElapsedTimer();

  if (HasCanOfferSigninError()) {
    AbortAndDelete();
    return;
  }

  if (!IsCrossAccountError(profile_, account_info_.gaia)) {
    TurnSyncOnWithProfileMode(ProfileMode::CURRENT_PROFILE);
    return;
  }

  // Handles cross account sign in error. If |account_info_| does not match the
  // last authenticated account of the current profile, then Chrome will show a
  // confirmation dialog before starting sync.
  // TODO(skym): Warn for high risk upgrade scenario (https://crbug.com/572754).
  std::string last_email = profile_->GetPrefs()->GetString(
      prefs::kGoogleServicesLastSyncingUsername);
  delegate_->ShowMergeSyncDataConfirmation(
      last_email, account_info_.email,
      base::BindOnce(&TurnSyncOnHelper::OnMergeAccountConfirmation,
                     weak_pointer_factory_.GetWeakPtr()));
}

bool TurnSyncOnHelper::HasCanOfferSigninError() {
  SigninUIError can_offer_error =
      CanOfferSignin(profile_, account_info_.gaia, account_info_.email);
  if (can_offer_error.IsOk()) {
    return false;
  }

  // Display the error message
  delegate_->ShowLoginError(can_offer_error);
  return true;
}

void TurnSyncOnHelper::OnMergeAccountConfirmation(signin::SigninChoice choice) {
  user_input_complete_timer_ = base::ElapsedTimer();

  switch (choice) {
    case signin::SIGNIN_CHOICE_NEW_PROFILE:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_DontImport"));
      TurnSyncOnWithProfileMode(ProfileMode::NEW_PROFILE);
      break;
    case signin::SIGNIN_CHOICE_CONTINUE:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_ImportData"));
      TurnSyncOnWithProfileMode(ProfileMode::CURRENT_PROFILE);
      break;
    case signin::SIGNIN_CHOICE_CANCEL:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));
      AbortAndDelete();
      break;
    case signin::SIGNIN_CHOICE_SIZE:
      NOTREACHED_IN_MIGRATION();
      AbortAndDelete();
      break;
  }
}

void TurnSyncOnHelper::OnEnterpriseAccountConfirmation(
    signin::SigninChoice choice) {
  user_input_complete_timer_ = base::ElapsedTimer();

  enterprise_account_confirmed_ = choice == signin::SIGNIN_CHOICE_CONTINUE ||
                                  choice == signin::SIGNIN_CHOICE_NEW_PROFILE;
  signin_util::RecordEnterpriseProfileCreationUserChoice(
      profile_, enterprise_account_confirmed_);

  switch (choice) {
    case signin::SIGNIN_CHOICE_CANCEL:
      base::RecordAction(
          base::UserMetricsAction("Signin_EnterpriseAccountPrompt_Cancel"));
      AbortAndDelete();
      break;
    case signin::SIGNIN_CHOICE_CONTINUE:
      base::RecordAction(
          base::UserMetricsAction("Signin_EnterpriseAccountPrompt_ImportData"));
      LoadPolicyWithCachedCredentials();
      break;
    case signin::SIGNIN_CHOICE_NEW_PROFILE:
      base::RecordAction(base::UserMetricsAction(
          "Signin_EnterpriseAccountPrompt_DontImportData"));
      CreateNewSignedInProfile();
      break;
    case signin::SIGNIN_CHOICE_SIZE:
      NOTREACHED_IN_MIGRATION();
      AbortAndDelete();
      break;
  }
}

void TurnSyncOnHelper::TurnSyncOnWithProfileMode(ProfileMode profile_mode) {
  switch (profile_mode) {
    case ProfileMode::CURRENT_PROFILE: {
      // If this is a new signin (no account authenticated yet) try loading
      // policy for this user now, before any signed in services are
      // initialized.
      policy_fetch_tracker_ =
          TurnSyncOnHelperPolicyFetchTracker::CreateInstance(profile_,
                                                             account_info_);
      policy_fetch_tracker_->RegisterForPolicy(base::BindOnce(
          &TurnSyncOnHelper::OnRegisteredForPolicy, base::Unretained(this)));
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

void TurnSyncOnHelper::OnRegisteredForPolicy(bool is_account_managed) {
  if (!is_account_managed) {
    // Just finish signing in.
    DVLOG(1) << "Policy registration failed";
    SigninAndShowSyncConfirmationUI();
    return;
  }

  if (!enterprise_util::UserAcceptedAccountManagement(profile_)) {
    // Allow user to create a new profile before continuing with sign-in.
    delegate_->ShowEnterpriseAccountConfirmation(
        account_info_,
        base::BindOnce(&TurnSyncOnHelper::OnEnterpriseAccountConfirmation,
                       weak_pointer_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(enterprise_util::UserAcceptedAccountManagement(profile_));
  LoadPolicyWithCachedCredentials();
}

void TurnSyncOnHelper::LoadPolicyWithCachedCredentials() {
  DCHECK(policy_fetch_tracker_);
  bool fetch_started = policy_fetch_tracker_->FetchPolicy(
      base::BindOnce(&TurnSyncOnHelper::SigninAndShowSyncConfirmationUI,
                     base::Unretained(this)));
  DCHECK(fetch_started);
}

void TurnSyncOnHelper::CreateNewSignedInProfile() {
  // Use the same the default search engine in the new profile.
  search_engines::ChoiceData search_engine_choice_data =
      SearchEngineChoiceDialogService::GetChoiceDataFromProfile(*profile_);

  base::OnceCallback<void(Profile*)> profile_created_callback = base::BindOnce(
      &TurnSyncOnHelper::OnNewSignedInProfileCreated, base::Unretained(this),
      std::move(search_engine_choice_data));

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  DCHECK(!dice_signed_in_profile_creator_);
  // Unretained is fine because the profile creator is owned by this.
  dice_signed_in_profile_creator_ =
      std::make_unique<DiceSignedInProfileCreator>(
          profile_, account_info_.account_id,
          /*local_profile_name=*/std::u16string(),
          /*icon_index=*/std::nullopt, std::move(profile_created_callback));
#else
  DCHECK(!profile_->IsMainProfile());
  lacros_sign_in_provider_ =
      std::make_unique<ProfilePickerLacrosSignInProvider>(
          /*hidden_profile=*/false);
  lacros_sign_in_provider_->CreateSignedInProfileWithExistingAccount(
      account_info_.gaia, std::move(profile_created_callback));
#endif
}

syncer::SyncService* TurnSyncOnHelper::GetSyncService() {
  return SyncServiceFactory::IsSyncAllowed(profile_)
             ? SyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}

void TurnSyncOnHelper::OnNewSignedInProfileCreated(
    search_engines::ChoiceData search_engine_choice_data,
    Profile* new_profile) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  DCHECK(dice_signed_in_profile_creator_);
  dice_signed_in_profile_creator_.reset();
#else
  DCHECK(lacros_sign_in_provider_);
  DCHECK(!profile_->IsMainProfile());
  lacros_sign_in_provider_.reset();
  if (new_profile) {
    // The `dice_signed_in_profile_creator_` removes the account from the source
    // profile as part of its run, but `lacros_sign_in_provider_` does not.
    // Remove the account now.
    g_browser_process->profile_manager()
        ->GetAccountProfileMapper()
        ->RemoveAccount(
            profile_->GetPath(),
            {account_info_.gaia, account_manager::AccountType::kGaia});
  }
#endif

  ProfileMetrics::LogProfileAddNewUser(ProfileMetrics::ADD_NEW_USER_SYNC_FLOW);
  if (!new_profile) {
    LOG(WARNING) << "Failed switching the Sync opt-in flow to a new profile.";
    // TODO(atwilson): On error, unregister the client to release the DMToken
    // and surface a better error for the user.
    AbortAndDelete();
    return;
  }

  DCHECK_NE(profile_, new_profile);
  SwitchToProfile(new_profile);
  DCHECK_EQ(profile_, new_profile);

  // The new profile inherits the default search provider and the search
  // engine choice timestamp from the previous profile.
  SearchEngineChoiceDialogService::UpdateProfileFromChoiceData(
      *new_profile, search_engine_choice_data);

  if (policy_fetch_tracker_) {
    // Load policy for the just-created profile - once policy has finished
    // loading the signin process will complete.
    // Note: the fetch might not happen if the account is not managed.
    policy_fetch_tracker_->FetchPolicy(
        base::BindOnce(&TurnSyncOnHelper::SigninAndShowSyncConfirmationUI,
                       base::Unretained(this)));
  } else {
    // No policy to load - simply complete the signin process.
    SigninAndShowSyncConfirmationUI();
  }
}

void TurnSyncOnHelper::SigninAndShowSyncConfirmationUI() {
  auto* primary_account_mutator = identity_manager_->GetPrimaryAccountMutator();

  // Signin.
  if (auto* signin_manager = SigninManagerFactory::GetForProfile(profile_)) {
    // `signin_manager` is null in tests.
    account_change_blocker_ =
        signin_manager->CreateAccountSelectionInProgressHandle();
  }
  primary_account_mutator->SetPrimaryAccount(account_info_.account_id,
                                             signin::ConsentLevel::kSignin,
                                             signin_access_point_);
  // If the account is already signed in, `SetPrimaryAccount()` above is a no-op
  // and the logs below are inaccurate.
  signin_metrics::LogSigninAccessPointCompleted(signin_access_point_,
                                                signin_promo_action_);
  base::RecordAction(base::UserMetricsAction("Signin_Signin_Succeed"));

  bool user_accepted_management =
      enterprise_util::UserAcceptedAccountManagement(profile_);
  if (!user_accepted_management) {
    enterprise_util::SetUserAcceptedAccountManagement(
        profile_, enterprise_account_confirmed_);
    user_accepted_management = enterprise_account_confirmed_;
  }
  if (user_accepted_management) {
    signin_aborted_mode_ = SigninAbortedMode::KEEP_ACCOUNT;
  }

  syncer::SyncService* sync_service = GetSyncService();
  if (sync_service) {
    // Take a SyncSetupInProgressHandle, so that the UI code can use
    // IsFirstSyncSetupInProgress() as a way to know if there is a signin in
    // progress.
    // TODO(crbug.com/41369996): Remove this handle.
    sync_blocker_ = sync_service->GetSetupInProgressHandle();
    sync_service->SetSyncFeatureRequested();

    // For managed users and users on enterprise machines that might have cloud
    // policies, it is important to wait until sync is initialized so that the
    // confirmation UI can be aware of startup errors. Since all users can be
    // subjected to cloud policies through device or browser management (CBCM),
    // this is needed to make sure that all cloud policies are loaded before any
    // dialog is shown to check whether sync was disabled by admin. Only wait
    // for cloud policies because local policies are instantly available. See
    // http://crbug.com/812546
    bool may_have_cloud_policies =
        signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
            account_info_.email) ||
        policy::ManagementServiceFactory::GetForProfile(profile_)
            ->HasManagementAuthority(
                policy::EnterpriseManagementAuthority::CLOUD) ||
        policy::ManagementServiceFactory::GetForProfile(profile_)
            ->HasManagementAuthority(
                policy::EnterpriseManagementAuthority::CLOUD_DOMAIN) ||
        policy::ManagementServiceFactory::GetForPlatform()
            ->HasManagementAuthority(
                policy::EnterpriseManagementAuthority::CLOUD) ||
        policy::ManagementServiceFactory::GetForPlatform()
            ->HasManagementAuthority(
                policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

    if (may_have_cloud_policies &&
        SyncStartupTracker::GetServiceStartupState(sync_service) ==
            SyncStartupTracker::ServiceStartupState::kPending) {
      sync_startup_tracker_ = std::make_unique<SyncStartupTracker>(
          sync_service,
          base::BindOnce(&TurnSyncOnHelper::OnSyncStartupStateChanged,
                         weak_pointer_factory_.GetWeakPtr()));
      return;
    }
  }
  ShowSyncConfirmationUI();
}

void TurnSyncOnHelper::OnSyncStartupStateChanged(
    SyncStartupTracker::ServiceStartupState state) {
  switch (state) {
    case SyncStartupTracker::ServiceStartupState::kPending:
      NOTREACHED_IN_MIGRATION();
      break;
    case SyncStartupTracker::ServiceStartupState::kTimeout:
      DVLOG(1) << "Waiting for Sync Service to start timed out.";
      [[fallthrough]];
    case SyncStartupTracker::ServiceStartupState::kError:
    case SyncStartupTracker::ServiceStartupState::kComplete:
      DCHECK(sync_startup_tracker_);
      sync_startup_tracker_.reset();
      ShowSyncConfirmationUI();
      break;
  }
}

// static
void TurnSyncOnHelper::SetShowSyncEnabledUiForTesting(
    bool show_sync_enabled_ui_for_testing) {
  g_show_sync_enabled_ui_for_testing_ = show_sync_enabled_ui_for_testing;
}

// static
bool TurnSyncOnHelper::HasCurrentTurnSyncOnHelperForTesting(Profile* profile) {
  return !!GetCurrentTurnSyncOnHelper(profile);
}

void TurnSyncOnHelper::ShowSyncConfirmationUI() {
  // We have now gathered all the required async information to show either the
  // sync confirmation UI, or another screen.
  DCHECK(user_input_complete_timer_);
  base::UmaHistogramMediumTimes("Signin.SyncOptIn.PreSyncConfirmationLatency",
                                user_input_complete_timer_->Elapsed());

  if (g_show_sync_enabled_ui_for_testing_ || GetSyncService()) {
    signin_metrics::LogSyncOptInStarted(signin_access_point_);
    delegate_->ShowSyncConfirmation(
        base::BindOnce(&TurnSyncOnHelper::FinishSyncSetupAndDelete,
                       weak_pointer_factory_.GetWeakPtr()));
    return;
  }

  // Sync is disabled. Check if we need to display the disabled confirmation UI
  // first.
  if (delegate_->ShouldAbortBeforeShowSyncDisabledConfirmation()) {
    FinishSyncSetupAndDelete(
        LoginUIService::SyncConfirmationUIClosedResult::ABORT_SYNC);
    return;
  }

  // TODO(crbug.com/40249681): Once we stop completing the Sync opt-in when it's
  // disabled, we also should stop recording opt-in start events.
  signin_metrics::LogSyncOptInStarted(signin_access_point_);

  // The sync disabled dialog has an explicit "sign-out" label for the
  // LoginUIService::ABORT_SYNC action, force the mode to remove the account.
  if (!enterprise_util::UserAcceptedAccountManagement(profile_) ||
      !base::FeatureList::IsEnabled(kDisallowManagedProfileSignout)) {
    signin_aborted_mode_ = SigninAbortedMode::REMOVE_ACCOUNT;
  }
  // Use the email-based heuristic if `account_info_` isn't fully initialized.
  const bool is_managed_account =
      account_info_.IsValid()
          ? account_info_.IsManaged()
          : signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
                account_info_.email);
  delegate_->ShowSyncDisabledConfirmation(
      is_managed_account,
      base::BindOnce(&TurnSyncOnHelper::FinishSyncSetupAndDelete,
                     weak_pointer_factory_.GetWeakPtr()));
}

void TurnSyncOnHelper::FinishSyncSetupAndDelete(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(profile_);
  auto* primary_account_mutator = identity_manager_->GetPrimaryAccountMutator();
  DCHECK(primary_account_mutator);

  switch (result) {
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      primary_account_mutator->SetPrimaryAccount(account_info_.account_id,
                                                 signin::ConsentLevel::kSync,
                                                 signin_access_point_);
      if (consent_service) {
        consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
      }
      signin_metrics::LogSyncSettingsOpened(signin_access_point_);
      delegate_->ShowSyncSettings();
      break;
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS:
      primary_account_mutator->SetPrimaryAccount(account_info_.account_id,
                                                 signin::ConsentLevel::kSync,
                                                 signin_access_point_);
      if (auto* sync_service = GetSyncService()) {
        sync_service->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
            syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
      }
      if (consent_service) {
        consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
      }
      break;
    case LoginUIService::ABORT_SYNC:
      AbortAndDelete();
      return;

    case LoginUIService::UI_CLOSED:
      // When force sign in is enabled and the user did not accept enterprise
      // management, or did not enable sync; make sure to clear the primary
      // account. This is mainly useful not to remember information on the
      // Default Profile that already exists (when creating a new profile the
      // flow will simply stop).
      if (signin_util::IsForceSigninEnabled() &&
          !enterprise_util::UserAcceptedAccountManagement(profile_)) {
        primary_account_mutator->ClearPrimaryAccount(
            signin_metrics::ProfileSignout::kAbortSignin);
      }

      // No explicit action when the ui gets closed. No final callback is sent.
      scoped_callback_runner_.ReplaceClosure(base::OnceClosure());
      break;
  }
  delete this;
}

void TurnSyncOnHelper::SwitchToProfile(Profile* new_profile) {
  // The sync setup process shouldn't have been started if the user still had
  // the option to switch profiles, or it should have been properly cleaned up.
  DCHECK(!account_change_blocker_);
  DCHECK(!sync_blocker_);
  DCHECK(!sync_startup_tracker_);

  policy::UserPolicySigninServiceFactory::GetForProfile(profile_)
      ->ShutdownCloudPolicyManager();
  SetCurrentTurnSyncOnHelper(profile_, nullptr);  // Detach from old profile
  profile_ = new_profile;
  initial_primary_account_ = CoreAccountId();
  AttachToProfile();

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
  shutdown_subscription_ =
      TurnSyncOnHelperShutdownNotifierFactory::GetInstance()
          ->Get(profile_)
          ->Subscribe(base::BindOnce(&TurnSyncOnHelper::AbortAndDelete,
                                     base::Unretained(this)));
  delegate_->SwitchToProfile(new_profile);
  if (policy_fetch_tracker_) {
    policy_fetch_tracker_->SwitchToProfile(profile_);
  }
}

void TurnSyncOnHelper::AttachToProfile() {
  // Delete any current helper.
  TurnSyncOnHelper* current_helper = GetCurrentTurnSyncOnHelper(profile_);
  if (current_helper) {
    // If the existing flow was using the same account, keep the account.
    if (current_helper->account_info_.account_id == account_info_.account_id) {
      current_helper->signin_aborted_mode_ = SigninAbortedMode::KEEP_ACCOUNT;
    }
    policy::UserPolicySigninServiceFactory::GetForProfile(profile_)
        ->ShutdownCloudPolicyManager();
    current_helper->AbortAndDelete();
  }
  DCHECK(!GetCurrentTurnSyncOnHelper(profile_));

  // Set this as the current helper.
  SetCurrentTurnSyncOnHelper(profile_, this);
}

void TurnSyncOnHelper::AbortAndDelete() {
  // If the initial primary account is still valid, reset it.
  // Otherwise, `RemoveAccount()` will assume the primary account is being
  // removed and will call `ClearPrimaryAccount()` that will signout the profile
  // completely.
  if (!initial_primary_account_.empty() &&
      identity_manager_->HasAccountWithRefreshToken(initial_primary_account_)) {
    identity_manager_->GetPrimaryAccountMutator()->SetPrimaryAccount(
        initial_primary_account_, signin::ConsentLevel::kSignin);
  }

  switch (signin_aborted_mode_) {
    case SigninAbortedMode::REMOVE_ACCOUNT:
    case SigninAbortedMode::KEEP_ACCOUNT_ON_WEB_ONLY:
      RemoveAccount();
      break;

    case SigninAbortedMode::KEEP_ACCOUNT:
      // Do nothing.
      break;
  }

  delete this;
}

void TurnSyncOnHelper::RemoveAccount() {
  CHECK(signin_aborted_mode_ == SigninAbortedMode::REMOVE_ACCOUNT ||
        signin_aborted_mode_ == SigninAbortedMode::KEEP_ACCOUNT_ON_WEB_ONLY);
  bool is_primary_account =
      account_info_.account_id ==
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .account_id;
  if (is_primary_account) {
    policy::UserPolicySigninServiceFactory::GetForProfile(profile_)
        ->ShutdownCloudPolicyManager();
    auto* primary_account_mutator =
        identity_manager_->GetPrimaryAccountMutator();
    if (signin_aborted_mode_ == SigninAbortedMode::KEEP_ACCOUNT_ON_WEB_ONLY) {
      primary_account_mutator->RemovePrimaryAccountButKeepTokens(
          signin_metrics::ProfileSignout::
              kCancelSyncConfirmationOnWebOnlySignedIn);
    } else {
      primary_account_mutator->ClearPrimaryAccount(
          signin_metrics::ProfileSignout::kCancelSyncConfirmationRemoveAccount);
    }
    return;
  }

  if (signin_aborted_mode_ == SigninAbortedMode::KEEP_ACCOUNT_ON_WEB_ONLY) {
    return;
  }
  // Revoke the token, and the `AccountReconcilor` and/or the Gaia server
  // will take care of invalidating the cookies.
  auto* accounts_mutator = identity_manager_->GetAccountsMutator();
  accounts_mutator->RemoveAccount(
      account_info_.account_id,
      signin_metrics::SourceForRefreshTokenOperation::kTurnOnSyncHelper_Abort);
}

// static
void TurnSyncOnHelper::EnsureFactoryBuilt() {
  TurnSyncOnHelperShutdownNotifierFactory::GetInstance();
}
