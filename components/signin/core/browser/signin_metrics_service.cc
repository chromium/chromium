// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_metrics_service.h"

#include <optional>
#include <string_view>

#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
const char kExplicitSigninMigrationHistogramName[] =
    "Signin.ExplicitSigninMigration";
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace {

const char kSigingPendingResolutionTimeBaseHistogram[] =
    "Signin.SigninPending.ResolutionTime.";

const char kSyncPausedResolutionTimeBaseHistogram[] =
    "Signin.SyncPaused.ResolutionTime.";

// Pref used to record the time from Signin Pending to the resolution of this
// state.
constexpr char kSigninPendingStartTimePref[] =
    "signin.signin_pending_start_time";

// Pref used to record the time from Sync Paused to the resolution of this
// state.
constexpr char kSyncPausedStartTimePref[] = "signin.sync_paused_start_time";

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// This pref contains the web signin start time of the accounts that have signed
// on the web only. If the account is removed or any account gets signed in to
// Chrome, the pref is cleared.
// The pref is a dictionary that maps the account ids to the web signin start
// time per account.
// Storing the account_id is not ideal as it might not be consistent with
// different platforms, however it is fine the purpose of this metric.
constexpr char kWebSigninAccountStartTimesPref[] =
    "signin.web_signin_accounts_start_time_dict";
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PendingResolutionSource {
  kReauth = 0,
  kSignout = 1,

  kMaxValue = kSignout,
};

void RecordPendingResolutionTime(const char* histogram_base_name,
                                 PendingResolutionSource resolution,
                                 base::Time start_time) {
  std::string_view resolution_string;
  switch (resolution) {
    case PendingResolutionSource::kReauth:
      resolution_string = "Reauth";
      break;
    case PendingResolutionSource::kSignout:
      resolution_string = "Signout";
      break;
  }

  std::string histogram_resolution_time_name =
      base::StrCat({histogram_base_name, resolution_string});

  base::TimeDelta time_in_pending_state = base::Time::Now() - start_time;
  base::UmaHistogramCustomTimes(histogram_resolution_time_name,
                                time_in_pending_state, base::Seconds(0),
                                base::Days(14), 50);
}

void RecordSigninPendingResolution(PendingResolutionSource resolution,
                                   base::Time signin_pending_start_time) {
  base::UmaHistogramEnumeration("Signin.SigninPending.Resolution", resolution);

  RecordPendingResolutionTime(kSigingPendingResolutionTimeBaseHistogram,
                              resolution, signin_pending_start_time);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Metric is recorded for specific access points.
void MaybeRecordWebSigninToChromeSigninTimes(
    base::Time web_signin_start_time,
    signin_metrics::AccessPoint access_point) {
  std::string_view access_point_string;
  switch (access_point) {
    case signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo:
      access_point_string = "ProfileMenu";
      break;
    case signin_metrics::AccessPoint::kPasswordBubble:
      access_point_string = "PasswordSigninPromo";
      break;
    case signin_metrics::AccessPoint::kAddressBubble:
      access_point_string = "AddressSigninPromo";
      break;
    case signin_metrics::AccessPoint::kBookmarkBubble:
      access_point_string = "BookmarkSigninPromo";
      break;
    // All other access point should not record this metric.
    case signin_metrics::AccessPoint::kStartPage:
    case signin_metrics::AccessPoint::kNtpLink:
    case signin_metrics::AccessPoint::kMenu:
    case signin_metrics::AccessPoint::kSettings:
    case signin_metrics::AccessPoint::kSettingsYourSavedInfo:
    case signin_metrics::AccessPoint::kSupervisedUser:
    case signin_metrics::AccessPoint::kExtensionInstallBubble:
    case signin_metrics::AccessPoint::kExtensions:
    case signin_metrics::AccessPoint::kBookmarkManager:
    case signin_metrics::AccessPoint::kAvatarBubbleSignIn:
    case signin_metrics::AccessPoint::kUserManager:
    case signin_metrics::AccessPoint::kDevicesPage:
    case signin_metrics::AccessPoint::kFullscreenSigninPromo:
    case signin_metrics::AccessPoint::kRecentTabs:
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
    case signin_metrics::AccessPoint::kCollaborationShareTabGroup:
    case signin_metrics::AccessPoint::kGlicLaunchButton:
    case signin_metrics::AccessPoint::kHistoryPage:
    case signin_metrics::AccessPoint::kCollaborationJoinTabGroup:
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

  std::string histogram_web_signin_to_chrome_signin_time_name = base::StrCat(
      {"Signin.WebSignin.TimeToChromeSignin.", access_point_string});
  base::TimeDelta time_in_web_signin_only_until_chrome_signin =
      base::Time::Now() - web_signin_start_time;

  base::UmaHistogramCustomTimes(histogram_web_signin_to_chrome_signin_time_name,
                                time_in_web_signin_only_until_chrome_signin,
                                base::Seconds(0), base::Days(7), 50);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// Returns whether the service was aware of any previous auth error, where a
// pref should have been recorded. Also ignore refresh token sources from
// `TokenService_LoadCredentials` because the state may not be fully initialized
// yet.
bool HasExistingAuthError(
    bool auth_error_pref_recorded,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  return auth_error_pref_recorded &&
         // When Loading credentials, it is possible that the client is
         // not yet aware of an existing error (mainly if the error was
         // generated externally; e.g a token expired and not by the
         // client itself). The error not being persisted will not show an
         // error, and therefore should not be considered as a reauth in
         // this case.
         token_operation_source !=
             signin_metrics::SourceForRefreshTokenOperation::
                 kTokenService_LoadCredentials;
}

}  // namespace

SigninMetricsService::SigninMetricsService(
    signin::IdentityManager& identity_manager,
    PrefService& pref_service,
    signin::ActivePrimaryAccountsMetricsRecorder*
        active_primary_accounts_metrics_recorder)
    : identity_manager_(identity_manager),
      pref_service_(pref_service),
      active_primary_accounts_metrics_recorder_(
          active_primary_accounts_metrics_recorder),
      management_type_recorder_(identity_manager) {
  identity_manager_scoped_observation_.Observe(&identity_manager_.get());

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  RecordExplicitSigninMigrationStatus();
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  UpdateIsManagedForAllAccounts();
}

SigninMetricsService::~SigninMetricsService() = default;

// static
void SigninMetricsService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kSigninPendingStartTimePref, base::Time());
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  registry->RegisterDictionaryPref(kWebSigninAccountStartTimesPref);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
  registry->RegisterTimePref(kSyncPausedStartTimePref, base::Time());
}

void SigninMetricsService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      std::optional<signin_metrics::AccessPoint> access_point =
          event_details.GetSetPrimaryAccountAccessPoint();
      CHECK(access_point.has_value());

      MaybeRecordMetricsForSigninPromoLimitsExperiment(
          event_details.GetCurrentState().primary_account,
          access_point.value());

      MaybeRecordWebSigninToChromeSigninMetrics(
          event_details.GetCurrentState().primary_account.account_id,
          access_point.value());

      // Clear all related web signin information on the first Chrome signin
      // event.
      pref_service_->ClearPref(kWebSigninAccountStartTimesPref);

      RecordSigninInterceptionMetrics(
          event_details.GetCurrentState().primary_account.gaia,
          access_point.value());
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

      if (active_primary_accounts_metrics_recorder_) {
        const CoreAccountInfo& account =
            event_details.GetCurrentState().primary_account;
        const AccountInfo& extended_info =
            identity_manager_->FindExtendedAccountInfo(account);

        active_primary_accounts_metrics_recorder_->MarkAccountAsActiveNow(
            account.gaia, extended_info.IsManaged());
      }

      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      if (pref_service_->HasPrefPath(kSigninPendingStartTimePref)) {
        RecordSigninPendingResolution(
            PendingResolutionSource::kSignout,
            pref_service_->GetTime(kSigninPendingStartTimePref));
        pref_service_->ClearPref(kSigninPendingStartTimePref);
      }
      break;
  }

  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSync)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      std::optional<signin_metrics::AccessPoint> access_point =
          event_details.GetSetPrimaryAccountAccessPoint();
      CHECK(access_point.has_value());
      if (access_point == signin_metrics::AccessPoint::
                              kHistorySyncOptinExpansionPillOnStartup) {
        CHECK(switches::IsAvatarSyncPromoFeatureEnabled());
        SigninPrefs signin_prefs(pref_service_.get());
        const CoreAccountInfo& account =
            event_details.GetCurrentState().primary_account;
        base::UmaHistogramExactLinear(
            "Signin.SyncOptIn.IdentityPill.SyncAtShowCount",
            signin_prefs.GetSyncPromoIdentityPillShownCount(account.gaia),
            // Arbitrary number that is higher than the possible show count that
            // the promo can reach
            // (`user_education::features::GetNewBadgeShowCount()`: 10).
            /*exclusive_max=*/30);
      }
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      if (pref_service_->HasPrefPath(kSyncPausedStartTimePref)) {
        RecordPendingResolutionTime(
            kSyncPausedResolutionTimeBaseHistogram,
            PendingResolutionSource::kSignout,
            pref_service_->GetTime(kSyncPausedStartTimePref));
        pref_service_->ClearPref(kSyncPausedStartTimePref);
      }
      break;
  }
}

