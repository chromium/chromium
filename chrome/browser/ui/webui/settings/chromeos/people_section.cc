// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/people_section.h"

#include "ash/public/cpp/ash_features.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/chromeos/sync/os_sync_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/account_manager_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/fingerprint_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/kerberos_accounts_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_features_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/parental_controls_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/quick_unlock_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/settings/people_handler.h"
#include "chrome/browser/ui/webui/settings/profile_info_handler.h"
#include "chrome/browser/ui/webui/settings/shared_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetPeopleSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PEOPLE,
       mojom::kPeopleSectionPath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kPeople}},
      {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_PIN_OR_PASSWORD,
       mojom::kSecurityAndSignInSubpagePath,
       mojom::SearchResultIcon::kLock,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kChangeAuthPin},
       {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_PIN_OR_PASSWORD_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_USERNAMES_AND_PHOTOS,
       mojom::kManageOtherPeopleSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kShowUsernamesAndPhotosAtSignIn},
       {IDS_OS_SETTINGS_TAG_USERNAMES_AND_PHOTOS_ALT1,
        IDS_OS_SETTINGS_TAG_USERNAMES_AND_PHOTOS_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_PEOPLE_ACCOUNTS,
       mojom::kMyAccountsSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kMyAccounts}},
      {IDS_OS_SETTINGS_TAG_PEOPLE_ACCOUNTS_ADD,
       mojom::kMyAccountsSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddAccount}},
      {IDS_OS_SETTINGS_TAG_RESTRICT_SIGN_IN_REMOVE,
       mojom::kManageOtherPeopleSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRemoveFromUserWhitelist}},
      {IDS_OS_SETTINGS_TAG_GUEST_BROWSING,
       mojom::kManageOtherPeopleSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kGuestBrowsing}},
      {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_WHEN_WAKING,
       mojom::kSecurityAndSignInSubpagePath,
       mojom::SearchResultIcon::kLock,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kLockScreen},
       {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_WHEN_WAKING_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_RESTRICT_SIGN_IN,
       mojom::kManageOtherPeopleSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRestrictSignIn},
       {IDS_OS_SETTINGS_TAG_RESTRICT_SIGN_IN_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_LOCK_SCREEN,
       mojom::kSecurityAndSignInSubpagePath,
       mojom::SearchResultIcon::kLock,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kSecurityAndSignIn}},
      {IDS_OS_SETTINGS_TAG_RESTRICT_SIGN_IN_ADD,
       mojom::kManageOtherPeopleSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddToUserWhitelist}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetRemoveAccountSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PEOPLE_ACCOUNTS_REMOVE,
       mojom::kMyAccountsSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRemoveAccount}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetNonSplitSyncSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_SYNC_AND_GOOGLE_SERVICES,
       mojom::kSyncDeprecatedSubpagePath,
       mojom::SearchResultIcon::kSync,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kSyncDeprecated}},
      {IDS_OS_SETTINGS_TAG_SYNC_MANAGEMENT,
       mojom::kSyncDeprecatedAdvancedSubpagePath,
       mojom::SearchResultIcon::kSync,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kSyncDeprecatedAdvanced}},
      {IDS_OS_SETTINGS_TAG_SYNC_ENCRYPTION_OPTIONS,
       mojom::kSyncDeprecatedSubpagePath,
       mojom::SearchResultIcon::kSync,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kNonSplitSyncEncryptionOptions},
       {IDS_OS_SETTINGS_TAG_SYNC_ENCRYPTION_OPTIONS_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_AUTOCOMPLETE_SEARCHES_AND_URLS,
       mojom::kSyncDeprecatedSubpagePath,
       mojom::SearchResultIcon::kSync,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAutocompleteSearchesAndUrls}},
      {IDS_OS_SETTINGS_TAG_MAKE_SEARCHES_AND_BROWSER_BETTER,
       mojom::kSyncDeprecatedSubpagePath,
       mojom::SearchResultIcon::kSync,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMakeSearchesAndBrowsingBetter}},
      {IDS_OS_SETTINGS_TAG_GOOGLE_DRIVE_SEARCH_SUGGESTIONS,
       mojom::kSyncDeprecatedSubpagePath,
       mojom::SearchResultIcon::kSync,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kGoogleDriveSearchSuggestions}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetSplitSyncSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_SYNC,
       mojom::kSyncSubpagePath,
       mojom::SearchResultIcon::kSync,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kSync}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetSplitSyncOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_SYNC_TURN_OFF,
       mojom::kSyncSubpagePath,
       mojom::SearchResultIcon::kSync,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSplitSyncOnOff},
       {IDS_OS_SETTINGS_TAG_SYNC_TURN_OFF_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetSplitSyncOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_SYNC_TURN_ON,
       mojom::kSyncSubpagePath,
       mojom::SearchResultIcon::kSync,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSplitSyncOnOff},
       {IDS_OS_SETTINGS_TAG_SYNC_TURN_ON_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetKerberosSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_KERBEROS_ADD,
       mojom::kKerberosSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddKerberosTicket}},
      {IDS_OS_SETTINGS_TAG_KERBEROS_REMOVE,
       mojom::kKerberosSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRemoveKerberosTicket}},
      {IDS_OS_SETTINGS_TAG_KERBEROS,
       mojom::kKerberosSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kKerberos}},
      {IDS_OS_SETTINGS_TAG_KERBEROS_ACTIVE,
       mojom::kKerberosSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kSetActiveKerberosTicket}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetFingerprintSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_FINGERPRINT_ADD,
       mojom::kFingerprintSubpagePath,
       mojom::SearchResultIcon::kFingerprint,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddFingerprint}},
      {IDS_OS_SETTINGS_TAG_FINGERPRINT,
       mojom::kFingerprintSubpagePath,
       mojom::SearchResultIcon::kFingerprint,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kFingerprint}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetRemoveFingerprintSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_FINGERPRINT_REMOVE,
       mojom::kFingerprintSubpagePath,
       mojom::SearchResultIcon::kFingerprint,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRemoveFingerprint}},
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

void AddAccountManagerPageStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"accountManagerDescription", IDS_SETTINGS_ACCOUNT_MANAGER_DESCRIPTION},
      {"accountManagerChildDescription",
       IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_DESCRIPTION},
      {"accountManagerChildFirstMessage",
       IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_FIRST_MESSAGE},
      {"accountManagerChildSecondMessage",
       IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_SECOND_MESSAGE},
      {"accountListHeader", IDS_SETTINGS_ACCOUNT_MANAGER_LIST_HEADER},
      {"accountManagerPrimaryAccountTooltip",
       IDS_SETTINGS_ACCOUNT_MANAGER_PRIMARY_ACCOUNT_TOOLTIP},
      {"accountManagerEducationAccountLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_EDUCATION_ACCOUNT},
      {"removeAccountLabel", IDS_SETTINGS_ACCOUNT_MANAGER_REMOVE_ACCOUNT_LABEL},
      {"addAccountLabel", IDS_SETTINGS_ACCOUNT_MANAGER_ADD_ACCOUNT_LABEL},
      {"addSchoolAccountLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_ADD_SCHOOL_ACCOUNT_LABEL},
      {"accountManagerSecondaryAccountsDisabledText",
       IDS_SETTINGS_ACCOUNT_MANAGER_SECONDARY_ACCOUNTS_DISABLED_TEXT},
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
      {"accountManagerManagedLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_MANAGEMENT_STATUS_MANAGED_ACCOUNT},
      {"accountManagerUnmanagedLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_MANAGEMENT_STATUS_UNMANAGED_ACCOUNT},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddString("accountManagerLearnMoreUrl",
                         chrome::kAccountManagerLearnMoreURL);
}

void AddKerberosAddAccountDialogStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"kerberosAccountsAdvancedConfigLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_ADVANCED_CONFIG_LABEL},
      {"kerberosAdvancedConfigTitle",
       IDS_SETTINGS_KERBEROS_ADVANCED_CONFIG_TITLE},
      {"kerberosAdvancedConfigDesc",
       IDS_SETTINGS_KERBEROS_ADVANCED_CONFIG_DESC},
      {"addKerberosAccountRememberPassword",
       IDS_SETTINGS_ADD_KERBEROS_ACCOUNT_REMEMBER_PASSWORD},
      {"kerberosPassword", IDS_SETTINGS_KERBEROS_PASSWORD},
      {"kerberosUsername", IDS_SETTINGS_KERBEROS_USERNAME},
      {"addKerberosAccountDescription",
       IDS_SETTINGS_ADD_KERBEROS_ACCOUNT_DESCRIPTION},
      {"kerberosErrorNetworkProblem",
       IDS_SETTINGS_KERBEROS_ERROR_NETWORK_PROBLEM},
      {"kerberosErrorUsernameInvalid",
       IDS_SETTINGS_KERBEROS_ERROR_USERNAME_INVALID},
      {"kerberosErrorUsernameUnknown",
       IDS_SETTINGS_KERBEROS_ERROR_USERNAME_UNKNOWN},
      {"kerberosErrorDuplicatePrincipalName",
       IDS_SETTINGS_KERBEROS_ERROR_DUPLICATE_PRINCIPAL_NAME},
      {"kerberosErrorContactingServer",
       IDS_SETTINGS_KERBEROS_ERROR_CONTACTING_SERVER},
      {"kerberosErrorPasswordInvalid",
       IDS_SETTINGS_KERBEROS_ERROR_PASSWORD_INVALID},
      {"kerberosErrorPasswordExpired",
       IDS_SETTINGS_KERBEROS_ERROR_PASSWORD_EXPIRED},
      {"kerberosErrorKdcEncType", IDS_SETTINGS_KERBEROS_ERROR_KDC_ENC_TYPE},
      {"kerberosErrorGeneral", IDS_SETTINGS_KERBEROS_ERROR_GENERAL},
      {"kerberosConfigErrorSectionNestedInGroup",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_SECTION_NESTED_IN_GROUP},
      {"kerberosConfigErrorSectionSyntax",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_SECTION_SYNTAX},
      {"kerberosConfigErrorExpectedOpeningCurlyBrace",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_EXPECTED_OPENING_CURLY_BRACE},
      {"kerberosConfigErrorExtraCurlyBrace",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_EXTRA_CURLY_BRACE},
      {"kerberosConfigErrorRelationSyntax",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_RELATION_SYNTAX_ERROR},
      {"kerberosConfigErrorKeyNotSupported",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_KEY_NOT_SUPPORTED},
      {"kerberosConfigErrorSectionNotSupported",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_SECTION_NOT_SUPPORTED},
      {"kerberosConfigErrorKrb5FailedToParse",
       IDS_SETTINGS_KERBEROS_CONFIG_ERROR_KRB5_FAILED_TO_PARSE},
      {"addKerberosAccountRefreshButtonLabel",
       IDS_SETTINGS_ADD_KERBEROS_ACCOUNT_REFRESH_BUTTON_LABEL},
      {"addKerberosAccount", IDS_SETTINGS_ADD_KERBEROS_ACCOUNT},
      {"refreshKerberosAccount", IDS_SETTINGS_REFRESH_KERBEROS_ACCOUNT},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  PrefService* local_state = g_browser_process->local_state();

  // Whether the 'Remember password' checkbox is enabled.
  html_source->AddBoolean(
      "kerberosRememberPasswordEnabled",
      local_state->GetBoolean(::prefs::kKerberosRememberPasswordEnabled));

  // Kerberos default configuration.
  html_source->AddString(
      "defaultKerberosConfig",
      chromeos::KerberosCredentialsManager::GetDefaultKerberosConfig());
}

void AddLockScreenPageStrings(content::WebUIDataSource* html_source,
                              PrefService* pref_service) {
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
      {"lockScreenPinAutoSubmit",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PIN_AUTOSUBMIT},
      {"lockScreenSetupFingerprintButton",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_SETUP_BUTTON},
      {"lockScreenNotificationHide",
       IDS_ASH_SETTINGS_LOCK_SCREEN_NOTIFICATION_HIDE},
      {"lockScreenEditFingerprints",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_EDIT_FINGERPRINTS},
      {"lockScreenPasswordOnly", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_PASSWORD_ONLY},
      {"lockScreenChangePinButton",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_CHANGE_PIN_BUTTON},
      {"lockScreenEditFingerprintsDescription",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_EDIT_FINGERPRINTS_DESCRIPTION},
      {"lockScreenNumberFingerprints",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NUM_FINGERPRINTS},
      {"lockScreenNone", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NONE},
      {"lockScreenFingerprintNewName",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_NEW_FINGERPRINT_DEFAULT_NAME},
      {"lockScreenDeleteFingerprintLabel",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_DELETE_FINGERPRINT_ARIA_LABEL},
      {"lockScreenOptionsLock", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_OPTIONS_LOCK},
      {"lockScreenOptionsLoginLock",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_OPTIONS_LOGIN_LOCK},
      {"lockScreenSetupPinButton",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_SETUP_PIN_BUTTON},
      {"lockScreenTitleLock", IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE_LOCK},
      {"lockScreenTitleLoginLock",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE_LOGIN_LOCK},
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
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean("quickUnlockEnabled",
                          chromeos::quick_unlock::IsPinEnabled(pref_service));
  html_source->AddBoolean("quickUnlockPinAutosubmitFeatureEnabled",
                          chromeos::features::IsPinAutosubmitFeatureEnabled());
  html_source->AddBoolean(
      "quickUnlockDisabledByPolicy",
      chromeos::quick_unlock::IsPinDisabledByPolicy(pref_service));
  html_source->AddBoolean("lockScreenNotificationsEnabled",
                          ash::features::IsLockScreenNotificationsEnabled());
  html_source->AddBoolean(
      "lockScreenHideSensitiveNotificationsSupported",
      ash::features::IsLockScreenHideSensitiveNotificationsSupported());
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
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

void AddFingerprintStrings(content::WebUIDataSource* html_source,
                           bool are_fingerprint_settings_allowed) {
  html_source->AddBoolean("fingerprintUnlockEnabled",
                          are_fingerprint_settings_allowed);
  if (are_fingerprint_settings_allowed) {
    html_source->AddInteger(
        "fingerprintReaderLocation",
        static_cast<int32_t>(chromeos::quick_unlock::GetFingerprintLocation()));

    // To use lottie, the worker-src CSP needs to be updated for the web ui that
    // is using it. Since as of now there are only a couple of webuis using
    // lottie animations, this update has to be performed manually. As the usage
    // increases, set this as the default so manual override is no longer
    // required.
    html_source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::WorkerSrc,
        "worker-src blob: 'self';");
    html_source->AddResourcePath("finger_print.json",
                                 IDR_LOGIN_FINGER_PRINT_TABLET_ANIMATION);
  }

  int instruction_id, aria_label_id;
  using FingerprintLocation = chromeos::quick_unlock::FingerprintLocation;
  switch (chromeos::quick_unlock::GetFingerprintLocation()) {
    case FingerprintLocation::TABLET_POWER_BUTTON:
      instruction_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_POWER_BUTTON;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_POWER_BUTTON_ARIA_LABEL;
      break;
    case FingerprintLocation::KEYBOARD_BOTTOM_LEFT:
      instruction_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD_BOTTOM_LEFT_ARIA_LABEL;
      break;
    case FingerprintLocation::KEYBOARD_BOTTOM_RIGHT:
      instruction_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD_BOTTOM_RIGHT_ARIA_LABEL;
      break;
    case FingerprintLocation::KEYBOARD_TOP_RIGHT:
      instruction_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD_TOP_RIGHT_ARIA_LABEL;
      break;
  }
  html_source->AddLocalizedString(
      "configureFingerprintInstructionLocateScannerStep", instruction_id);
  html_source->AddLocalizedString("configureFingerprintScannerStepAriaLabel",
                                  aria_label_id);
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
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
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
      {"pinKeyboardPlaceholderPin", IDS_PIN_KEYBOARD_HINT_TEXT_PIN},
      {"pinKeyboardPlaceholderPinPassword",
       IDS_PIN_KEYBOARD_HINT_TEXT_PIN_PASSWORD},
      {"pinKeyboardDeleteAccessibleName",
       IDS_PIN_KEYBOARD_DELETE_ACCESSIBLE_NAME},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  // Format numbers to be used on the pin keyboard.
  for (int j = 0; j <= 9; j++) {
    html_source->AddString("pinKeyboard" + base::NumberToString(j),
                           base::FormatNumber(int64_t{j}));
  }
}

void AddSyncControlsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"syncEverythingCheckboxLabel",
       IDS_SETTINGS_SYNC_EVERYTHING_CHECKBOX_LABEL},
      {"wallpaperCheckboxLabel", IDS_OS_SETTINGS_WALLPAPER_CHECKBOX_LABEL},
      {"osSyncTurnOff", IDS_OS_SETTINGS_SYNC_TURN_OFF},
      {"osSyncSettingsCheckboxLabel",
       IDS_OS_SETTINGS_SYNC_SETTINGS_CHECKBOX_LABEL},
      {"osSyncWifiConfigurationsCheckboxLabel",
       IDS_OS_SETTINGS_WIFI_CONFIGURATIONS_CHECKBOX_LABEL},
      {"osSyncAppsCheckboxLabel", IDS_OS_SETTINGS_SYNC_APPS_CHECKBOX_LABEL},
      {"osSyncTurnOn", IDS_OS_SETTINGS_SYNC_TURN_ON},
      {"osSyncFeatureLabel", IDS_OS_SETTINGS_SYNC_FEATURE_LABEL},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean("splitSettingsSyncEnabled",
                          chromeos::features::IsSplitSettingsSyncEnabled());
  html_source->AddBoolean("useBrowserSyncConsent",
                          chromeos::features::ShouldUseBrowserSyncConsent());
  html_source->AddString(
      "browserSettingsSyncSetupUrl",
      base::StrCat({chrome::kChromeUISettingsURL, chrome::kSyncSetupSubPage}));
}

void AddUsersStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"usersModifiedByOwnerLabel", IDS_SETTINGS_USERS_MODIFIED_BY_OWNER_LABEL},
      {"guestBrowsingLabel", IDS_SETTINGS_USERS_GUEST_BROWSING_LABEL},
      {"settingsManagedLabel", IDS_SETTINGS_USERS_MANAGED_LABEL},
      {"showOnSigninLabel", IDS_SETTINGS_USERS_SHOW_ON_SIGNIN_LABEL},
      {"restrictSigninLabel", IDS_SETTINGS_USERS_RESTRICT_SIGNIN_LABEL},
      {"deviceOwnerLabel", IDS_SETTINGS_USERS_DEVICE_OWNER_LABEL},
      {"removeUserTooltip", IDS_SETTINGS_USERS_REMOVE_USER_TOOLTIP},
      {"addUsers", IDS_SETTINGS_USERS_ADD_USERS},
      {"addUsersEmail", IDS_SETTINGS_USERS_ADD_USERS_EMAIL},
      {"userExistsError", IDS_SETTINGS_USER_EXISTS_ERROR},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);
}

void AddParentalControlStrings(content::WebUIDataSource* html_source,
                               bool are_parental_control_settings_allowed,
                               SupervisedUserService* supervised_user_service) {
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
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  html_source->AddBoolean("showParentalControls",
                          are_parental_control_settings_allowed);

  bool is_child = user_manager::UserManager::Get()->IsLoggedInAsChildUser();
  html_source->AddBoolean("isChild", is_child);

  base::string16 tooltip;
  if (is_child) {
    std::string custodian = supervised_user_service->GetCustodianName();
    std::string second_custodian =
        supervised_user_service->GetSecondCustodianName();

    base::string16 child_managed_tooltip;
    if (second_custodian.empty()) {
      child_managed_tooltip = l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_MANAGED_BY_ONE_PARENT_TOOLTIP,
          base::UTF8ToUTF16(custodian));
    } else {
      child_managed_tooltip = l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_MANAGED_BY_TWO_PARENTS_TOOLTIP,
          base::UTF8ToUTF16(custodian), base::UTF8ToUTF16(second_custodian));
    }
    tooltip = child_managed_tooltip;
  }
  html_source->AddString("accountManagerPrimaryAccountChildManagedTooltip",
                         tooltip);
}

