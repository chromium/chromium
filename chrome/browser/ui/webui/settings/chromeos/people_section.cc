// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/people_section.h"

#include "ash/components/account_manager/account_manager_factory.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_features.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/account_manager_facade_factory.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/chromeos/sync/os_sync_handler.h"
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
#include "components/account_manager_core/account_manager_facade.h"
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
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"

namespace chromeos {
namespace settings {
namespace {

using ::ash::IsAccountManagerAvailable;

const std::vector<SearchConcept>& GetPeopleSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags([] {
    std::vector<SearchConcept> all_tags({
        {IDS_OS_SETTINGS_TAG_PEOPLE_ACCOUNTS,
         mojom::kMyAccountsSubpagePath,
         mojom::SearchResultIcon::kAvatar,
         mojom::SearchResultDefaultRank::kMedium,
         mojom::SearchResultType::kSubpage,
         {.subpage = mojom::Subpage::kMyAccounts}},
    });

    if (chromeos::features::IsAccountManagementFlowsV2Enabled()) {
      all_tags.insert(all_tags.end(),
                      {{IDS_OS_SETTINGS_TAG_PEOPLE_V2,
                        mojom::kPeopleSectionPath,
                        mojom::SearchResultIcon::kAvatar,
                        mojom::SearchResultDefaultRank::kMedium,
                        mojom::SearchResultType::kSection,
                        {.section = mojom::Section::kPeople}},
                       {IDS_OS_SETTINGS_TAG_PEOPLE_ACCOUNTS_ADD_V2,
                        mojom::kMyAccountsSubpagePath,
                        mojom::SearchResultIcon::kAvatar,
                        mojom::SearchResultDefaultRank::kMedium,
                        mojom::SearchResultType::kSetting,
                        {.setting = mojom::Setting::kAddAccount}}});
    } else {
      all_tags.insert(
          all_tags.end(),
          {
              {IDS_OS_SETTINGS_TAG_PEOPLE,
               mojom::kPeopleSectionPath,
               mojom::SearchResultIcon::kAvatar,
               mojom::SearchResultDefaultRank::kMedium,
               mojom::SearchResultType::kSection,
               {.section = mojom::Section::kPeople}},
              {IDS_OS_SETTINGS_TAG_PEOPLE_ACCOUNTS_ADD,
               mojom::kMyAccountsSubpagePath,
               mojom::SearchResultIcon::kAvatar,
               mojom::SearchResultDefaultRank::kMedium,
               mojom::SearchResultType::kSetting,
               {.setting = mojom::Setting::kAddAccount}},
              {IDS_OS_SETTINGS_TAG_USERNAMES_AND_PHOTOS,
               mojom::kManageOtherPeopleSubpagePath,
               mojom::SearchResultIcon::kAvatar,
               mojom::SearchResultDefaultRank::kMedium,
               mojom::SearchResultType::kSetting,
               {.setting = mojom::Setting::kShowUsernamesAndPhotosAtSignIn},
               {IDS_OS_SETTINGS_TAG_USERNAMES_AND_PHOTOS_ALT1,
                IDS_OS_SETTINGS_TAG_USERNAMES_AND_PHOTOS_ALT2,
                SearchConcept::kAltTagEnd}},
              {IDS_OS_SETTINGS_TAG_RESTRICT_SIGN_IN_REMOVE,
               mojom::kManageOtherPeopleSubpagePath,
               mojom::SearchResultIcon::kAvatar,
               mojom::SearchResultDefaultRank::kMedium,
               mojom::SearchResultType::kSetting,
               {.setting = mojom::Setting::kRemoveFromUserAllowlist}},
              {IDS_OS_SETTINGS_TAG_GUEST_BROWSING,
               mojom::kManageOtherPeopleSubpagePath,
               mojom::SearchResultIcon::kAvatar,
               mojom::SearchResultDefaultRank::kMedium,
               mojom::SearchResultType::kSetting,
               {.setting = mojom::Setting::kGuestBrowsing}},
              {IDS_OS_SETTINGS_TAG_RESTRICT_SIGN_IN,
               mojom::kManageOtherPeopleSubpagePath,
               mojom::SearchResultIcon::kAvatar,
               mojom::SearchResultDefaultRank::kMedium,
               mojom::SearchResultType::kSetting,
               {.setting = mojom::Setting::kRestrictSignIn},
               {IDS_OS_SETTINGS_TAG_RESTRICT_SIGN_IN_ALT1,
                SearchConcept::kAltTagEnd}},
              {IDS_OS_SETTINGS_TAG_RESTRICT_SIGN_IN_ADD,
               mojom::kManageOtherPeopleSubpagePath,
               mojom::SearchResultIcon::kAvatar,
               mojom::SearchResultDefaultRank::kMedium,
               mojom::SearchResultType::kSetting,
               {.setting = mojom::Setting::kAddToUserAllowlist}},
              {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_PIN_OR_PASSWORD,
               mojom::kSecurityAndSignInSubpagePath,
               mojom::SearchResultIcon::kLock,
               mojom::SearchResultDefaultRank::kMedium,
               mojom::SearchResultType::kSetting,
               {.setting = mojom::Setting::kChangeAuthPin},
               {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_PIN_OR_PASSWORD_ALT1,
                SearchConcept::kAltTagEnd}},
              {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_WHEN_WAKING,
               mojom::kSecurityAndSignInSubpagePath,
               mojom::SearchResultIcon::kLock,
               mojom::SearchResultDefaultRank::kMedium,
               mojom::SearchResultType::kSetting,
               {.setting = mojom::Setting::kLockScreen},
               {IDS_OS_SETTINGS_TAG_LOCK_SCREEN_WHEN_WAKING_ALT1,
                SearchConcept::kAltTagEnd}},
              {IDS_OS_SETTINGS_TAG_LOCK_SCREEN,
               mojom::kSecurityAndSignInSubpagePath,
               mojom::SearchResultIcon::kLock,
               mojom::SearchResultDefaultRank::kMedium,
               mojom::SearchResultType::kSubpage,
               {.subpage = mojom::Subpage::kSecurityAndSignIn}},
          });
    }
    return all_tags;
  }());

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
       mojom::kKerberosAccountsSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kAddKerberosTicket}},
      {IDS_OS_SETTINGS_TAG_KERBEROS_REMOVE,
       mojom::kKerberosAccountsSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRemoveKerberosTicket}},
      {IDS_OS_SETTINGS_TAG_KERBEROS,
       mojom::kKerberosAccountsSubpagePath,
       mojom::SearchResultIcon::kAvatar,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kKerberosAccounts}},
      {IDS_OS_SETTINGS_TAG_KERBEROS_ACTIVE,
       mojom::kKerberosAccountsSubpagePath,
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

void AddAccountManagerPageStrings(content::WebUIDataSource* html_source,
                                  Profile* profile) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"accountManagerChildFirstMessage",
       IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_FIRST_MESSAGE},
      {"accountManagerChildSecondMessage",
       IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_SECOND_MESSAGE},
      {"accountManagerPrimaryAccountTooltip",
       IDS_SETTINGS_ACCOUNT_MANAGER_PRIMARY_ACCOUNT_TOOLTIP},
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
      {"accountManagerManagedLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_MANAGEMENT_STATUS_MANAGED_ACCOUNT},
      {"accountManagerUnmanagedLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_MANAGEMENT_STATUS_UNMANAGED_ACCOUNT},
      {"accountListDescription", IDS_SETTINGS_ACCOUNT_MANAGER_LIST_DESCRIPTION},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

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
                         base::UTF8ToUTF16(chrome::kChromeUIManagementURL));

  if (chromeos::features::IsAccountManagementFlowsV2Enabled()) {
    static constexpr webui::LocalizedString kLocalizedStringsV2[] = {
        {"addAccountLabel", IDS_SETTINGS_ACCOUNT_MANAGER_ADD_ACCOUNT_LABEL_V2},
        {"accountListHeader", IDS_SETTINGS_ACCOUNT_MANAGER_LIST_HEADER_V2},
        {"accountListHeaderChild",
         IDS_SETTINGS_ACCOUNT_MANAGER_LIST_HEADER_CHILD},
        {"accountManagerDescription",
         IDS_SETTINGS_ACCOUNT_MANAGER_DESCRIPTION_V2},
        {"accountManagerChildDescription",
         IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_DESCRIPTION_V2},
        {"accountManagerSecondaryAccountsDisabledText",
         IDS_SETTINGS_ACCOUNT_MANAGER_SECONDARY_ACCOUNTS_DISABLED_TEXT_V2},
    };
    html_source->AddLocalizedStrings(kLocalizedStringsV2);
  } else {
    static constexpr webui::LocalizedString kLocalizedStringsV1[] = {
        {"accountManagerDescription", IDS_SETTINGS_ACCOUNT_MANAGER_DESCRIPTION},
        {"accountManagerChildDescription",
         IDS_SETTINGS_ACCOUNT_MANAGER_CHILD_DESCRIPTION},
        {"addAccountLabel", IDS_SETTINGS_ACCOUNT_MANAGER_ADD_ACCOUNT_LABEL},
        {"accountListHeader", IDS_SETTINGS_ACCOUNT_MANAGER_LIST_HEADER},
        {"accountManagerSecondaryAccountsDisabledText",
         IDS_SETTINGS_ACCOUNT_MANAGER_SECONDARY_ACCOUNTS_DISABLED_TEXT},
    };
    html_source->AddLocalizedStrings(kLocalizedStringsV1);
  }
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
  html_source->AddLocalizedStrings(kLocalizedStrings);

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

  html_source->AddString("lockScreenFingerprintNotice",
                         l10n_util::GetStringFUTF16(
                             IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_NOTICE,
                             ui::GetChromeOSDeviceName()));
  html_source->AddString("fingerprintLearnMoreLink",
                         chrome::kFingerprintLearnMoreURL);

  if (chromeos::features::IsAccountManagementFlowsV2Enabled()) {
    html_source->AddLocalizedString(
        "lockScreenTitleLoginLock",
        IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE_LOGIN_LOCK_V2);
  } else {
    html_source->AddLocalizedString(
        "lockScreenTitleLoginLock",
        IDS_SETTINGS_PEOPLE_LOCK_SCREEN_TITLE_LOGIN_LOCK);
  }
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
  if (are_fingerprint_settings_allowed) {
    chromeos::quick_unlock::AddFingerprintResources(html_source);
  }

  int instruction_id, aria_label_id;
  bool aria_label_includes_device = false;
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
    case quick_unlock::FingerprintLocation::RIGHT_SIDE:
      instruction_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_RIGHT_SIDE_ARIA_LABEL;
      aria_label_includes_device = true;
      break;
    case quick_unlock::FingerprintLocation::LEFT_SIDE:
      instruction_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_LEFT_SIDE_ARIA_LABEL;
      aria_label_includes_device = true;
      break;
    case FingerprintLocation::UNKNOWN:
      instruction_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD;
      break;
  }
  html_source->AddLocalizedString(
      "configureFingerprintInstructionLocateScannerStep", instruction_id);
  if (aria_label_includes_device) {
    html_source->AddString(
        "configureFingerprintScannerStepAriaLabel",
        l10n_util::GetStringFUTF16(aria_label_id, ui::GetChromeOSDeviceName()));
  } else {
    html_source->AddLocalizedString("configureFingerprintScannerStepAriaLabel",
                                    aria_label_id);
  }
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
  html_source->AddLocalizedStrings(kLocalizedStrings);

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
  html_source->AddLocalizedStrings(kLocalizedStrings);
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
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("showParentalControls",
                          are_parental_control_settings_allowed);

  bool is_child = user_manager::UserManager::Get()->IsLoggedInAsChildUser();
  html_source->AddBoolean("isChild", is_child);

  std::u16string tooltip;
  if (is_child) {
    std::string custodian = supervised_user_service->GetCustodianName();
    std::string second_custodian =
        supervised_user_service->GetSecondCustodianName();

    std::u16string child_managed_tooltip;
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

bool IsSameAccount(const ::account_manager::AccountKey& account_key,
                   const AccountId& account_id) {
  switch (account_key.account_type) {
    case account_manager::AccountType::kGaia:
      return account_id.GetAccountType() == AccountType::GOOGLE &&
             account_id.GetGaiaId() == account_key.id;
    case account_manager::AccountType::kActiveDirectory:
      return account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY &&
             account_id.GetObjGuid() == account_key.id;
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
    auto* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    account_manager_ = factory->GetAccountManager(profile->GetPath().value());
    DCHECK(account_manager_);
    account_manager_facade_ =
        ::GetAccountManagerFacade(profile->GetPath().value());
    DCHECK(account_manager_facade_);
    account_manager_facade_observation_.Observe(account_manager_facade_);
    FetchAccounts();
  }

  // No Kerberos search tags are registered here if Kerberos settings are in a
  // separate section.
  if (kerberos_credentials_manager_ &&
      !chromeos::features::IsKerberosSettingsSectionEnabled()) {
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
  if (AreFingerprintSettingsAllowed() &&
      !chromeos::features::IsAccountManagementFlowsV2Enabled()) {
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
  if (kerberos_credentials_manager_ &&
      !chromeos::features::IsKerberosSettingsSectionEnabled()) {
    kerberos_credentials_manager_->RemoveObserver(this);
  }

  if (chromeos::features::IsSplitSettingsSyncEnabled() && sync_service_)
    sync_service_->RemoveObserver(this);
}

void PeopleSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"osPeoplePageTitle", IDS_OS_SETTINGS_PEOPLE},
      {"accountManagerSubMenuLabel",
       IDS_SETTINGS_ACCOUNT_MANAGER_SUBMENU_LABEL},
      {"accountManagerPageTitle", IDS_SETTINGS_ACCOUNT_MANAGER_PAGE_TITLE},
      {"accountManagerPageTitle", IDS_SETTINGS_ACCOUNT_MANAGER_PAGE_TITLE},
      {"lockScreenFingerprintTitle",
       IDS_SETTINGS_PEOPLE_LOCK_SCREEN_FINGERPRINT_SUBPAGE_TITLE},
      {"manageOtherPeople", IDS_SETTINGS_PEOPLE_MANAGE_OTHER_PEOPLE},
      {"osSyncPageTitle", IDS_OS_SETTINGS_SYNC_PAGE_TITLE},
      {"syncAndNonPersonalizedServices",
       IDS_SETTINGS_SYNC_SYNC_AND_NON_PERSONALIZED_SERVICES},
      {"syncDisconnectConfirm", IDS_SETTINGS_SYNC_DISCONNECT_CONFIRM},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile());
  DCHECK(user);

  if (chromeos::features::IsAccountManagementFlowsV2Enabled()) {
    html_source->AddLocalizedString("osPeoplePageTitle",
                                    IDS_OS_SETTINGS_PEOPLE_V2);

    // This string is not used if the flag is disabled.
    html_source->AddString("osProfileName", l10n_util::GetStringFUTF16(
                                                IDS_OS_SETTINGS_PROFILE_NAME,
                                                user->GetGivenName()));
    html_source->AddString(
        "accountManagerPageTitle",
        l10n_util::GetStringFUTF16(IDS_SETTINGS_ACCOUNT_MANAGER_PAGE_TITLE_V2,
                                   user->GetGivenName()));
  } else {
    static constexpr webui::LocalizedString kAccountManagerStrings[] = {
        {"osPeoplePageTitle", IDS_OS_SETTINGS_PEOPLE},
        {"accountManagerPageTitle", IDS_SETTINGS_ACCOUNT_MANAGER_PAGE_TITLE},
    };
    html_source->AddLocalizedStrings(kAccountManagerStrings);
  }

  // Toggles the Chrome OS Account Manager submenu in the People section.
  html_source->AddBoolean("isAccountManagerEnabled",
                          account_manager_facade_ != nullptr);
  html_source->AddBoolean(
      "isAccountManagementFlowsV2Enabled",
      chromeos::features::IsAccountManagementFlowsV2Enabled());
  html_source->AddBoolean(
      "isDeviceAccountManaged",
      user->IsActiveDirectoryUser() ||
          profile()->GetProfilePolicyConnector()->IsManaged());

  if (chromeos::features::ShouldUseBrowserSyncConsent()) {
    static constexpr webui::LocalizedString kTurnOffStrings[] = {
        {"syncDisconnect", IDS_SETTINGS_PEOPLE_SYNC_TURN_OFF},
        {"syncDisconnectTitle",
         IDS_SETTINGS_TURN_OFF_SYNC_AND_SIGN_OUT_DIALOG_TITLE},
    };
    html_source->AddLocalizedStrings(kTurnOffStrings);
  } else {
    static constexpr webui::LocalizedString kSignOutStrings[] = {
        {"syncDisconnect", IDS_SETTINGS_PEOPLE_SIGN_OUT},
        {"syncDisconnectTitle", IDS_SETTINGS_SYNC_DISCONNECT_TITLE},
    };
    html_source->AddLocalizedStrings(kSignOutStrings);
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

  AddAccountManagerPageStrings(html_source, profile());
  KerberosAccountsHandler::AddLoadTimeKerberosStrings(
      html_source, kerberos_credentials_manager_);
  AddLockScreenPageStrings(html_source, profile()->GetPrefs());
  AddFingerprintListStrings(html_source);
  AddFingerprintResources(html_source, AreFingerprintSettingsAllowed());
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

  if (account_manager_facade_) {
    web_ui->AddMessageHandler(
        std::make_unique<chromeos::settings::AccountManagerUIHandler>(
            account_manager_, account_manager_facade_, identity_manager_));
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

  // No Kerberos handler is created/added here if Kerberos settings are in a
  // separate section.
  if (!chromeos::features::IsKerberosSettingsSectionEnabled()) {
    std::unique_ptr<chromeos::settings::KerberosAccountsHandler>
        kerberos_accounts_handler =
            KerberosAccountsHandler::CreateIfKerberosEnabled(profile());
    if (kerberos_accounts_handler) {
      // Note that the UI is enabled only if Kerberos is enabled.
      web_ui->AddMessageHandler(std::move(kerberos_accounts_handler));
    }
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
      mojom::Setting::kAddToUserAllowlist,
      mojom::Setting::kRemoveFromUserAllowlist,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kManageOtherPeople,
                            kManageOtherPeopleSettings, generator);

  // Kerberos.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_KERBEROS_ACCOUNTS_PAGE_TITLE,
                                     mojom::Subpage::kKerberosAccounts,
                                     mojom::SearchResultIcon::kAvatar,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kKerberosAccountsSubpagePath);
  static constexpr mojom::Setting kKerberosAccountsSettings[] = {
      mojom::Setting::kAddKerberosTicket,
      mojom::Setting::kRemoveKerberosTicket,
      mojom::Setting::kSetActiveKerberosTicket,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kKerberosAccounts,
                            kKerberosAccountsSettings, generator);
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

void PeopleSection::UpdateAccountManagerSearchTags(
    const std::vector<::account_manager::Account>& accounts) {
  DCHECK(IsAccountManagerAvailable(profile()));

  // Start with no Account Manager search tags.
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.RemoveSearchTags(GetRemoveAccountSearchConcepts());

  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile());
  DCHECK(user);

  for (const ::account_manager::Account& account : accounts) {
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