void SigninMetricsService::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& core_account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  // Not recording information for Signed out users.
  if (core_account_info != identity_manager_->GetPrimaryAccountInfo(
                               signin::ConsentLevel::kSignin) ||
      // TODO(crbug.com/41434401): `core_account_info` is not supposed to be
      // empty but can potentially be. In that case we do not proceed with any
      // metric recording. More info in the linked bug.
      core_account_info.IsEmpty()) {
    return;
  }

  // Check the Sync case first, in order not to record for both Sync Paused and
  // Signin Pending.
  if (core_account_info ==
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)) {
    HandleSyncErrors(error, token_operation_source);
    return;
  }

  // Signin errors only exists with Explicit browser sign in -- SigninPending.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  HandleSigninErrors(error, token_operation_source);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  return;
}

void SigninMetricsService::Shutdown() {
  identity_manager_scoped_observation_.Reset();
  KeyedService::Shutdown();
}

void SigninMetricsService::HandleSyncErrors(
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  CHECK(identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync));

  // Detecting first error.
  if (error.IsPersistentError()) {
    if (!pref_service_->HasPrefPath(kSyncPausedStartTimePref)) {
      pref_service_->SetTime(kSyncPausedStartTimePref, base::Time::Now());
    }
    return;
  }

  // Detecting if a sync error existed and is being resolved.
  if (HasExistingAuthError(pref_service_->HasPrefPath(kSyncPausedStartTimePref),
                           token_operation_source)) {
    RecordPendingResolutionTime(
        kSyncPausedResolutionTimeBaseHistogram,
        PendingResolutionSource::kReauth,
        pref_service_->GetTime(kSyncPausedStartTimePref));
    pref_service_->ClearPref(kSyncPausedStartTimePref);
  }
}