bool IsSameAccount(const AccountManager::AccountKey& account_key,
                   const AccountId& account_id) {
  switch (account_key.account_type) {
    case account_manager::AccountType::ACCOUNT_TYPE_GAIA:
      return account_id.GetAccountType() == AccountType::GOOGLE &&
             account_id.GetGaiaId() == account_key.id;
    case account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY:
      return account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY &&
             account_id.GetObjGuid() == account_key.id;
    case account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED:
      return false;
  }
}

}  // namespace

PeopleSection::PeopleSection(
    Profile* profile,
    SearchTagRegistry* search_tag_registry,
    syncer::SyncService* sync_service,
    SupervisedUserService* supervised_user_service,
    KerberosCredentialsManager* kerberos_credentials_manager,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service)
    : OsSettingsSection(profile, search_tag_registry),
      sync_service_(sync_service),
      supervised_user_service_(supervised_user_service),
      kerberos_credentials_manager_(kerberos_credentials_manager),
      identity_manager_(identity_manager),
      pref_service_(pref_service) {
  // No search tags are registered if in guest mode.
  if (features::IsGuestModeActive())
    return;

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetPeopleSearchConcepts());

  // TODO(jamescook): Sort out how account management is split between Chrome
  // OS and browser settings.
  if (IsAccountManagerAvailable(profile)) {
    // Some Account Manager search tags are added/removed dynamically.
    AccountManagerFactory* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    account_manager_ = factory->GetAccountManager(profile->GetPath().value());
    DCHECK(account_manager_);

    account_manager_->AddObserver(this);
    FetchAccounts();
  }

  if (kerberos_credentials_manager_) {
    // Kerberos search tags are added/removed dynamically.
    kerberos_credentials_manager_->AddObserver(this);
    OnKerberosEnabledStateChanged();
  }

  if (chromeos::features::IsSplitSettingsSyncEnabled()) {
    if (sync_service_) {
      updater.AddSearchTags(GetSplitSyncSearchConcepts());

      // Sync search tags are added/removed dynamically.
      sync_service_->AddObserver(this);
      OnStateChanged(sync_service_);
    }
  } else {
    updater.AddSearchTags(GetNonSplitSyncSearchConcepts());
  }

  // Parental control search tags are added if necessary and do not update
  // dynamically during a user session.
  if (features::ShouldShowParentalControlSettings(profile))
    updater.AddSearchTags(GetParentalSearchConcepts());

  // Fingerprint search tags are added if necessary. Remove fingerprint search
  // tags update dynamically during a user session.
  if (AreFingerprintSettingsAllowed()) {
    updater.AddSearchTags(GetFingerprintSearchConcepts());

    fingerprint_pref_change_registrar_.Init(pref_service_);
    fingerprint_pref_change_registrar_.Add(
        ::prefs::kQuickUnlockFingerprintRecord,
        base::BindRepeating(&PeopleSection::UpdateRemoveFingerprintSearchTags,
                            base::Unretained(this)));
    UpdateRemoveFingerprintSearchTags();
  }
}

