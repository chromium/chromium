// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/people/people_section.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/edusumer/graduation_utils.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/account_manager/account_apps_availability_factory.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_features_util.h"
#include "chrome/browser/ui/webui/ash/settings/pages/people/account_manager_ui_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/people/fingerprint_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/people/parental_controls_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/people/quick_unlock_handler.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/people_handler.h"
#include "chrome/browser/ui/webui/settings/profile_info_handler.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/account_manager_core/pref_names.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kMyAccountsSubpagePath;
using ::chromeos::settings::mojom::kPeopleSectionPath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const char* GetAccountsPath() {
  return ash::features::IsOsSettingsRevampWayfindingEnabled()
             ? mojom::kPeopleSectionPath
             : mojom::kMyAccountsSubpagePath;
}

const std::vector<SearchConcept>& GetPeopleSearchConcepts() {
  const char* kAccountsPath = GetAccountsPath();
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PEOPLE_ACCOUNTS,
       kAccountsPath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kMyAccounts}},
      {IDS_OS_SETTINGS_TAG_PEOPLE_V2,
       mojom::kPeopleSectionPath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kPeople}},
      {IDS_OS_SETTINGS_TAG_PEOPLE_ACCOUNTS_ADD_V2,
       kAccountsPath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddAccount}},
  });

  return *tags;
}

