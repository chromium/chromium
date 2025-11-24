// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"

#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "google_apis/gaia/core_account_id.h"

HistorySyncOptinServiceDefaultDelegate::
    HistorySyncOptinServiceDefaultDelegate() = default;

HistorySyncOptinServiceDefaultDelegate::
    ~HistorySyncOptinServiceDefaultDelegate() = default;

void HistorySyncOptinServiceDefaultDelegate::ShowHistorySyncOptinScreen(
    Profile* profile,
    HistorySyncOptinHelper::FlowCompletedCallback callback) {
  CHECK(profile);
  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  if (!browser) {
    // The browser has been closed in the meantime, nothing to do.
    std::move(callback.value())
        .Run(HistorySyncOptinHelper::ScreenChoiceResult::kScreenSkipped);
    return;
  }
  browser->GetFeatures()
      .signin_view_controller()
      ->ShowModalHistorySyncOptInDialog(/*should_close_modal_dialog=*/true,
                                        std::move(callback));
}

void HistorySyncOptinServiceDefaultDelegate::ShowAccountManagementScreen(
    signin::SigninChoiceCallback on_account_management_screen_closed) {
  // Flows with access to a browser should rely on
  // `ProfileManagementDisclaimerService` for displaying management screens.
  NOTREACHED();
}

void HistorySyncOptinServiceDefaultDelegate::
    FinishFlowWithoutHistorySyncOptin() {}

HistorySyncOptinService::HistorySyncOptinService(Profile* profile)
    : profile_(profile) {
  identity_manager_scoped_observation_.Observe(
      IdentityManagerFactory::GetForProfile(profile_));
}

void HistorySyncOptinService::SetDelegateForTesting(
    std::unique_ptr<HistorySyncOptinHelper::Delegate> delegate) {
  history_sync_optin_delegate_for_testing_ = std::move(delegate);
}

HistorySyncOptinService::~HistorySyncOptinService() = default;

bool HistorySyncOptinService::StartHistorySyncOptinFlow(
    const AccountInfo& account_info,
    std::unique_ptr<HistorySyncOptinHelper::Delegate> delegate,
    signin_metrics::AccessPoint access_point) {
  bool should_start =
      Initialize(account_info, std::move(delegate), access_point);
  if (!should_start) {
    return false;
  }
  history_sync_optin_helper_->StartHistorySyncOptinFlow();
  return true;
}

bool HistorySyncOptinService::
    ResumeShowHistorySyncOptinScreenFlowForManagedUser(
        CoreAccountId account_id,
        std::unique_ptr<HistorySyncOptinHelper::Delegate> delegate,
        signin_metrics::AccessPoint access_point) {
  auto account_info = IdentityManagerFactory::GetForProfile(profile_)
                          ->FindExtendedAccountInfoByAccountId(account_id);
  CHECK(!account_info.IsEmpty());
  bool should_start =
      Initialize(account_info, std::move(delegate), access_point);
  CHECK(should_start);
  // Sanity check that this method should be invoked for managed accounts only.
  // The information about the management should already be available as during
  // the profile swap the account is moved to the new managed profile.
  CHECK(account_info.IsManaged() == signin::Tribool::kTrue);
  history_sync_optin_helper_
      ->ResumeShowHistorySyncOptinScreenFlowForManagedAccount(account_id);
  return true;
}

void HistorySyncOptinService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HistorySyncOptinService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool HistorySyncOptinService::Initialize(
    const AccountInfo& account_info,
    std::unique_ptr<HistorySyncOptinHelper::Delegate> delegate,
    signin_metrics::AccessPoint access_point) {
  if (history_sync_optin_helper_) {
    // Another flow is already in progress, abort the new flow.
    return false;
  }
  if (history_sync_optin_delegate_for_testing_) {
    history_sync_optin_delegate_ =
        std::move(history_sync_optin_delegate_for_testing_);
  } else {
    history_sync_optin_delegate_ = std::move(delegate);
  }

  CHECK(history_sync_optin_delegate_);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  history_sync_optin_helper_ = HistorySyncOptinHelper::Create(
      identity_manager, profile_, account_info,
      history_sync_optin_delegate_.get(),
      HistorySyncOptinHelper::LaunchContext::kInBrowser, access_point);
  history_sync_optin_observation_.Observe(history_sync_optin_helper_.get());
  return true;
}

void HistorySyncOptinService::Shutdown() {
  Reset();
  identity_manager_scoped_observation_.Reset();
}

void HistorySyncOptinService::Reset() {
  history_sync_optin_observation_.Reset();
  history_sync_optin_helper_.reset();
  history_sync_optin_delegate_.reset();
  for (Observer& observer : observers_) {
    observer.OnHistorySyncOptinServiceReset();
  }
}