PeopleSection::~PeopleSection() {
  if (kerberos_credentials_manager_)
    kerberos_credentials_manager_->RemoveObserver(this);

  if (chromeos::features::IsSplitSettingsSyncEnabled() && sync_service_)
    sync_service_->RemoveObserver(this);

  if (account_manager_)
    account_manager_->RemoveObserver(this);
}

void PeopleSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"osPeoplePageTitle", IDS_OS_SETTINGS_PEOPLE},
      {"accountManagerSubMenuLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_SUBMENU_LABEL},
      {"accountManagerPageTitle", IDS_SETTINGS_ACCOUNT_MANAGER_PAGE_TITLE},
      {"kerberosAccountsSubMenuLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_SUBMENU_LABEL},
      {"accountManagerPageTitle", IDS_SETTINGS_ACCOUNT_MANAGER_PAGE_TITLE},
      {"kerberosAccountsPageTitle", IDS_SETTINGS_KERBEROS_ACCOUNTS_PAGE_TITLE},
      {"lockScreenFingerprintTitle",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_SUBPAGE_TITLE},
      {"manageOtherPeople", IDS_SETTINGS_PEOPLE_MANAGE_OTHER_PEOPLE},
      {"osSyncPageTitle", IDS_OS_SETTINGS_SYNC_PAGE_TITLE},
      {"syncAndNonPersonalizedServices",
       IDS_SETTINGS_SYNC_SYNC_AND_NON_PERSONALIZED_SERVICES},
      {"syncDisconnectConfirm", IDS_SETTINGS_SYNC_DISCONNECT_CONFIRM},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  // Toggles the Chrome OS Account Manager submenu in the People section.
  html_source->AddBoolean("isAccountManagerEnabled",
                          account_manager_ != nullptr);

  if (chromeos::features::ShouldUseBrowserSyncConsent()) {
    static constexpr webui::LocalizedString kTurnOffStrings[] = {
        {"syncDisconnect", IDS_SETTINGS_PEOPLE_SYNC_TURN_OFF},
        {"syncDisconnectTitle",
         IDS_SETTINGS_TURN_OFF_SYNC_AND_SIGN_OUT_DIALOG_TITLE},
    };
    AddLocalizedStringsBulk(html_source, kTurnOffStrings);
  } else {
    static constexpr webui::LocalizedString kSignOutStrings[] = {
        {"syncDisconnect", IDS_SETTINGS_PEOPLE_SIGN_OUT},
        {"syncDisconnectTitle", IDS_SETTINGS_SYNC_DISCONNECT_TITLE},
    };
    AddLocalizedStringsBulk(html_source, kSignOutStrings);
  }

  std::string sync_dashboard_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kSyncGoogleDashboardURL),
          g_browser_process->GetApplicationLocale())
          .spec();

  html_source->AddString(
      "syncDisconnectExplanation",
      l10n_util::GetStringFUTF8(IDS_SETTINGS_SYNC_DISCONNECT_EXPLANATION,
                                base::ASCIIToUTF16(sync_dashboard_url)));

  html_source->AddBoolean(
      "secondaryGoogleAccountSigninAllowed",
      pref_service_->GetBoolean(
          chromeos::prefs::kSecondaryGoogleAccountSigninAllowed));

  html_source->AddBoolean(
      "driveSuggestAvailable",
      base::FeatureList::IsEnabled(omnibox::kDocumentProvider));

  AddAccountManagerPageStrings(html_source);
  AddKerberosAccountsPageStrings(html_source);
  AddKerberosAddAccountDialogStrings(html_source);
  AddLockScreenPageStrings(html_source, profile()->GetPrefs());
  AddFingerprintListStrings(html_source);
  AddFingerprintStrings(html_source, AreFingerprintSettingsAllowed());
  AddSetupFingerprintDialogStrings(html_source);
  AddSetupPinDialogStrings(html_source);
  AddSyncControlsStrings(html_source);
  AddUsersStrings(html_source);
  AddParentalControlStrings(
      html_source, features::ShouldShowParentalControlSettings(profile()),
      supervised_user_service_);

  ::settings::AddSyncControlsStrings(html_source);
  ::settings::AddSyncAccountControlStrings(html_source);
  ::settings::AddPasswordPromptDialogStrings(html_source);
  ::settings::AddSyncPageStrings(html_source);
}

void PeopleSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<::settings::PeopleHandler>(profile()));
  web_ui->AddMessageHandler(
      std::make_unique<::settings::ProfileInfoHandler>(profile()));

  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("profileLabel",
                                            IDS_OS_SETTINGS_PROFILE_LABEL);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  if (account_manager_) {
    web_ui->AddMessageHandler(
        std::make_unique<chromeos::settings::AccountManagerUIHandler>(
            account_manager_, identity_manager_));
  }

  if (chromeos::features::IsSplitSettingsSyncEnabled())
    web_ui->AddMessageHandler(std::make_unique<OSSyncHandler>(profile()));

  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::QuickUnlockHandler>());

  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::FingerprintHandler>(profile()));

  if (!profile()->IsGuestSession() &&
      features::ShouldShowParentalControlSettings(profile())) {
    web_ui->AddMessageHandler(
        std::make_unique<chromeos::settings::ParentalControlsHandler>(
            profile()));
  }

  std::unique_ptr<chromeos::settings::KerberosAccountsHandler>
      kerberos_accounts_handler =
          KerberosAccountsHandler::CreateIfKerberosEnabled(profile());
  if (kerberos_accounts_handler) {
    // Note that the UI is enabled only if Kerberos is enabled.
    web_ui->AddMessageHandler(std::move(kerberos_accounts_handler));
  }
}

int PeopleSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_PEOPLE;
}

mojom::Section PeopleSection::GetSection() const {
  return mojom::Section::kPeople;
}

mojom::SearchResultIcon PeopleSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kAvatar;
}

std::string PeopleSection::GetSectionPath() const {
  return mojom::kPeopleSectionPath;
}

bool PeopleSection::LogMetric(mojom::Setting setting,
                              base::Value& value) const {
  // Unimplemented.
  return false;
}

void PeopleSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kSetUpParentalControls);

  // My accounts.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_ACCOUNT_MANAGER_PAGE_TITLE, mojom::Subpage::kMyAccounts,
      mojom::SearchResultIcon::kAvatar, mojom::SearchResultDefaultRank::kMedium,
      mojom::kMyAccountsSubpagePath);
  static constexpr mojom::Setting kMyAccountsSettings[] = {
      mojom::Setting::kAddAccount,
      mojom::Setting::kRemoveAccount,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kMyAccounts, kMyAccountsSettings,
                            generator);

  // Combined browser/OS sync.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_SYNC_SYNC_AND_NON_PERSONALIZED_SERVICES,
      mojom::Subpage::kSyncDeprecated, mojom::SearchResultIcon::kSync,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kSyncDeprecatedSubpagePath);
  static constexpr mojom::Setting kSyncDeprecatedSettings[] = {
      mojom::Setting::kNonSplitSyncEncryptionOptions,
      mojom::Setting::kAutocompleteSearchesAndUrls,
      mojom::Setting::kMakeSearchesAndBrowsingBetter,
      mojom::Setting::kGoogleDriveSearchSuggestions,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kSyncDeprecated,
                            kSyncDeprecatedSettings, generator);
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_SYNC_ADVANCED_PAGE_TITLE,
      mojom::Subpage::kSyncDeprecatedAdvanced, mojom::Subpage::kSyncDeprecated,
      mojom::SearchResultIcon::kSync, mojom::SearchResultDefaultRank::kMedium,
      mojom::kSyncDeprecatedAdvancedSubpagePath);

  // OS sync.
  generator->RegisterTopLevelSubpage(
      IDS_OS_SETTINGS_SYNC_PAGE_TITLE, mojom::Subpage::kSync,
      mojom::SearchResultIcon::kSync, mojom::SearchResultDefaultRank::kMedium,
      mojom::kSyncSubpagePath);
  generator->RegisterNestedSetting(mojom::Setting::kSplitSyncOnOff,
                                   mojom::Subpage::kSync);

  // Security and sign-in.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE_LOGIN_LOCK,
      mojom::Subpage::kSecurityAndSignIn, mojom::SearchResultIcon::kLock,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kSecurityAndSignInSubpagePath);
  static constexpr mojom::Setting kSecurityAndSignInSettings[] = {
      mojom::Setting::kLockScreen,
      mojom::Setting::kChangeAuthPin,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kSecurityAndSignIn,
                            kSecurityAndSignInSettings, generator);

  // Fingerprint.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_SUBPAGE_TITLE,
      mojom::Subpage::kFingerprint, mojom::Subpage::kSecurityAndSignIn,
      mojom::SearchResultIcon::kFingerprint,
      mojom::SearchResultDefaultRank::kMedium, mojom::kFingerprintSubpagePath);
  static constexpr mojom::Setting kFingerprintSettings[] = {
      mojom::Setting::kAddFingerprint,
      mojom::Setting::kRemoveFingerprint,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kFingerprint, kFingerprintSettings,
                            generator);

  // Manage other people.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_PEOPLE_MANAGE_OTHER_PEOPLE,
                                     mojom::Subpage::kManageOtherPeople,
                                     mojom::SearchResultIcon::kAvatar,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kManageOtherPeopleSubpagePath);
  static constexpr mojom::Setting kManageOtherPeopleSettings[] = {
      mojom::Setting::kGuestBrowsing,
      mojom::Setting::kShowUsernamesAndPhotosAtSignIn,
      mojom::Setting::kRestrictSignIn,
      mojom::Setting::kAddToUserWhitelist,
      mojom::Setting::kRemoveFromUserWhitelist,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kManageOtherPeople,
                            kManageOtherPeopleSettings, generator);

  // Kerberos.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_KERBEROS_ACCOUNTS_PAGE_TITLE, mojom::Subpage::kKerberos,
      mojom::SearchResultIcon::kAvatar, mojom::SearchResultDefaultRank::kMedium,
      mojom::kKerberosSubpagePath);
  static constexpr mojom::Setting kKerberosSettings[] = {
      mojom::Setting::kAddKerberosTicket,
      mojom::Setting::kRemoveKerberosTicket,
      mojom::Setting::kSetActiveKerberosTicket,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kKerberos, kKerberosSettings,
                            generator);
}