const std::vector<SearchConcept>& GetRemoveAccountSearchConcepts(
    const char* accounts_path) {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PEOPLE_ACCOUNTS_REMOVE,
       accounts_path,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRemoveAccount}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetParentalSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PARENTAL_CONTROLS,
       mojom::kPeopleSectionPath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSetUpParentalControls},
       {IDS_OS_SETTINGS_TAG_PARENTAL_CONTROLS_ALT1,
        IDS_OS_SETTINGS_TAG_PARENTAL_CONTROLS_ALT2, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

void AddAccountManagerPageStrings(content::WebUIDataSource* html_source,
                                  Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"accountManagerChildFirstMessage",
       IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_FIRST_MESSAGE},
      {"accountManagerChildSecondMessage",
       IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_SECOND_MESSAGE},
      {"accountManagerEducationAccountLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_EDUCATION_ACCOUNT},
      {"removeAccountLabel", IDS_SETTINGS_ACCOUNT_MANAGER_REMOVE_ACCOUNT_LABEL},
      {"addSchoolAccountLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_ADD_SCHOOL_ACCOUNT_LABEL},
      {"accountManagerSecondaryAccountsDisabledChildText",
       IDS_SETTINGS_ACCOUNT_MANAGER_SECONDARY_ACCOUNTS_DISABLED_CHILD_TEXT},
      {"accountManagerSignedOutAccountName",
       IDS_SETTINGS_ACCOUNT_MANAGER_SIGNED_OUT_ACCOUNT_PLACEHOLDER},
      {"accountManagerUnmigratedAccountName",
       IDS_SETTINGS_ACCOUNT_MANAGER_UNMIGRATED_ACCOUNT_PLACEHOLDER},
      {"accountManagerMigrationLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_MIGRATION_LABEL},
      {"accountManagerReauthenticationLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_REAUTHENTICATION_LABEL},
      {"accountManagerMigrationTooltip",
       IDS_SETTINGS_ACCOUNT_MANAGER_MIGRATION_TOOLTIP},
      {"accountManagerReauthenticationTooltip",
       IDS_SETTINGS_ACCOUNT_MANAGER_REAUTHENTICATION_TOOLTIP},
      {"accountManagerMoreActionsTooltip",
       IDS_SETTINGS_ACCOUNT_MANAGER_MORE_ACTIONS_TOOLTIP},
      {"addAccountLabel", IDS_SETTINGS_ACCOUNT_MANAGER_ADD_ACCOUNT_LABEL_V2},
      {"accountListHeader", IDS_SETTINGS_ACCOUNT_MANAGER_LIST_HEADER_V2},
      {"accountListHeaderChild",
       IDS_SETTINGS_ACCOUNT_MANAGER_LIST_HEADER_CHILD},
      {"accountManagerChildDescription",
       IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_DESCRIPTION_V2},
      {"accountManagerSecondaryAccountsDisabledText",
       IDS_SETTINGS_ACCOUNT_MANAGER_SECONDARY_ACCOUNTS_DISABLED_TEXT_V2},
      {"removeLacrosAccountDialogTitle",
       IDS_SETTINGS_ACCOUNT_MANAGER_REMOVE_LACROS_ACCOUNT_DIALOG_TITLE},
      {"removeLacrosAccountDialogBody",
       IDS_SETTINGS_ACCOUNT_MANAGER_REMOVE_LACROS_ACCOUNT_DIALOG_BODY},
      {"removeLacrosAccountDialogRemove",
       IDS_SETTINGS_ACCOUNT_MANAGER_REMOVE_LACROS_ACCOUNT_DIALOG_REMOVE},
      {"removeLacrosAccountDialogCancel",
       IDS_SETTINGS_ACCOUNT_MANAGER_REMOVE_LACROS_ACCOUNT_DIALOG_CANCEL},
      {"accountNotUsedInArcLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_NOT_USED_IN_ARC_LABEL},
      {"accountNotAllowedInArcLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_NOT_ALLOWED_IN_ARC_LABEL},
      {"accountUseInArcButtonLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_USE_IN_ARC_BUTTON_LABEL},
      {"accountStopUsingInArcButtonLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_STOP_USING_IN_ARC_BUTTON_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  if (AccountAppsAvailability::IsArcAccountRestrictionsEnabled()) {
    html_source->AddString("accountListDescription",
                           l10n_util::GetStringFUTF16(
                               IDS_SETTINGS_ACCOUNT_MANAGER_LIST_DESCRIPTION_V2,
                               ui::GetChromeOSDeviceName()));
  } else {
    html_source->AddLocalizedString(
        "accountListDescription",
        IDS_SETTINGS_ACCOUNT_MANAGER_LIST_DESCRIPTION);
  }

  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);
  html_source->AddString(
      "accountListChildDescription",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ACCOUNT_MANAGER_LIST_CHILD_DESCRIPTION,
          base::UTF8ToUTF16(user->GetDisplayEmail())));

  html_source->AddString("accountManagerLearnMoreUrl",
                         chrome::kAccountManagerLearnMoreURL);
  html_source->AddLocalizedString(
      "accountManagerManagementDescription",
      profile->IsChild() ? IDS_SETTINGS_ACCOUNT_MANAGER_MANAGEMENT_STATUS_CHILD
                         : IDS_SETTINGS_ACCOUNT_MANAGER_MANAGEMENT_STATUS);
  html_source->AddString("accountManagerChromeUIManagementURL",
                         chrome::kChromeUIManagementURL16);
  html_source->AddString(
      "accountManagerDescription",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_ACCOUNT_MANAGER_DESCRIPTION_V2,
                                 ui::GetChromeOSDeviceName()));
  html_source->AddBoolean("lacrosEnabled",
                          crosapi::browser_util::IsLacrosEnabled());
  html_source->AddBoolean(
      "arcAccountRestrictionsEnabled",
      AccountAppsAvailability::IsArcAccountRestrictionsEnabled());
  html_source->AddBoolean(
      "arcManagedAccountRestrictionEnabled",
      AccountAppsAvailability::IsArcManagedAccountRestrictionEnabled());
}

void AddLockScreenPageStrings(content::WebUIDataSource* html_source,
                              Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"lockScreenNotificationTitle",
       IDS_ASH_SETTINGS_LOCK_SCREEN_NOTIFICATION_TITLE},
      {"lockScreenNotificationHideSensitive",
       IDS_ASH_SETTINGS_LOCK_SCREEN_NOTIFICATION_HIDE_SENSITIVE},
      {"enableScreenlock", IDS_SETTINGS_PEOPLE_ENABLE_SCREENLOCK},
      {"lockScreenNotificationShow",
       IDS_ASH_SETTINGS_LOCK_SCREEN_NOTIFICATION_SHOW},
      {"lockScreenPinOrPassword",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PIN_OR_PASSWORD},
      {"lockScreenPinMoreButtonAriaLabel",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PIN_MORE_BUTTON_ARIA_LABEL},
      {"lockScreenPinAutoSubmit",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PIN_AUTOSUBMIT},
      {"lockScreenSetupFingerprintButton",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_SETUP_BUTTON},
      {"lockScreenNotificationHide",
       IDS_ASH_SETTINGS_LOCK_SCREEN_NOTIFICATION_HIDE},
      {"lockScreenEditFingerprints",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_EDIT_FINGERPRINTS},
      {"lockScreenPasswordOnly", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PASSWORD_ONLY},
      {"lockScreenChangePasswordButton",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_CHANGE_PASSWORD_BUTTON},
      {"lockScreenChangePinButton",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_CHANGE_PIN_BUTTON},
      {"lockScreenEditFingerprintsDescription",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_EDIT_FINGERPRINTS_DESCRIPTION},
      {"lockScreenNone", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NONE},
      {"lockScreenFingerprintNewName",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NEW_FINGERPRINT_DEFAULT_NAME},
      {"lockScreenDeleteFingerprintLabel",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_DELETE_FINGERPRINT_ARIA_LABEL},
      {"lockScreenOptionsLock", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_OPTIONS_LOCK},
      {"lockScreenOptionsLoginLock",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_OPTIONS_LOGIN_LOCK},
      {"lockScreenRemovePinButton",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_REMOVE_PIN_BUTTON},
      {"lockScreenPasswordLabel",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PASSWORD_LABEL},
      {"lockScreenPinLabel", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PIN_LABEL},
      {"lockScreenSetupPasswordButton",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_SETUP_PASSWORD_BUTTON},
      {"lockScreenSetupPinButton",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_SETUP_PIN_BUTTON},
      {"lockScreenSignInOptions",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_SIGN_IN_OPTIONS},
      {"lockScreenTitleLock", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE_LOCK},
      {"lockScreenTitleLoginLock",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE_LOGIN_LOCK_V2},
      {"passwordPromptEnterPasswordLock",
       IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_ENTER_PASSWORD_LOCK},
      {"pinAutoSubmitPrompt",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PIN_AUTOSUBMIT_PROMPT},
      {"pinAutoSubmitLongPinError",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PIN_AUTOSUBMIT_ERROR_PIN_TOO_LONG},
      {"pinAutoSubmitPinIncorrect",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PIN_AUTOSUBMIT_ERROR_PIN_INCORRECT},
      {"passwordPromptEnterPasswordLoginLock",
       IDS_SETTINGS_PEOPLE_PASSWORD_PROMPT_ENTER_PASSWORD_LOGIN_LOCK},
      {"recoveryToggleLabel", IDS_SETTINGS_PEOPLE_RECOVERY_TOGGLE_LABEL},
      {"recoveryToggleSubLabel", IDS_SETTINGS_PEOPLE_RECOVERY_TOGGLE_SUB_LABEL},
      {"recoveryDisableDialogTitle",
       IDS_SETTINGS_PEOPLE_RECOVERY_DISABLE_DIALOG_TITLE},
      {"recoveryDisableDialogMessage",
       IDS_SETTINGS_PEOPLE_RECOVERY_DISABLE_DIALOG_MESSAGE},
      {"recoveryNotSupportedMessage",
       IDS_SETTINGS_PEOPLE_RECOVERY_NOT_SUPPORTED_MESSAGE},
      {"setLocalPasswordDialogTitle",
       IDS_OS_SETTINGS_PEOPLE_SET_LOCAL_PASSWORD_DIALOG_TITLE},
      {"setLocalPasswordDialogInternalError",
       IDS_OS_SETTINGS_PEOPLE_SET_LOCAL_PASSWORD_DIALOG_INTERNAL_ERROR},
      {"showPassword", IDS_AUTH_SETUP_SHOW_PASSWORD},
      {"hidePassword", IDS_AUTH_SETUP_HIDE_PASSWORD},
      {"setLocalPasswordPlaceholder",
       IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_PLACEHOLDER},
      {"setLocalPasswordConfirmPlaceholder",
       IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_CONFIRM_PLACEHOLDER},
      {"setLocalPasswordMinCharsHint",
       IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_MIN_CHARS_HINT},
      {"setLocalPasswordNoMatchError",
       IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_NO_MATCH_ERROR},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("quickUnlockEnabled", quick_unlock::IsPinEnabled());
  html_source->AddBoolean(
      "quickUnlockDisabledByPolicy",
      quick_unlock::IsPinDisabledByPolicy(profile->GetPrefs(),
                                          quick_unlock::Purpose::kAny));
  html_source->AddBoolean("lockScreenNotificationsEnabled",
                          ash::features::IsLockScreenNotificationsEnabled());
  html_source->AddBoolean(
      "lockScreenHideSensitiveNotificationsSupported",
      ash::features::IsLockScreenHideSensitiveNotificationsSupported());
  html_source->AddBoolean("changePasswordFactorSetupEnabled",
                          ash::features::IsChangePasswordFactorSetupEnabled());

  html_source->AddString(
      "lockScreenSwitchLocalPasswordDescription",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PEOPLE_LOCK_SCREEN_SWITCH_LOCAL_PASSWORD_DESCRIPTION,
          ui::GetChromeOSDeviceName()));
  html_source->AddString(
      "lockScreenSwitchSetLocalPasswordDescription",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PASSWORD_DESCRIPTION,
          ui::GetChromeOSDeviceName()));
  html_source->AddString("lockScreenFingerprintNotice",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_NOTICE,
                             ui::GetChromeOSDeviceName()));
  html_source->AddString("fingerprintLearnMoreLink",
                         chrome::kFingerprintLearnMoreURL);
  html_source->AddString("recoveryLearnMoreUrl", chrome::kRecoveryLearnMoreURL);
}

void AddFingerprintListStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"lockScreenAddFingerprint",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_ADD_FINGERPRINT_BUTTON},
      {"lockScreenRegisteredFingerprints",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_REGISTERED_FINGERPRINTS_LABEL},
      {"lockScreenFingerprintWarning",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_LESS_SECURE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddFingerprintResources(content::WebUIDataSource* html_source,
                             bool are_fingerprint_settings_allowed) {
  html_source->AddBoolean("fingerprintUnlockEnabled",
                          are_fingerprint_settings_allowed);

  html_source->AddResourcePath("fingerprint_scanner_animation.json",
                               IDR_FINGERPRINT_DEFAULT_ANIMATION);

  if (are_fingerprint_settings_allowed) {
    quick_unlock::AddFingerprintResources(html_source);
  }

  auto fp_setup_strings = quick_unlock::GetFingerprintDescriptionStrings(
      quick_unlock::GetFingerprintLocation());
  html_source->AddString(
      "configureFingerprintInstructionLocateScannerStep",
      l10n_util::GetStringFUTF16(fp_setup_strings.description_id,
                                 ui::GetChromeOSDeviceName()));
}

void AddSetupFingerprintDialogStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"configureFingerprintTitle", IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_TITLE},
      {"configureFingerprintAddAnotherButton",
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_ADD_ANOTHER_BUTTON},
      {"configureFingerprintInstructionReadyStep",
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_READY},
      {"configureFingerprintLiftFinger",
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_LIFT_FINGER},
      {"configureFingerprintTryAgain",
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_TRY_AGAIN},
      {"configureFingerprintImmobile",
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_FINGER_IMMOBILE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddSetupPinDialogStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"configurePinChoosePinTitle",
       IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_CHOOSE_PIN_TITLE},
      {"configurePinConfirmPinTitle",
       IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_CONFIRM_PIN_TITLE},
      {"invalidPin", IDS_SETTINGS_PEOPLE_PIN_PROMPT_INVALID_PIN},
      {"configurePinMismatched", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_MISMATCHED},
      {"configurePinTooShort", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_TOO_SHORT},
      {"configurePinTooLong", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_TOO_LONG},
      {"configurePinWeakPin", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_WEAK_PIN},
      {"configurePinNondigit", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_NONDIGIT},
      {"internalError", IDS_SETTINGS_PEOPLE_CONFIGURE_PIN_INTERNAL_ERROR},
      {"pinKeyboardPlaceholderPin", IDS_PIN_KEYBOARD_HINT_TEXT_PIN},
      {"pinKeyboardPlaceholderPinPassword",
       IDS_PIN_KEYBOARD_HINT_TEXT_PIN_PASSWORD},
      {"pinKeyboardDeleteAccessibleName",
       IDS_PIN_KEYBOARD_DELETE_ACCESSIBLE_NAME},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  // Format numbers to be used on the pin keyboard.
  for (int j = 0; j <= 9; j++) {
    html_source->AddString("pinKeyboard" + base::NumberToString(j),
                           base::FormatNumber(int64_t{j}));
  }
}