void SigninMetricsService::HandleSigninErrors(
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  CHECK(!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync));

  // Detecting first error.
  if (error.IsPersistentError()) {
    if (!pref_service_->HasPrefPath(kSigninPendingStartTimePref)) {
      pref_service_->SetTime(kSigninPendingStartTimePref, base::Time::Now());
    }
    return;
  }

  // Detecting if a signin error existed and is being resolved.
  if (HasExistingAuthError(
          pref_service_->HasPrefPath(kSigninPendingStartTimePref),
          token_operation_source)) {
    RecordSigninPendingResolution(
        PendingResolutionSource::kReauth,
        pref_service_->GetTime(kSigninPendingStartTimePref));
    pref_service_->ClearPref(kSigninPendingStartTimePref);

    AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
        identity_manager_->GetPrimaryAccountInfo(
            signin::ConsentLevel::kSignin));
    if (account_info.access_point != signin_metrics::AccessPoint::kUnknown) {
      // Only record `Started` from WEB_SIGNIN, since there is no way to
      // know that a WebSignin resolution has started until it was
      // completed. Other access points are client access points which can
      // be tracked at the real started event.
      if (account_info.access_point ==
          signin_metrics::AccessPoint::kWebSignin) {
        base::UmaHistogramEnumeration(
            "Signin.SigninPending.ResolutionSourceStarted",
            account_info.access_point);
      }

      base::UmaHistogramEnumeration(
          "Signin.SigninPending.ResolutionSourceCompleted",
          account_info.access_point);
    }
  }
}

