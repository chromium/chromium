// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "base/i18n/timezone.h"
#include "chrome/browser/ash/arc/arc_support_host.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler.h"
#include "chrome/browser/ash/login/screens/arc_terms_of_service_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/arc/arc_prefs.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

using ArcBackupAndRestoreConsent =
    sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent;
using ArcGoogleLocationServiceConsent =
    sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent;
using ArcPlayTermsOfServiceConsent =
    sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;

using sync_pb::UserConsentTypes;

namespace chromeos {

constexpr StaticOobeScreenId ArcTermsOfServiceScreenView::kScreenId;

ArcTermsOfServiceScreenHandler::ArcTermsOfServiceScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container),
      is_child_account_(
          user_manager::UserManager::Get()->IsLoggedInAsChildUser()) {
  set_user_acted_method_path("login.ArcTermsOfServiceScreen.userActed");
}

ArcTermsOfServiceScreenHandler::~ArcTermsOfServiceScreenHandler() {
  OobeUI* oobe_ui = GetOobeUI();
  if (oobe_ui)
    oobe_ui->RemoveObserver(this);
  chromeos::NetworkHandler::Get()->network_state_handler()->RemoveObserver(
      this, FROM_HERE);
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  for (auto& observer : observer_list_)
    observer.OnViewDestroyed(this);
}

void ArcTermsOfServiceScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();

  AddCallback("arcTermsOfServiceAccept",
              &ArcTermsOfServiceScreenHandler::HandleAccept);
}

void ArcTermsOfServiceScreenHandler::MaybeLoadPlayStoreToS(
    bool ignore_network_state) {
  const chromeos::NetworkState* default_network =
      chromeos::NetworkHandler::Get()
          ->network_state_handler()
          ->DefaultNetwork();
  if (!ignore_network_state && !default_network)
    return;
  const std::string country_code = base::CountryCodeForCurrentTimezone();
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("login.ArcTermsOfServiceScreen.loadPlayStoreToS", country_code);
  } else {
    LOG(ERROR) << "Silently dropping MaybeLoadPlayStoreToS request.";
  }
}

void ArcTermsOfServiceScreenHandler::OnCurrentScreenChanged(
    OobeScreenId current_screen,
    OobeScreenId new_screen) {
  if (new_screen != GaiaView::kScreenId)
    return;

  MaybeLoadPlayStoreToS(false);
  StartNetworkAndTimeZoneObserving();
}

void ArcTermsOfServiceScreenHandler::TimezoneChanged(
    const icu::TimeZone& timezone) {
  MaybeLoadPlayStoreToS(false);
}

void ArcTermsOfServiceScreenHandler::DefaultNetworkChanged(
    const NetworkState* network) {
  MaybeLoadPlayStoreToS(false);
}

void ArcTermsOfServiceScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("arcTermsOfServiceScreenHeading", IDS_ARC_OOBE_TERMS_HEADING);
  builder->Add("arcTermsOfServiceScreenDescription",
      IDS_ARC_OOBE_TERMS_DESCRIPTION);
  builder->Add("arcTermsOfServiceLoading", IDS_ARC_OOBE_TERMS_LOADING);
  builder->Add("arcTermsOfServiceErrorTitle", IDS_OOBE_GENERIC_FATAL_ERROR_TITLE);
  builder->Add("arcTermsOfServiceErrorMessage", IDS_ARC_OOBE_TERMS_LOAD_ERROR);
  builder->Add("arcTermsOfServiceRetryButton", IDS_ARC_OOBE_TERMS_BUTTON_RETRY);
  builder->Add("arcTermsOfServiceAcceptButton",
               IDS_ARC_OOBE_TERMS_BUTTON_ACCEPT);
  builder->Add("arcTermsOfServiceAcceptAndContinueButton",
               IDS_ARC_OOBE_TERMS_BUTTON_ACCEPT_AND_CONTINUE);
  builder->Add("arcTermsOfServiceNextButton",
               IDS_ARC_OPT_IN_DIALOG_BUTTON_NEXT);
  builder->Add("arcPolicyLink", IDS_ARC_OPT_IN_PRIVACY_POLICY_LINK);
  builder->Add("arcTextBackupRestore", is_child_account_
                                           ? IDS_ARC_OOBE_BACKUP_RESTORE_CHILD
                                           : IDS_ARC_OOBE_BACKUP_RESTORE);
  builder->Add("arcTextLocationService",
               is_child_account_ ? IDS_ARC_OOBE_LOCATION_SETTING_CHILD
                                 : IDS_ARC_OOBE_LOCATION_SETTING);
  builder->Add("arcTextPaiService", IDS_ARC_OOBE_PAI);
  builder->Add("arcTextGoogleServiceConfirmation",
               IDS_ARC_OPT_IN_GOOGLE_SERVICE_CONFIRMATION);
  builder->Add("arcTextReviewSettings", IDS_ARC_REVIEW_SETTINGS);
  builder->Add("arcTextMetricsEnabled",
               IDS_ARC_OOBE_TERMS_DIALOG_METRICS_ENABLED);
  builder->Add("arcTextMetricsDisabled",
               IDS_ARC_OOBE_TERMS_DIALOG_METRICS_DISABLED);
  builder->Add("arcTextMetricsManagedEnabled",
               IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_ENABLED);
  builder->Add("arcTextMetricsManagedDisabled",
               IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_DISABLED);
  builder->Add("arcTextMetricsEnabledChild",
               IDS_ARC_OOBE_TERMS_DIALOG_METRICS_ENABLED_CHILD);
  builder->Add("arcTextMetricsDisabledChild",
               IDS_ARC_OOBE_TERMS_DIALOG_METRICS_DISABLED_CHILD);
  builder->Add("arcTextMetricsManagedEnabledChild",
               IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_ENABLED_CHILD);
  builder->Add("arcTextMetricsManagedDisabledChild",
               IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_DISABLED_CHILD);
  builder->Add("arcTextMetricsDemoApps", IDS_ARC_OOBE_TERMS_DIALOG_DEMO_APPS);
  builder->Add("arcAcceptAndContinueGoogleServiceConfirmation",
               IDS_ARC_OPT_IN_ACCEPT_AND_CONTINUE_GOOGLE_SERVICE_CONFIRMATION);
  builder->Add("arcLearnMoreStatisticsTitle",
               IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_TITLE);
  builder->Add("arcLearnMoreStatisticsP1",
               is_child_account_ ? IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_CHILD_P1
                                 : IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_P1);
  builder->Add("arcLearnMoreStatisticsP2",
               is_child_account_ ? IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_CHILD_P2
                                 : IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_P2);
  builder->Add("arcLearnMoreStatisticsP3",
               is_child_account_ ? IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_CHILD_P3
                                 : IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_P3);
  builder->Add("arcLearnMoreStatisticsP4",
               is_child_account_ ? IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_CHILD_P4
                                 : IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_P4);
  builder->Add("arcLearnMoreLocationServiceTitle",
               IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES_TITLE);
  builder->Add("arcLearnMoreLocationServiceP1",
               is_child_account_
                   ? IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES_CHILD_P1
                   : IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES_P1);
  builder->Add("arcLearnMoreLocationServiceP2",
               is_child_account_
                   ? IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES_CHILD_P2
                   : IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES_P2);
  builder->Add("arcLearnMoreBackupAndRestoreTitle",
               IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_TITLE);
  builder->Add("arcLearnMoreBackupAndRestoreP1",
               IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_P1);
  builder->Add("arcLearnMoreBackupAndRestoreP2",
               IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_P2);
  builder->Add("arcLearnMoreBackupAndRestoreP3",
               IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_P3);
  builder->Add("arcLearnMoreBackupAndRestoreP4",
               IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_P4);
  builder->Add("arcLearnMoreBackupAndRestoreP5",
               IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_P5);
  builder->Add("arcLearnMoreBackupAndRestoreChildP1",
               IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_CHILD_P1);
  builder->Add("arcLearnMoreBackupAndRestoreChildP2",
               IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_CHILD_P2);
  builder->Add("arcLearnMoreBackupAndRestoreChildP3",
               IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_CHILD_P3);
  builder->Add("arcLearnMorePaiServiceTitle",
               IDS_ARC_OPT_IN_LEARN_MORE_PAI_SERVICE_TITLE);
  builder->Add("arcLearnMorePaiService", IDS_ARC_OOBE_LEARN_MORE_PAI_SERVICE);
  builder->Add("arcOverlayClose", IDS_ARC_OOBE_TERMS_POPUP_HELP_CLOSE_BUTTON);
  builder->Add("oobeModalDialogClose", IDS_CHROMEOS_OOBE_CLOSE_DIALOG);
  builder->Add("arcOverlayLoading", IDS_ARC_POPUP_HELP_LOADING);
  builder->Add("arcLearnMoreText", IDS_ARC_OPT_IN_DIALOG_LEARN_MORE_LINK_TEXT);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kArcTosHostForTests)) {
    builder->Add("arcTosHostNameForTesting",
                 base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                     switches::kArcTosHostForTests));
  }
}