void AddUsersStrings(content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kLocalizedStrings[] = {
      {"usersModifiedByOwnerLabel", IDS_SETTINGS_USERS_MODIFIED_BY_OWNER_LABEL},
      {"guestBrowsingLabel",
       kIsRevampEnabled ? IDS_OS_SETTINGS_REVAMP_USERS_GUEST_BROWSING_LABEL
                        : IDS_SETTINGS_USERS_GUEST_BROWSING_LABEL},
      {"settingsManagedLabel", IDS_SETTINGS_USERS_MANAGED_LABEL},
      {"showOnSigninLabel", IDS_SETTINGS_USERS_SHOW_ON_SIGNIN_LABEL},
      {"restrictSigninLabel",
       kIsRevampEnabled ? IDS_OS_SETTINGS_REVAMP_USERS_RESTRICT_SIGNIN_LABEL
                        : IDS_SETTINGS_USERS_RESTRICT_SIGNIN_LABEL},
      {"restrictSigninDescription",
       IDS_OS_SETTINGS_REVAMP_USERS_RESTRICT_SIGNIN_DESCRIPTION},
      {"deviceOwnerLabel", IDS_SETTINGS_USERS_DEVICE_OWNER_LABEL},
      {"removeUserTooltip", IDS_SETTINGS_USERS_REMOVE_USER_TOOLTIP},
      {"userRemovedMessage", IDS_SETTINGS_USERS_USER_REMOVED_MESSAGE},
      {"userAddedMessage", IDS_SETTINGS_USERS_USER_ADDED_MESSAGE},
      {"addUsers", IDS_SETTINGS_USERS_ADD_USERS},
      {"addUsersEmail", IDS_SETTINGS_USERS_ADD_USERS_EMAIL},
      {"userExistsError", IDS_SETTINGS_USER_EXISTS_ERROR},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddParentalControlStrings(content::WebUIDataSource* html_source,
                               bool are_parental_control_settings_allowed) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"parentalControlsPageTitle", IDS_SETTINGS_PARENTAL_CONTROLS_PAGE_TITLE},
      {"parentalControlsPageSetUpLabel",
       IDS_SETTINGS_PARENTAL_CONTROLS_PAGE_SET_UP_LABEL},
      {"parentalControlsPageViewSettingsLabel",
       IDS_SETTINGS_PARENTAL_CONTROLS_PAGE_VIEW_SETTINGS_LABEL},
      {"parentalControlsPageConnectToInternetLabel",
       IDS_SETTINGS_PARENTAL_CONTROLS_PAGE_CONNECT_TO_INTERNET_LABEL},
      {"parentalControlsSetUpButtonLabel",
       IDS_SETTINGS_PARENTAL_CONTROLS_SET_UP_BUTTON_LABEL},
      {"parentalControlsSetUpButtonRole",
       IDS_SETTINGS_PARENTAL_CONTROLS_SET_UP_BUTTON_ROLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("showParentalControls",
                          are_parental_control_settings_allowed);
}

void AddGraduationStrings(content::WebUIDataSource* html_source,
                          Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"graduationSectionTitle", IDS_SETTINGS_GRADUATION_SECTION_TITLE},
      {"graduationRowTitle", IDS_SETTINGS_GRADUATION_ROW_TITLE},
      {"graduationRowSubtitle", IDS_SETTINGS_GRADUATION_ROW_SUBTITLE}};

  html_source->AddLocalizedStrings(kLocalizedStrings);

  bool is_graduation_app_enabled =
      profile->GetProfilePolicyConnector()->IsManaged() &&
      graduation::IsEligibleForGraduation(profile->GetPrefs());
  html_source->AddBoolean("isGraduationFlagEnabled",
                          features::IsGraduationEnabled());
  html_source->AddBoolean("isGraduationAppEnabled", is_graduation_app_enabled);
}