void PeopleSection::FetchAccounts() {
  account_manager_->GetAccounts(
      base::BindOnce(&PeopleSection::UpdateAccountManagerSearchTags,
                     weak_factory_.GetWeakPtr()));
}

void PeopleSection::OnTokenUpserted(const AccountManager::Account& account) {
  FetchAccounts();
}

void PeopleSection::OnAccountRemoved(const AccountManager::Account& account) {
  FetchAccounts();
}

void PeopleSection::UpdateAccountManagerSearchTags(
    const std::vector<AccountManager::Account>& accounts) {
  DCHECK(IsAccountManagerAvailable(profile()));

  // Start with no Account Manager search tags.
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetRemoveAccountSearchConcepts());

  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile());
  DCHECK(user);

  for (const AccountManager::Account& account : accounts) {
    if (IsSameAccount(account.key, user->GetAccountId()))
      continue;

    // If a non-device account exists, add the "Remove Account" search tag.
    updater.AddSearchTags(GetRemoveAccountSearchConcepts());
    return;
  }
}

void PeopleSection::OnStateChanged(syncer::SyncService* sync_service) {
  DCHECK(chromeos::features::IsSplitSettingsSyncEnabled());
  DCHECK_EQ(sync_service, sync_service_);

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (sync_service_->IsEngineInitialized() &&
      sync_service_->GetUserSettings()->IsOsSyncFeatureEnabled()) {
    updater.AddSearchTags(GetSplitSyncOnSearchConcepts());
    updater.RemoveSearchTags(GetSplitSyncOffSearchConcepts());
  } else {
    updater.RemoveSearchTags(GetSplitSyncOnSearchConcepts());
    updater.AddSearchTags(GetSplitSyncOffSearchConcepts());
  }
}

void PeopleSection::OnKerberosEnabledStateChanged() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  if (kerberos_credentials_manager_->IsKerberosEnabled())
    updater.AddSearchTags(GetKerberosSearchConcepts());
  else
    updater.RemoveSearchTags(GetKerberosSearchConcepts());
}

void PeopleSection::AddKerberosAccountsPageStrings(
    content::WebUIDataSource* html_source) const {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"kerberosAccountsAddAccountLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_ADD_ACCOUNT_LABEL},
      {"kerberosAccountsRefreshNowLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_REFRESH_NOW_LABEL},
      {"kerberosAccountsSetAsActiveAccountLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_SET_AS_ACTIVE_ACCOUNT_LABEL},
      {"kerberosAccountsSignedOut", IDS_SETTINGS_KERBEROS_ACCOUNTS_SIGNED_OUT},
      {"kerberosAccountsListHeader",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_LIST_HEADER},
      {"kerberosAccountsRemoveAccountLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_REMOVE_ACCOUNT_LABEL},
      {"kerberosAccountsReauthenticationLabel",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_REAUTHENTICATION_LABEL},
      {"kerberosAccountsTicketActive",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_TICKET_ACTIVE},
      {"kerberosAccountsAccountRemovedTip",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_ACCOUNT_REMOVED_TIP},
      {"kerberosAccountsAccountRefreshedTip",
       IDS_SETTINGS_KERBEROS_ACCOUNTS_ACCOUNT_REFRESHED_TIP},
      {"kerberosAccountsSignedIn", IDS_SETTINGS_KERBEROS_ACCOUNTS_SIGNED_IN},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  // Toggles the Chrome OS Kerberos Accounts submenu in the People section.
  // Note that the handler is also dependent on this pref.
  html_source->AddBoolean(
      "isKerberosEnabled",
      kerberos_credentials_manager_ != nullptr &&
          kerberos_credentials_manager_->IsKerberosEnabled());

  PrefService* local_state = g_browser_process->local_state();

  // Whether new Kerberos accounts may be added.
  html_source->AddBoolean(
      "kerberosAddAccountsAllowed",
      local_state->GetBoolean(::prefs::kKerberosAddAccountsAllowed));

  // Kerberos accounts page with "Learn more" link.
  html_source->AddString(
      "kerberosAccountsDescription",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_KERBEROS_ACCOUNTS_DESCRIPTION,
          GetHelpUrlWithBoard(chrome::kKerberosAccountsLearnMoreURL)));
}

bool PeopleSection::AreFingerprintSettingsAllowed() {
  return chromeos::quick_unlock::IsFingerprintEnabled(profile());
}

void PeopleSection::UpdateRemoveFingerprintSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetRemoveFingerprintSearchConcepts());

  // "Remove fingerprint" search tag should exist only when 1 or more
  // fingerprints are registered.
  int registered_fingerprint_count =
      pref_service_->GetInteger(::prefs::kQuickUnlockFingerprintRecord);
  if (registered_fingerprint_count > 0) {
    updater.AddSearchTags(GetRemoveFingerprintSearchConcepts());
  }
}

}  // namespace settings
}  // namespace chromeos