void ArcTermsOfServiceScreenHandler::OnMetricsModeChanged(bool enabled,
                                                          bool managed) {
  const Profile* const profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  CHECK(user);

  const AccountId owner =
      user_manager::UserManager::Get()->GetOwnerAccountId();

  // Owner may not be set in case of initial account setup. Note, in case of
  // enterprise enrolled devices owner is always empty and we need to account
  // managed flag.
  const bool owner_profile = !owner.is_valid() || user->GetAccountId() == owner;

  std::string message;
  if (owner_profile && !managed) {
    if (is_child_account_) {
      message = enabled ? "arcTextMetricsEnabledChild"
                        : "arcTextMetricsDisabledChild";
    } else {
      message = enabled ? "arcTextMetricsEnabled" : "arcTextMetricsDisabled";
    }
  } else {
    if (is_child_account_) {
      message = enabled ? "arcTextMetricsManagedEnabledChild"
                        : "arcTextMetricsManagedDisabledChild";
    } else {
      message = enabled ? "arcTextMetricsManagedEnabled"
                        : "arcTextMetricsManagedDisabled";
    }
  }
  CallJS("login.ArcTermsOfServiceScreen.setMetricsMode", message, true);
}

void ArcTermsOfServiceScreenHandler::OnBackupAndRestoreModeChanged(
    bool enabled, bool managed) {
  backup_restore_managed_ = managed;
  CallJS("login.ArcTermsOfServiceScreen.setBackupAndRestoreMode", enabled,
         managed);
}

void ArcTermsOfServiceScreenHandler::OnLocationServicesModeChanged(
    bool enabled, bool managed) {
  location_services_managed_ = managed;
  CallJS("login.ArcTermsOfServiceScreen.setLocationServicesMode", enabled,
         managed);
}

void ArcTermsOfServiceScreenHandler::AddObserver(
    ArcTermsOfServiceScreenViewObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ArcTermsOfServiceScreenHandler::RemoveObserver(
    ArcTermsOfServiceScreenViewObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ArcTermsOfServiceScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  // Demo mode setup flow requires different variant of Play Store terms. It
  // does not allow to skip, but instead has back button. Some options are not
  // displayed, because they are not relevant for demo mode usage.
  if (arc::IsArcDemoModeSetupFlow()) {
    DoShowForDemoModeSetup();
  } else {
    DoShow();
  }
}

void ArcTermsOfServiceScreenHandler::Hide() {
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  pref_handler_.reset();
}

void ArcTermsOfServiceScreenHandler::Bind(ArcTermsOfServiceScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
}

void ArcTermsOfServiceScreenHandler::StartNetworkAndTimeZoneObserving() {
  // TODO(crbug.com/1180291) - Clean up work. Fix this logic.
  if (network_time_zone_observing_)
    return;

  chromeos::NetworkHandler::Get()->network_state_handler()->AddObserver(
      this, FROM_HERE);
  system::TimezoneSettings::GetInstance()->AddObserver(this);
  network_time_zone_observing_ = true;
}

void ArcTermsOfServiceScreenHandler::Initialize() {
  if (!show_on_init_) {
    // Send time zone information as soon as possible to able to pre-load the
    // Play Store ToS.
    GetOobeUI()->AddObserver(this);
    return;
  }

  Show();
  show_on_init_ = false;
}

void ArcTermsOfServiceScreenHandler::DoShow() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);

  CallJS("login.ArcTermsOfServiceScreen.clearDemoMode");

  // Enable ARC to match ArcSessionManager logic. ArcSessionManager expects that
  // ARC is enabled (prefs::kArcEnabled = true) on showing Terms of Service. If
  // user accepts ToS then prefs::kArcEnabled is left activated. If user skips
  // ToS then prefs::kArcEnabled is automatically reset in ArcSessionManager.
  arc::SetArcPlayStoreEnabledForProfile(profile, true);

  action_taken_ = false;

  ShowScreen(kScreenId);

  arc_managed_ = arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile);
  CallJS("login.ArcTermsOfServiceScreen.setArcManaged", arc_managed_,
         is_child_account_);

  MaybeLoadPlayStoreToS(true);
  StartNetworkAndTimeZoneObserving();

  pref_handler_ = std::make_unique<arc::ArcOptInPreferenceHandler>(
      this, profile->GetPrefs());
  pref_handler_->Start();
}