bool IsSameAccount(const ::account_manager::AccountKey& account_key,
                   const AccountId& account_id) {
  switch (account_key.account_type()) {
    case account_manager::AccountType::kGaia:
      return account_id.GetAccountType() == AccountType::GOOGLE &&
             account_id.GetGaiaId() == account_key.id();
    case account_manager::AccountType::kActiveDirectory:
      return account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY &&
             account_id.GetObjGuid() == account_key.id();
  }
}

}  // namespace

PeopleSection::PeopleSection(Profile* profile,
                             SearchTagRegistry* search_tag_registry,
                             signin::IdentityManager* identity_manager,
                             PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      sync_subsection_(
          !ash::features::IsOsSettingsRevampWayfindingEnabled()
              ? std::make_optional<SyncSection>(profile, search_tag_registry)
              : std::nullopt),
      identity_manager_(identity_manager),
      pref_service_(pref_service),
      auth_performer_(UserDataAuthClient::Get()),
      fp_engine_(&auth_performer_) {
  // No search tags are registered if in guest mode.
  if (IsGuestModeActive()) {
    return;
  }

  if (!ash::features::IsOsSettingsRevampWayfindingEnabled()) {
    CHECK(sync_subsection_);
  }

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetPeopleSearchConcepts());

  // TODO(jamescook): Sort out how account management is split between Chrome
  // OS and browser settings.
  if (IsAccountManagerAvailable(profile)) {
    // Some Account Manager search tags are added/removed dynamically.
    auto* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    account_manager_ = factory->GetAccountManager(profile->GetPath().value());
    DCHECK(account_manager_);
    account_manager_facade_ =
        ::GetAccountManagerFacade(profile->GetPath().value());
    DCHECK(account_manager_facade_);
    account_manager_facade_observation_.Observe(account_manager_facade_.get());
    account_apps_availability_ =
        AccountAppsAvailabilityFactory::GetForProfile(profile);
    FetchAccounts();
  }

  // Parental control search tags are added if necessary and do not update
  // dynamically during a user session.
  if (ShouldShowParentalControlSettings(profile)) {
    updater.AddSearchTags(GetParentalSearchConcepts());
  }
}