void SigninMetricsService::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (info.access_point == signin_metrics::AccessPoint::kWebSignin &&
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    ScopedDictPrefUpdate update(&pref_service_.get(),
                                kWebSigninAccountStartTimesPref);
    update->Set(info.account_id.ToString(),
                base::TimeToValue(base::Time::Now()));
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  if (active_primary_accounts_metrics_recorder_ &&
      info.IsManaged() != signin::Tribool::kUnknown) {
    active_primary_accounts_metrics_recorder_->MarkAccountAsManaged(
        info.gaia, signin::TriboolToBoolOrDie(info.IsManaged()));
  }
}

void SigninMetricsService::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& core_account_id) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (pref_service_->HasPrefPath(kWebSigninAccountStartTimesPref)) {
    ScopedDictPrefUpdate update(&pref_service_.get(),
                                kWebSigninAccountStartTimesPref);
    update->Remove(core_account_id.ToString());
  }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

void SigninMetricsService::OnRefreshTokensLoaded() {
  UpdateIsManagedForAllAccounts();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

void SigninMetricsService::RecordExplicitSigninMigrationStatus() {
  ExplicitSigninMigration explicit_signin_migration =
      ExplicitSigninMigration::kMigratedSignedOut;
  const bool explicit_signin_pref =
      pref_service_->GetBoolean(prefs::kExplicitBrowserSignin);
  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    explicit_signin_migration =
        explicit_signin_pref ? ExplicitSigninMigration::kMigratedSyncing
                             : ExplicitSigninMigration::kNotMigratedSyncing;
  } else if (identity_manager_->HasPrimaryAccount(
                 signin::ConsentLevel::kSignin)) {
    explicit_signin_migration =
        explicit_signin_pref ? ExplicitSigninMigration::kMigratedSignedIn
                             : ExplicitSigninMigration::kNotMigratedSignedIn;
  }

  base::UmaHistogramEnumeration(kExplicitSigninMigrationHistogramName,
                                explicit_signin_migration);
}

