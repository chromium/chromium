// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"

#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "base/i18n/timezone.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/optin/arc_optin_preference_handler.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/arc/arc_prefs.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
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

  AddCallback("arcTermsOfServiceSkip",
              &ArcTermsOfServiceScreenHandler::HandleSkip);
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
  CallJS("login.ArcTermsOfServiceScreen.loadPlayStoreToS", country_code);
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
  builder->Add("arcTermsOfServiceError", IDS_ARC_OOBE_TERMS_LOAD_ERROR);
  builder->Add("arcTermsOfServiceSkipButton", IDS_ARC_OOBE_TERMS_BUTTON_SKIP);
  builder->Add("arcTermsOfServiceRetryButton", IDS_ARC_OOBE_TERMS_BUTTON_RETRY);
  builder->Add("arcTermsOfServiceAcceptButton",
               IDS_ARC_OOBE_TERMS_BUTTON_ACCEPT);
  builder->Add("arcTermsOfServiceAcceptAndContinueButton",
               IDS_ARC_OOBE_TERMS_BUTTON_ACCEPT_AND_CONTINUE);
  builder->Add("arcTermsOfServiceNextButton",
               IDS_ARC_OPT_IN_DIALOG_BUTTON_NEXT);
  builder->Add("arcPolicyLink", IDS_ARC_OPT_IN_PRIVACY_POLICY_LINK);
  builder->Add("arcTextBackupRestore",
               is_child_account_ ? IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE_CHILD
                                 : IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE);
  builder->Add("arcTextLocationService",
               is_child_account_ ? IDS_ARC_OPT_IN_LOCATION_SETTING_CHILD
                                 : IDS_ARC_OPT_IN_LOCATION_SETTING);
  builder->Add("arcTextPaiService", IDS_ARC_OPT_IN_PAI);
  builder->Add("arcTextGoogleServiceConfirmation",
               IDS_ARC_OPT_IN_GOOGLE_SERVICE_CONFIRMATION);
  builder->Add("arcTextReviewSettings", IDS_ARC_REVIEW_SETTINGS);
  builder->Add("arcTextMetricsManagedEnabled",
               IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_ENABLED);
  builder->Add("arcTextMetricsDemoApps", IDS_ARC_OOBE_TERMS_DIALOG_DEMO_APPS);
  builder->Add("arcAcceptAndContinueGoogleServiceConfirmation",
               IDS_ARC_OPT_IN_ACCEPT_AND_CONTINUE_GOOGLE_SERVICE_CONFIRMATION);
  builder->Add("arcLearnMoreStatistics",
               is_child_account_ ? IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS_CHILD
                                 : IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS);
  builder->Add("arcLearnMoreLocationService",
               is_child_account_
                   ? IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES_CHILD
                   : IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES);
  builder->Add("arcLearnMoreBackupAndRestore",
               is_child_account_
                   ? IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE_CHILD
                   : IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE);
  builder->Add("arcLearnMorePaiService", IDS_ARC_OPT_IN_LEARN_MORE_PAI_SERVICE);
  builder->Add("arcOverlayClose", IDS_ARC_OOBE_TERMS_POPUP_HELP_CLOSE_BUTTON);
  builder->Add("arcOverlayLoading", IDS_ARC_POPUP_HELP_LOADING);
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

  int message_id;
  if (owner_profile && !managed) {
    if (is_child_account_) {
      message_id = enabled ? IDS_ARC_OOBE_TERMS_DIALOG_METRICS_ENABLED_CHILD
                           : IDS_ARC_OOBE_TERMS_DIALOG_METRICS_DISABLED_CHILD;
    } else {
      message_id = enabled ? IDS_ARC_OOBE_TERMS_DIALOG_METRICS_ENABLED
                           : IDS_ARC_OOBE_TERMS_DIALOG_METRICS_DISABLED;
    }
  } else {
    if (is_child_account_) {
      message_id =
          enabled ? IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_ENABLED_CHILD
                  : IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_DISABLED_CHILD;
    } else {
      message_id = enabled ? IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_ENABLED
                           : IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_DISABLED;
    }
  }
  CallJS("login.ArcTermsOfServiceScreen.setMetricsMode",
         l10n_util::GetStringUTF16(message_id), true);
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

  // Hide the Skip button if the ToS screen can not be skipped during OOBE.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableArcOobeOptinNoSkip) ||
      arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile)) {
    CallJS("login.ArcTermsOfServiceScreen.hideSkipButton");
  }

  action_taken_ = false;

  ShowScreen(kScreenId);

  arc_managed_ = arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile);
  CallJS("login.ArcTermsOfServiceScreen.setArcManaged", arc_managed_);

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
  DCHECK(identity_manager->HasPrimaryAccount());
  const std::string account_id = identity_manager->GetPrimaryAccountId();

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

void ArcTermsOfServiceScreenHandler::HandleSkip(
    const std::string& tos_content) {
  DCHECK(!arc::IsArcDemoModeSetupFlow());

  if (!NeedDispatchEventOnAction())
    return;

  // Record consents as not accepted for consents that are under user control
  // when the user skips ARC setup.
  RecordConsents(tos_content, !arc_managed_, /*tos_accepted=*/false,
                 !backup_restore_managed_, /*backup_accepted=*/false,
                 !location_services_managed_, /*location_accepted=*/false);

  for (auto& observer : observer_list_)
    observer.OnSkip();
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