void HistorySyncOptinService::OnHistorySyncOptinHelperFlowFinished() {
  // As the history_sync_optin_helper_ finishes the flow, it will be destroyed
  // by this call. Post a task to avoid re-entrant calls.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&HistorySyncOptinService::Reset,
                                weak_ptr_factory_.GetWeakPtr()));
}

void HistorySyncOptinService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return;
  }

  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    return;
  }

  auto access_point = event_details.GetSetPrimaryAccountAccessPoint();
  if (!access_point.has_value()) {
    return;
  }

  syncer::UserSelectableTypeSet required_types;
  int error_message_id = 0;
  // Add more access points as needed. Typically, an error should be displayed
  // if the `required_types` are needed for a specific action which the signin
  // was triggered for, but cannot be enabled due to policies in the account.
  // An example of this is the tabs from other devices page in history
  // (`kRecentTabs`), which needs `syncer::UserSelectableType::kTabs` to be
  // enabled in order to work. If the user signs in through a promo displayed in
  // that page with an account that does not allow syncing tabs, an error is
  // displayed after the management screens have been accepted.
  switch (access_point.value()) {
    case signin_metrics::AccessPoint::kRecentTabs:
      required_types = {syncer::UserSelectableType::kTabs};
      error_message_id = IDS_TABS_DISABLED_ERROR_DESCRIPTION;
      break;
    case signin_metrics::AccessPoint::kCollaborationJoinTabGroup:
    case signin_metrics::AccessPoint::kCollaborationShareTabGroup:
      required_types = {syncer::UserSelectableType::kSavedTabGroups};
      error_message_id = IDS_COLLABORATION_ENTREPRISE_TABS_SYNC_DISABLED_BODY;
      break;
    // All other access points should not trigger an error.
    case signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo:
    case signin_metrics::AccessPoint::kPasswordBubble:
    case signin_metrics::AccessPoint::kAddressBubble:
    case signin_metrics::AccessPoint::kStartPage:
    case signin_metrics::AccessPoint::kNtpLink:
    case signin_metrics::AccessPoint::kMenu:
    case signin_metrics::AccessPoint::kSettings:
    case signin_metrics::AccessPoint::kSettingsYourSavedInfo:
    case signin_metrics::AccessPoint::kSupervisedUser:
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
    case signin_metrics::AccessPoint::kExtensions:
    case signin_metrics::AccessPoint::kBookmarkBubble:
    case signin_metrics::AccessPoint::kBookmarkManager:
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
    case signin_metrics::AccessPoint::kUserManager:
    case signin_metrics::AccessPoint::kDevicesPage:
    case signin_metrics::AccessPoint::kFullscreenSigninPromo:
    case signin_metrics::AccessPoint::kUnknown:
    case signin_metrics::AccessPoint::kAutofillDropdown:
    case signin_metrics::AccessPoint::kResigninInfobar:
    case signin_metrics::AccessPoint::kTabSwitcher:
    case signin_metrics::AccessPoint::kMachineLogon:
    case signin_metrics::AccessPoint::kGoogleServicesSettings:
    case signin_metrics::AccessPoint::kSyncErrorCard:
    case signin_metrics::AccessPoint::kForcedSignin:
    case signin_metrics::AccessPoint::kAccountRenamed:
    case signin_metrics::AccessPoint::kWebSignin:
    case signin_metrics::AccessPoint::kSafetyCheck:
    case signin_metrics::AccessPoint::kKaleidoscope:
    case signin_metrics::AccessPoint::kEnterpriseSignoutCoordinator:
    case signin_metrics::AccessPoint::kSigninInterceptFirstRunExperience:
    case signin_metrics::AccessPoint::kSendTabToSelfPromo:
    case signin_metrics::AccessPoint::kNtpFeedTopPromo:
    case signin_metrics::AccessPoint::kSettingsSyncOffRow:
    case signin_metrics::AccessPoint::kPostDeviceRestoreSigninPromo:
    case signin_metrics::AccessPoint::kPostDeviceRestoreBackgroundSignin:
    case signin_metrics::AccessPoint::kNtpSignedOutIcon:
    case signin_metrics::AccessPoint::kNtpFeedCardMenuPromo:
    case signin_metrics::AccessPoint::kNtpFeedBottomPromo:
    case signin_metrics::AccessPoint::kDesktopSigninManager:
    case signin_metrics::AccessPoint::kForYouFre:
    case signin_metrics::AccessPoint::kCreatorFeedFollow:
    case signin_metrics::AccessPoint::kReadingList:
    case signin_metrics::AccessPoint::kReauthInfoBar:
    case signin_metrics::AccessPoint::kAccountConsistencyService:
    case signin_metrics::AccessPoint::kSearchCompanion:
    case signin_metrics::AccessPoint::kSetUpList:
    case signin_metrics::AccessPoint::kSaveToPhotosIos:
    case signin_metrics::AccessPoint::kChromeSigninInterceptBubble:
    case signin_metrics::AccessPoint::kRestorePrimaryAccountOnProfileLoad:
    case signin_metrics::AccessPoint::kTabOrganization:
    case signin_metrics::AccessPoint::kSaveToDriveIos:
    case signin_metrics::AccessPoint::kTipsNotification:
    case signin_metrics::AccessPoint::kNotificationsOptInScreenContentToggle:
    case signin_metrics::AccessPoint::kSigninChoiceRemembered:
    case signin_metrics::AccessPoint::kProfileMenuSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kSettingsSignoutConfirmationPrompt:
    case signin_metrics::AccessPoint::kNtpIdentityDisc:
    case signin_metrics::AccessPoint::kOidcRedirectionInterception:
    case signin_metrics::AccessPoint::kWebauthnModalDialog:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccount:
    case signin_metrics::AccessPoint::kProductSpecifications:
    case signin_metrics::AccessPoint::kAccountMenuSwitchAccountFailed:
    case signin_metrics::AccessPoint::kCctAccountMismatchNotification:
    case signin_metrics::AccessPoint::kDriveFilePickerIos:
    case signin_metrics::AccessPoint::kGlicLaunchButton:
    case signin_metrics::AccessPoint::kHistoryPage:
    case signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup:
    case signin_metrics::AccessPoint::kWidget:
    case signin_metrics::AccessPoint::kCollaborationLeaveOrDeleteTabGroup:
    case signin_metrics::AccessPoint::kHistorySyncEducationalTip:
    case signin_metrics::AccessPoint::kManagedProfileAutoSigninIos:
    case signin_metrics::AccessPoint::kNonModalSigninPasswordPromo:
    case signin_metrics::AccessPoint::kNonModalSigninBookmarkPromo:
    case signin_metrics::AccessPoint::kUserManagerWithPrefilledEmail:
    case signin_metrics::AccessPoint::kEnterpriseManagementDisclaimerAtStartup:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterBrowserFocus:
    case signin_metrics::AccessPoint::
        kEnterpriseManagementDisclaimerAfterSignin:
    case signin_metrics::AccessPoint::kNtpFeaturePromo:
    case signin_metrics::AccessPoint::kEnterpriseDialogAfterSigninInterception:
      return;
  }

  CoreAccountId primary_account_id =
      event_details.GetCurrentState().primary_account.account_id;

  auto management_accepted_callback = base::BindOnce(
      [](const syncer::UserSelectableTypeSet& required_types,
         int error_message_id, Profile* profile, bool) {
        if (!profile) {
          return;
        }

        // Don't do anything if the user is not signed in.
        if (signin_util::GetSignedInState(IdentityManagerFactory::GetForProfile(
                profile)) != signin_util::SignedInState::kSignedIn) {
          return;
        }

        syncer::SyncService* sync_service =
            SyncServiceFactory::GetForProfile(profile);
        if (!sync_service) {
          return;
        }

        // Try to turn on history sync. Disabled types are simply skipped.
        signin_util::EnableHistorySync(sync_service);

        // If the required data types cannot be enabled, show an error.
        if (!signin_util::IsSyncingUserSelectableTypesAllowedByPolicy(
                sync_service, required_types)) {
          HistorySyncOptinService* history_sync_optin_service =
              HistorySyncOptinServiceFactory::GetForProfile(profile);
          CHECK(history_sync_optin_service);
          history_sync_optin_service->ShowErrorDialogWithMessage(
              error_message_id);
        }
      },
      required_types, error_message_id);

  ProfileManagementDisclaimerService* profile_management_disclaimer_service =
      ProfileManagementDisclaimerServiceFactory::GetForProfile(profile_);
  CHECK(profile_management_disclaimer_service);

  // Make sure that there is not already another account being considered for
  // management.
  const CoreAccountId considered_managed_account_id =
      profile_management_disclaimer_service
          ->GetAccountBeingConsideredForManagementIfAny();
  if (!considered_managed_account_id.empty() &&
      considered_managed_account_id != primary_account_id) {
    return;
  }

  profile_management_disclaimer_service->EnsureManagedProfileForAccount(
      primary_account_id, access_point.value(),
      std::move(management_accepted_callback));
}

void HistorySyncOptinService::ShowErrorDialogWithMessage(int error_message_id) {
  signin_util::ShowErrorDialogWithMessage(
      chrome::FindLastActiveWithProfile(profile_), error_message_id);
}