void SigninMetricsService::MaybeRecordMetricsForSigninPromoLimitsExperiment(
    const CoreAccountInfo& account_info,
    signin_metrics::AccessPoint access_point) {
  if (!base::FeatureList::IsEnabled(switches::kSigninPromoLimitsExperiment)) {
    return;
  }

  bool is_from_web_signin =
      GetTimeOfWebSignin(account_info.account_id).has_value();
  switch (access_point) {
    case signin_metrics::AccessPoint::kAddressBubble:
      base::UmaHistogramBoolean(
          "Signin.PromoLimitsExperiment.AddressSigninPromoShownCountAtSignin",
          is_from_web_signin
              ? SigninPrefs(pref_service_.get())
                    .GetAddressSigninPromoImpressionCount(account_info.gaia)
              : pref_service_->GetInteger(
                    prefs::
                        kAddressSignInPromoShownCountPerProfileForLimitsExperiment));
      break;
    case signin_metrics::AccessPoint::kPasswordBubble:
      base::UmaHistogramBoolean(
          "Signin.PromoLimitsExperiment.PasswordSigninPromoShownCountAtSignin",
          is_from_web_signin
              ? SigninPrefs(pref_service_.get())
                    .GetPasswordSigninPromoImpressionCount(account_info.gaia)
              : pref_service_->GetInteger(
                    prefs::
                        kPasswordSignInPromoShownCountPerProfileForLimitsExperiment));
      break;
    case signin_metrics::AccessPoint::kBookmarkBubble:
      base::UmaHistogramBoolean(
          "Signin.PromoLimitsExperiment.BookmarkSigninPromoShownCountAtSignin",
          is_from_web_signin
              ? SigninPrefs(pref_service_.get())
                    .GetBookmarkSigninPromoImpressionCount(account_info.gaia)
              : pref_service_->GetInteger(
                    prefs::
                        kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment));
      break;
    case signin_metrics::AccessPoint::kChromeSigninInterceptBubble: {
      const int uno_bubble_reprompt_count =
          SigninPrefs(pref_service_.get())
              .GetChromeSigninBubbleRepromptCount(account_info.gaia);
      if (uno_bubble_reprompt_count > 0) {
        base::UmaHistogramBoolean(
            "Signin.PromoLimitsExperiment.UnoBubbleRepromptCountAtSignin",
            uno_bubble_reprompt_count);
      }
      break;
    }
    default:
      // No other access points are relevant for this experiment.
      return;
  }
}

std::optional<base::Time> SigninMetricsService::GetTimeOfWebSignin(
    const CoreAccountId& account_id) const {
  if (!pref_service_->HasPrefPath(kWebSigninAccountStartTimesPref)) {
    return std::nullopt;
  }

  const base::Value::Dict& web_signin_account_start_time_dict =
      pref_service_->GetDict(kWebSigninAccountStartTimesPref);

  // This value only exists if the initial signin was from a web signin
  // source.
  const base::Value* start_time_value =
      web_signin_account_start_time_dict.Find(account_id.ToString());
  return start_time_value ? base::ValueToTime(start_time_value) : std::nullopt;
}

void SigninMetricsService::MaybeRecordWebSigninToChromeSigninMetrics(
    const CoreAccountId& account_id,
    signin_metrics::AccessPoint access_point) {
  std::optional<base::Time> web_signin_start_time =
      GetTimeOfWebSignin(account_id);
  if (web_signin_start_time.has_value()) {
    MaybeRecordWebSigninToChromeSigninTimes(web_signin_start_time.value(),
                                            access_point);

    base::UmaHistogramEnumeration("Signin.WebSignin.SourceToChromeSignin",
                                  access_point);
  }
}

void SigninMetricsService::RecordSigninInterceptionMetrics(
    const GaiaId& gaia_id,
    signin_metrics::AccessPoint access_point) {
  ChromeSigninUserChoice signin_choice =
      SigninPrefs(pref_service_.get())
          .GetChromeSigninInterceptionUserChoice(gaia_id);
  base::UmaHistogramEnumeration("Signin.Settings.ChromeSignin.OnSignin",
                                signin_choice);
  if (signin_choice == ChromeSigninUserChoice::kDoNotSignin) {
    base::UmaHistogramEnumeration(
        "Signin.Settings.ChromeSignin.AccessPointWithDoNotSignin",
        access_point);
  }
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

void SigninMetricsService::UpdateIsManagedForAllAccounts() {
  if (!active_primary_accounts_metrics_recorder_) {
    return;
  }
  std::vector<AccountInfo> accounts =
      identity_manager_->GetExtendedAccountInfoForAccountsWithRefreshToken();
  for (const AccountInfo& extended_info : accounts) {
    if (extended_info.IsManaged() != signin::Tribool::kUnknown) {
      active_primary_accounts_metrics_recorder_->MarkAccountAsManaged(
          extended_info.gaia,
          signin::TriboolToBoolOrDie(extended_info.IsManaged()));
    }
  }
}