void ArcTermsOfServiceScreenHandler::DoShowForDemoModeSetup() {
  DCHECK(arc::IsArcDemoModeSetupFlow());

  CallJS("login.ArcTermsOfServiceScreen.setupForDemoMode");
  action_taken_ = false;
  ShowScreen(kScreenId);
  MaybeLoadPlayStoreToS(true);
  StartNetworkAndTimeZoneObserving();
}

bool ArcTermsOfServiceScreenHandler::NeedDispatchEventOnAction() {
  if (action_taken_)
    return false;
  action_taken_ = true;
  return true;
}

void ArcTermsOfServiceScreenHandler::RecordConsents(
    const std::string& tos_content,
    bool record_tos_consent,
    bool tos_accepted,
    bool record_backup_consent,
    bool backup_accepted,
    bool record_location_consent,
    bool location_accepted) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  consent_auditor::ConsentAuditor* consent_auditor =
      ConsentAuditorFactory::GetForProfile(profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  // The account may or may not have consented to browser sync.
  DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  const CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  ArcPlayTermsOfServiceConsent play_consent;
  play_consent.set_status(tos_accepted ? UserConsentTypes::GIVEN
                                       : UserConsentTypes::NOT_GIVEN);
  play_consent.set_confirmation_grd_id(IDS_ARC_OOBE_TERMS_BUTTON_ACCEPT);
  play_consent.set_consent_flow(ArcPlayTermsOfServiceConsent::SETUP);
  if (record_tos_consent) {
    play_consent.set_play_terms_of_service_text_length(tos_content.length());
    play_consent.set_play_terms_of_service_hash(
        base::SHA1HashString(tos_content));
  }
  consent_auditor->RecordArcPlayConsent(account_id, play_consent);

  if (record_backup_consent) {
    ArcBackupAndRestoreConsent backup_and_restore_consent;
    backup_and_restore_consent.set_confirmation_grd_id(
        IDS_ARC_OOBE_TERMS_BUTTON_ACCEPT);
    backup_and_restore_consent.add_description_grd_ids(
        is_child_account_ ? IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE_CHILD
                          : IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE);
    backup_and_restore_consent.set_status(backup_accepted
                                              ? UserConsentTypes::GIVEN
                                              : UserConsentTypes::NOT_GIVEN);

    consent_auditor->RecordArcBackupAndRestoreConsent(
        account_id, backup_and_restore_consent);
  }

  if (record_location_consent) {
    ArcGoogleLocationServiceConsent location_service_consent;
    location_service_consent.set_confirmation_grd_id(
        IDS_ARC_OOBE_TERMS_BUTTON_ACCEPT);
    location_service_consent.add_description_grd_ids(
        is_child_account_ ? IDS_ARC_OPT_IN_LOCATION_SETTING
                          : IDS_ARC_OPT_IN_LOCATION_SETTING);
    location_service_consent.set_status(location_accepted
                                            ? UserConsentTypes::GIVEN
                                            : UserConsentTypes::NOT_GIVEN);

    consent_auditor->RecordArcGoogleLocationServiceConsent(
        account_id, location_service_consent);
  }
}

void ArcTermsOfServiceScreenHandler::HandleAccept(
    bool enable_backup_restore,
    bool enable_location_services,
    bool review_arc_settings,
    const std::string& tos_content) {
  if (arc::IsArcDemoModeSetupFlow()) {
    for (auto& observer : observer_list_)
      observer.OnAccept(false);
    // TODO(agawronska): Record consent.
    return;
  }

  if (!NeedDispatchEventOnAction())
    return;

  pref_handler_->EnableBackupRestore(enable_backup_restore);
  pref_handler_->EnableLocationService(enable_location_services);

  // Record consents as accepted or not accepted as appropriate for consents
  // that are under user control when the user completes ARC setup.
  RecordConsents(tos_content, !arc_managed_, /*tos_accepted=*/true,
                 !backup_restore_managed_, enable_backup_restore,
                 !location_services_managed_, enable_location_services);

  for (auto& observer : observer_list_)
    observer.OnAccept(review_arc_settings);
}

}  // namespace chromeos