PeopleSection::~PeopleSection() = default;

void PeopleSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"osPeoplePageTitle", IDS_OS_SETTINGS_PEOPLE_V2},
      {"accountsMenuItemDescription",
       IDS_OS_SETTINGS_ACCOUNTS_MENU_ITEM_DESCRIPTION},
      {"accountManagerSubMenuLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_SUBMENU_LABEL},
      {"accountManagerPageTitle", IDS_SETTINGS_ACCOUNT_MANAGER_PAGE_TITLE},
      {"lockScreenFingerprintTitle",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_SUBPAGE_TITLE},
      {"manageOtherPeople", IDS_SETTINGS_PEOPLE_MANAGE_OTHER_PEOPLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile());
  DCHECK(user);

  html_source->AddString(
      "osProfileName", l10n_util::GetStringFUTF16(IDS_OS_SETTINGS_PROFILE_NAME,
                                                  user->GetGivenName()));
  html_source->AddString(
      "accountManagerPageTitle",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_ACCOUNT_MANAGER_PAGE_TITLE_V2,
                                 user->GetGivenName()));

  // Toggles the ChromeOS Account Manager submenu in the People section.
  html_source->AddBoolean("isAccountManagerEnabled",
                          account_manager_facade_ != nullptr);
  html_source->AddBoolean("isDeviceAccountManaged",
                          profile()->GetProfilePolicyConnector()->IsManaged());

  html_source->AddBoolean(
      "secondaryGoogleAccountSigninAllowed",
      pref_service_->GetBoolean(
          ::account_manager::prefs::kSecondaryGoogleAccountSigninAllowed));

  html_source->AddBoolean(
      "driveSuggestAvailable",
      base::FeatureList::IsEnabled(omnibox::kDocumentProvider));

  AddAccountManagerPageStrings(html_source, profile());
  AddLockScreenPageStrings(html_source, profile());
  AddFingerprintListStrings(html_source);
  AddFingerprintResources(html_source, AreFingerprintSettingsAllowed());
  AddSetupFingerprintDialogStrings(html_source);
  AddSetupPinDialogStrings(html_source);
  AddUsersStrings(html_source);
  AddParentalControlStrings(html_source,
                            ShouldShowParentalControlSettings(profile()));
  AddGraduationStrings(html_source, profile());

  ::settings::AddPasswordPromptDialogStrings(html_source);

  // `sync_subsection_` is initialized only if the feature revamp wayfinding is
  // disabled.
  if (sync_subsection_) {
    sync_subsection_->AddLoadTimeData(html_source);
  }
}

void PeopleSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::PeopleHandler>(profile()));
  web_ui->AddMessageHandler(
      std::make_unique<::settings::ProfileInfoHandler>(profile()));

  if (account_manager_facade_) {
    web_ui->AddMessageHandler(std::make_unique<AccountManagerUIHandler>(
        account_manager_, account_manager_facade_, identity_manager_,
        account_apps_availability_));
  }

  web_ui->AddMessageHandler(
      std::make_unique<QuickUnlockHandler>(profile(), pref_service_));

  web_ui->AddMessageHandler(std::make_unique<FingerprintHandler>(profile()));

  if (!profile()->IsGuestSession() &&
      ShouldShowParentalControlSettings(profile())) {
    web_ui->AddMessageHandler(
        std::make_unique<ParentalControlsHandler>(profile()));
  }

  // `sync_subsection_` is initialized only if the feature revamp wayfinding is
  // disabled.
  if (sync_subsection_) {
    sync_subsection_->AddHandlers(web_ui);
  }
}

int PeopleSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_PEOPLE_V2;
}

mojom::Section PeopleSection::GetSection() const {
  return mojom::Section::kPeople;
}

mojom::SearchResultIcon PeopleSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kAvatar;
}

const char* PeopleSection::GetSectionPath() const {
  return mojom::kPeopleSectionPath;
}

bool PeopleSection::LogMetric(mojom::Setting setting,
                              base::Value& value) const {
  switch (setting) {
    case mojom::Setting::kAddAccount:
      base::UmaHistogramCounts1000("ChromeOS.Settings.People.AddAccountCount",
                                   value.GetInt());
      return true;

    default:
      return false;
  }
}

void PeopleSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kSetUpParentalControls);

  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_ACCOUNT_MANAGER_PAGE_TITLE, mojom::Subpage::kMyAccounts,
      mojom::SearchResultIcon::kAvatar, mojom::SearchResultDefaultRank::kMedium,
      mojom::kMyAccountsSubpagePath);

  // My accounts.
  if (ash::features::IsOsSettingsRevampWayfindingEnabled()) {
    // Accounts settings are up-leveled to the top level page if the revamp
    // wayfind is enabled.
    generator->RegisterTopLevelSetting(mojom::Setting::kAddAccount);
    generator->RegisterTopLevelSetting(mojom::Setting::kRemoveAccount);
  } else {
    static constexpr mojom::Setting kMyAccountsSettings[] = {
        mojom::Setting::kAddAccount,
        mojom::Setting::kRemoveAccount,
    };
    RegisterNestedSettingBulk(mojom::Subpage::kMyAccounts, kMyAccountsSettings,
                              generator);
  }

  // Smart Lock -- main setting is on multidevice page, but is mirrored here
  generator->RegisterNestedAltSetting(mojom::Setting::kSmartLockOnOff,
                                      mojom::Subpage::kSecurityAndSignInV2);

  // `sync_subsection_` is initialized only if the feature revamp wayfinding is
  // disabled.
  if (sync_subsection_) {
    sync_subsection_->RegisterHierarchy(generator);
  }
}

void PeopleSection::FetchAccounts() {
  account_manager_facade_->GetAccounts(
      base::BindOnce(&PeopleSection::UpdateAccountManagerSearchTags,
                     weak_factory_.GetWeakPtr()));
}

void PeopleSection::OnAccountUpserted(
    const ::account_manager::Account& account) {
  FetchAccounts();
}

void PeopleSection::OnAccountRemoved(
    const ::account_manager::Account& account) {
  FetchAccounts();
}

void PeopleSection::OnAuthErrorChanged(
    const account_manager::AccountKey& account,
    const GoogleServiceAuthError& error) {
  // Nothing to do.
}

void PeopleSection::UpdateAccountManagerSearchTags(
    const std::vector<::account_manager::Account>& accounts) {
  DCHECK(IsAccountManagerAvailable(profile()));

  // Start with no Account Manager search tags.
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetRemoveAccountSearchConcepts(GetAccountsPath()));

  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile());
  DCHECK(user);

  for (const ::account_manager::Account& account : accounts) {
    if (IsSameAccount(account.key, user->GetAccountId())) {
      continue;
    }

    // If a non-device account exists, add the "Remove Account" search tag.
    updater.AddSearchTags(GetRemoveAccountSearchConcepts(GetAccountsPath()));
    return;
  }
}

bool PeopleSection::AreFingerprintSettingsAllowed() {
  return fp_engine_.IsFingerprintEnabled(
      *profile()->GetPrefs(), LegacyFingerprintEngine::Purpose::kAny);
}

}  // namespace ash::settings
