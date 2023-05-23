// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"

#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extension_control_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/password_manager/sync_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/password_manager_resources.h"
#include "chrome/grit/password_manager_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/grit/components_scaled_resources.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/webui/settings/settings_security_key_handler.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/webui/password_manager/promo_card.h"
#include "chrome/browser/ui/webui/password_manager/promo_cards_handler.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#endif

#if !BUILDFLAG(OPTIMIZE_WEBUI)
#include "chrome/grit/settings_shared_resources.h"
#include "chrome/grit/settings_shared_resources_map.h"
#endif

namespace {

std::u16string InsertBrandedPasswordManager(int message_id) {
  return l10n_util::GetStringFUTF16(
      message_id,
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE));
}

content::WebUIDataSource* CreateAndAddPasswordsUIHTMLSource(
    Profile* profile,
    content::WebUI* web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, password_manager::kChromeUIPasswordManagerHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kPasswordManagerResources, kPasswordManagerResourcesSize),
      IDR_PASSWORD_MANAGER_PASSWORD_MANAGER_HTML);

#if !BUILDFLAG(OPTIMIZE_WEBUI)
  source->AddResourcePaths(
      base::make_span(kSettingsSharedResources, kSettingsSharedResourcesSize));
#endif

  static constexpr webui::LocalizedString kStrings[] = {
    {"accountStorageToggleLabel",
     IDS_PASSWORD_MANAGER_UI_ACCOUNT_STORAGE_TOGGLE_LABEL},
    {"addPassword", IDS_PASSWORD_MANAGER_UI_ADD_PASSWORD_BUTTON},
    {"addPasswordFooter", IDS_PASSWORD_MANAGER_UI_ADD_PASSWORD_FOOTNOTE},
    {"addPasswordStoreOptionAccount",
     IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_SAVE_TO_ACCOUNT},
    {"addPasswordStoreOptionDevice",
     IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_SAVE_TO_DEVICE},
    {"addPasswordStorePickerA11yDescription",
     IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_ACCESSIBLE_NAME},
    {"addPasswordTitle", IDS_PASSWORD_MANAGER_UI_ADD_PASSWORD},
    {"addShortcut", IDS_PASSWORD_MANAGER_UI_ADD_SHORTCUT_TITLE},
    {"alreadyChangedPasswordLink",
     IDS_PASSWORD_MANAGER_UI_ALREADY_CHANGED_PASSWORD},
    {"appsLabel", IDS_PASSWORD_MANAGER_UI_APPS_LABEL},
    {"authTimedOut", IDS_PASSWORD_MANAGER_UI_AUTH_TIMED_OUT},
    {"autosigninLabel", IDS_PASSWORD_MANAGER_UI_AUTOSIGNIN_TOGGLE_LABEL},
    {"backToCheckup", IDS_PASSWORD_MANAGER_UI_BACK_TO_CHECKUP_ARIA_DESCRIPTION},
    {"backToPasswords",
     IDS_PASSWORD_MANAGER_UI_BACK_TO_PASSWORDS_ARIA_DESCRIPTION},
    {"blockedSitesDescription",
     IDS_PASSWORD_MANAGER_UI_BLOCKED_SITES_DESCRIPTION},
    {"blockedSitesTitle", IDS_PASSWORD_MANAGER_UI_BLOCKED_SITES_TITLE},
    {"cancel", IDS_CANCEL},
    {"changePassword", IDS_PASSWORD_MANAGER_UI_CHANGE_PASSWORD_BUTTON},
    {"changePasswordAriaDescription",
     IDS_PASSWORD_MANAGER_UI_CHANGE_PASSWORD_BUTTON_ARIA_DESCRIPTION},
    {"changePasswordInApp", IDS_PASSWORD_MANAGER_UI_CHANGE_PASSWORD_IN_APP},
    {"checkup", IDS_PASSWORD_MANAGER_UI_CHECKUP},
    {"checkupCanceled", IDS_PASSWORD_MANAGER_UI_CHECKUP_CANCELED},
    {"checkupErrorGeneric", IDS_PASSWORD_MANAGER_UI_CHECKUP_OTHER_ERROR},
    {"checkupErrorNoPasswords", IDS_PASSWORD_MANAGER_UI_CHECKUP_NO_PASSWORDS},
    {"checkupErrorOffline", IDS_PASSWORD_MANAGER_UI_CHECKUP_OFFLINE},
    {"checkupErrorQuota", IDS_PASSWORD_MANAGER_UI_CHECKUP_QUOTA_LIMIT},
    {"checkupErrorSignedOut", IDS_PASSWORD_MANAGER_UI_CHECKUP_SIGNED_OUT},
    {"checkupResultGreen", IDS_PASSWORD_MANAGER_UI_CHECKUP_GREEN_STATE_A11Y},
    {"checkupResultRed", IDS_PASSWORD_MANAGER_UI_CHECKUP_RED_STATE_A11Y},
    {"checkupResultYellow", IDS_PASSWORD_MANAGER_UI_CHECKUP_YELLOW_STATE_A11Y},
    {"compromisedRowWithError",
     IDS_PASSWORD_MANAGER_UI_CHECKUP_COMPROMISED_SECTION},
    {"checkupProgress", IDS_PASSWORD_MANAGER_UI_CHECKUP_PROGRESS},
    {"checkupTitle", IDS_PASSWORD_MANAGER_UI_CHECKUP_TITLE},
    {"clearSearch", IDS_CLEAR_SEARCH},
    {"close", IDS_CLOSE},
    {"compromisedPasswordsDescription",
     IDS_PASSWORD_MANAGER_UI_COMPROMISED_PASSWORDS_DESCRIPTION},
    {"compromisedPasswordsEmpty",
     IDS_PASSWORD_MANAGER_UI_NO_COMPROMISED_PASSWORDS},
    {"compromisedPasswordsTitle",
     IDS_PASSWORD_MANAGER_UI_HAS_COMPROMISED_PASSWORDS},
    {"controlledByExtension", IDS_SETTINGS_CONTROLLED_BY_EXTENSION},
    {"copyPassword", IDS_PASSWORD_MANAGER_UI_COPY_PASSWORD},
    {"copyUsername", IDS_PASSWORD_MANAGER_UI_COPY_USERNAME},
    {"deletePassword", IDS_DELETE},
    {"deletePasswordConfirmationDescription",
     IDS_PASSWORD_MANAGER_UI_DELETE_PASSWORD_CONFIRMATION_DESCRIPTION},
    {"deletePasswordConfirmationTitle",
     IDS_PASSWORD_MANAGER_UI_DELETE_PASSWORD_CONFIRMATION_TITLE},
    {"deletePasswordDialogDevice",
     IDS_PASSWORD_MANAGER_UI_DELETE_DIALOG_FROM_DEVICE_CHECKBOX_LABEL},
    {"deletePasswordDialogBody", IDS_PASSWORD_MANAGER_UI_DELETE_DIALOG_BODY},
    {"deletePasswordDialogAccount",
     IDS_PASSWORD_MANAGER_UI_DELETE_DIALOG_FROM_ACCOUNT_CHECKBOX_LABEL},
    {"deletePasswordDialogTitle", IDS_PASSWORD_MANAGER_UI_DELETE_DIALOG_TITLE},
    {"disable", IDS_DISABLE},
    {"downloadFile", IDS_PASSWORD_MANAGER_UI_DOWNLOAD_FILE},
    {"downloadLinkShow", IDS_DOWNLOAD_LINK_SHOW},
    {"editDisclaimerDescription",
     IDS_PASSWORD_MANAGER_UI_EDIT_DISCLAIMER_DESCRIPTION},
    {"editDisclaimerTitle", IDS_PASSWORD_MANAGER_UI_EDIT_DISCLAIMER_TITLE},
    {"editPassword", IDS_EDIT},
    {"editPasswordFootnote", IDS_PASSWORD_MANAGER_UI_PASSWORD_EDIT_FOOTNOTE},
    {"editPasswordTitle", IDS_PASSWORD_MANAGER_UI_EDIT_PASSWORD},
    {"emptyNote", IDS_PASSWORD_MANAGER_UI_NO_NOTE_ADDED},
    {"emptyStateImportSyncing",
     IDS_PASSWORD_MANAGER_UI_EMPTY_STATE_SYNCING_USERS},
    {"exportPasswords", IDS_PASSWORD_MANAGER_UI_EXPORT_TITLE},
    {"exportPasswordsDescription",
     IDS_PASSWORD_MANAGER_UI_EXPORT_BANNER_DESCRIPTION},
    {"exportPasswordsFailTips", IDS_PASSWORD_MANAGER_UI_EXPORTING_FAILURE_TIPS},
    {"exportPasswordsFailTipsAnotherFolder",
     IDS_PASSWORD_MANAGER_UI_EXPORTING_FAILURE_TIP_ANOTHER_FOLDER},
    {"exportPasswordsFailTipsEnoughSpace",
     IDS_PASSWORD_MANAGER_UI_EXPORTING_FAILURE_TIP_ENOUGH_SPACE},
    {"exportPasswordsFailTitle",
     IDS_PASSWORD_MANAGER_UI_EXPORTING_FAILURE_TITLE},
    {"exportPasswordsTryAgain", IDS_PASSWORD_MANAGER_UI_EXPORT_TRY_AGAIN},
    {"exportSuccessful", IDS_PASSWORD_MANAGER_UI_EXPORT_SUCCESSFUL},
    {"federationLabel", IDS_PASSWORD_MANAGER_UI_FEDERATION_LABEL},
    {"gotIt", IDS_SETTINGS_GOT_IT},
    {"help", IDS_PASSWORD_MANAGER_UI_HELP},
    {"hidePassword", IDS_PASSWORD_MANAGER_UI_HIDE_PASSWORD},
    {"hidePasswordA11yLabel", IDS_PASSWORD_MANAGER_UI_HIDE_PASSWORD_A11Y},
    {"importPasswords", IDS_PASSWORD_MANAGER_UI_IMPORT_BANNER_TITLE},
    {"importPasswordsCancel", IDS_PASSWORD_MANAGER_UI_IMPORT_CANCEL},
    {"importPasswordsSkip", IDS_PASSWORD_MANAGER_UI_IMPORT_SKIP},
    {"importPasswordsReplace", IDS_PASSWORD_MANAGER_UI_IMPORT_REPLACE},
    {"importPasswordsAlreadyActive",
     IDS_PASSWORD_MANAGER_UI_IMPORT_ALREADY_ACTIVE},
    {"importPasswordsFileSizeExceeded",
     IDS_PASSWORD_MANAGER_UI_IMPORT_FILE_SIZE_EXCEEDED},
    {"importPasswordsUnknownError",
     IDS_PASSWORD_MANAGER_UI_IMPORT_ERROR_UNKNOWN},
    {"importPasswordsBadFormatError",
     IDS_PASSWORD_MANAGER_UI_IMPORT_ERROR_BAD_FORMAT},
    {"importPasswordsErrorTitle", IDS_PASSWORD_MANAGER_UI_IMPORT_ERROR_TITLE},
    {"importPasswordsMissingPassword",
     IDS_PASSWORD_MANAGER_UI_IMPORT_MISSING_PASSWORD},
    {"importPasswordsMissingURL", IDS_PASSWORD_MANAGER_UI_IMPORT_MISSING_URL},
    {"importPasswordsInvalidURL", IDS_PASSWORD_MANAGER_UI_IMPORT_INVALID_URL},
    {"importPasswordsLongURL", IDS_PASSWORD_MANAGER_UI_IMPORT_LONG_URL},
    {"importPasswordsLongPassword",
     IDS_PASSWORD_MANAGER_UI_IMPORT_LONG_PASSWORD},
    {"importPasswordsLongUsername",
     IDS_PASSWORD_MANAGER_UI_IMPORT_LONG_USERNAME},
    {"importPasswordsLongNote", IDS_PASSWORD_MANAGER_UI_IMPORT_LONG_NOTE},
    {"importPasswordsConflictDevice",
     IDS_PASSWORD_MANAGER_UI_IMPORT_CONFLICT_DEVICE},
    {"importPasswordsConflictAccount",
     IDS_PASSWORD_MANAGER_UI_IMPORT_CONFLICT_ACCOUNT},
    {"importPasswordsCompleteTitle",
     IDS_PASSWORD_MANAGER_UI_IMPORT_COMPLETE_TITLE},
    {"importPasswordsSuccessTitle",
     IDS_PASSWORD_MANAGER_UI_IMPORT_SUCCESS_TITLE},
    {"importPasswordsSuccessTip", IDS_PASSWORD_MANAGER_UI_IMPORT_SUCCESS_TIP},
    {"importPasswordsDeleteFileOption",
     IDS_PASSWORD_MANAGER_UI_IMPORT_DELETE_FILE_OPTION},
    {"importPasswordsDescriptionAccount",
     IDS_PASSWORD_MANAGER_UI_IMPORT_DESCRIPTION_SYNCING_USERS},
    {"importPasswordsSelectFile",
     IDS_PASSWORD_MANAGER_UI_IMPORT_SELECT_FILE_DESCRIPTION},
    {"importPasswordsStorePickerA11yDescription",
     IDS_PASSWORD_MANAGER_UI_IMPORT_STORE_PICKER_ACCESSIBLE_NAME},
    {"passwordsStoreOptionAccount",
     IDS_PASSWORD_MANAGER_UI_STORE_PICKER_OPTION_ACCOUNT},
    {"justNow", IDS_PASSWORD_MANAGER_UI_JUST_NOW},
    {"leakedPassword", IDS_PASSWORD_MANAGER_UI_PASSWORD_LEAKED},
    {"localPasswordManager",
     IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE},
    {"manage", IDS_SETTINGS_MANAGE},
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    {"managePasskeysLabel", IDS_PASSWORD_MANAGER_UI_MANAGE_PASSKEYS_LABEL},
#endif
    {"menu", IDS_MENU},
    {"menuButtonLabel", IDS_SETTINGS_MENU_BUTTON_LABEL},
    {"missingTLD", IDS_PASSWORD_MANAGER_UI_MISSING_TLD},
    {"moreActions", IDS_PASSWORD_MANAGER_UI_MORE_ACTIONS},
    {"moreActionsAriaDescription",
     IDS_PASSWORD_MANAGER_UI_MORE_ACTIONS_ARIA_DESCRIPTION},
    {"movePasswordsButton", IDS_PASSWORD_MANAGER_UI_MOVE_PASSWORDS_BUTTON},
    {"movePasswordsDescription",
     IDS_PASSWORD_MANAGER_UI_MOVE_PASSWORDS_DESCRIPTION},
    {"movePasswordsTitle", IDS_PASSWORD_MANAGER_UI_MOVE_PASSWORDS_TITLE},
    {"muteCompromisedPassword", IDS_PASSWORD_MANAGER_UI_MUTE_ISSUE},
    {"mutedCompromisedCredentials",
     IDS_PASSWORD_MANAGER_UI_MUTED_COMPROMISED_PASSWORDS},
    {"notValidWebsite", IDS_PASSWORD_MANAGER_UI_NOT_VALID_WEB_ADDRESS},
    {"noteLabel", IDS_PASSWORD_MANAGER_UI_NOTE_LABEL},
    {"noPasswordsFound", IDS_PASSWORD_MANAGER_UI_NO_PASSWORDS_FOUND},
    {"opensInNewTab", IDS_PASSWORD_MANAGER_UI_OPENS_IN_NEW_TAB},
    {"passwordCopiedToClipboard",
     IDS_PASSWORD_MANAGER_UI_PASSWORD_COPIED_TO_CLIPBOARD},
    {"passwordDeleted", IDS_PASSWORD_MANAGER_UI_PASSWORD_DELETED},
    {"passwordLabel", IDS_PASSWORD_MANAGER_UI_PASSWORD_LABEL},
    {"passwordManager",
     IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT},
    {"passwordNoteCharacterCount",
     IDS_PASSWORD_MANAGER_UI_NOTE_CHARACTER_COUNT},
    {"passwordNoteCharacterCountWarning",
     IDS_PASSWORD_MANAGER_UI_NOTE_CHARACTER_COUNT_WARNING},
    {"passwords", IDS_PASSWORD_MANAGER_UI_PASSWORDS},
    {"phishedAndLeakedPassword",
     IDS_PASSWORD_MANAGER_UI_PASSWORD_PHISHED_AND_LEAKED},
    {"phishedPassword", IDS_PASSWORD_MANAGER_UI_PASSWORD_PHISHED},
    {"removeBlockedAriaDescription",
     IDS_PASSWORD_MANAGER_UI_REMOVE_BLOCKED_SITE_ARIA_DESCRIPTION},
    {"reload", IDS_RELOAD},
    {"reusedPasswordsDescription",
     IDS_PASSWORD_MANAGER_UI_REUSED_PASSWORDS_DESCRIPTION},
    {"reusedPasswordsEmpty", IDS_PASSWORD_MANAGER_UI_NO_REUSED_PASSWORDS},
    {"reusedPasswordsTitle", IDS_PASSWORD_MANAGER_UI_HAS_REUSED_PASSWORDS},
    {"runCheckupAriaDescription",
     IDS_PASSWORD_MANAGER_UI_RUN_CHECKUP_ARIA_DESCRIPTION},
    {"save", IDS_SAVE},
    {"savePasswordsLabel", IDS_PASSWORD_MANAGER_UI_SAVE_PASSWORDS_TOGGLE_LABEL},
    {"share", IDS_PASSWORD_MANAGER_UI_SHARE},
    {"searchPrompt", IDS_PASSWORD_MANAGER_UI_SEARCH_PROMPT},
    {"selectFile", IDS_PASSWORD_MANAGER_UI_SELECT_FILE},
    {"settings", IDS_PASSWORD_MANAGER_UI_SETTINGS},
    {"showMore", IDS_PASSWORD_MANAGER_UI_SHOW_MORE},
    {"showPassword", IDS_PASSWORD_MANAGER_UI_SHOW_PASSWORD},
    {"showPasswordA11yLabel", IDS_PASSWORD_MANAGER_UI_SHOW_PASSWORD_A11Y},
    {"sitesAndAppsLabel", IDS_PASSWORD_MANAGER_UI_SITES_AND_APPS_LABEL},
    {"sitesLabel", IDS_PASSWORD_MANAGER_UI_SITES_LABEL},
    {"title", IDS_PASSWORD_MANAGER_UI_TITLE},
    {"trustedVaultBannerLabelOfferOptIn",
     IDS_PASSWORD_MANAGER_UI_TRUSTED_VAULT_OPT_IN_TITLE},
    {"trustedVaultBannerSubLabelOfferOptIn",
     IDS_PASSWORD_MANAGER_UI_TRUSTED_VAULT_OPT_IN_DESCRIPTION},
    {"trustedVaultBannerLabelOptedIn",
     IDS_PASSWORD_MANAGER_UI_TRUSTED_VAULT_OPTED_IN_TITLE},
    {"trustedVaultBannerSubLabelOptedIn",
     IDS_PASSWORD_MANAGER_UI_TRUSTED_VAULT_OPTED_IN_DESCRIPTION},
    {"tryAgain", IDS_PASSWORD_MANAGER_UI_CHECK_PASSWORDS_AFTER_ERROR},
    {"undoRemovePassword", IDS_PASSWORD_MANAGER_UI_UNDO},
    {"unmuteCompromisedPassword", IDS_PASSWORD_MANAGER_UI_UNMUTE_ISSUE},
    {"usernameAlreadyUsed", IDS_PASSWORD_MANAGER_UI_USERNAME_ALREADY_USED},
    {"usernameCopiedToClipboard",
     IDS_PASSWORD_MANAGER_UI_USERNAME_COPIED_TO_CLIPBOARD},
    {"usernameLabel", IDS_PASSWORD_MANAGER_UI_USERNAME_LABEL},
    {"viewExistingPassword", IDS_PASSWORD_MANAGER_UI_VIEW_EXISTING_PASSWORD},
    {"viewExistingPasswordAriaDescription",
     IDS_PASSWORD_MANAGER_UI_VIEW_EXISTING_PASSWORD_ARIA_DESCRIPTION},
    {"viewPasswordAriaDescription",
     IDS_PASSWORD_MANAGER_UI_VIEW_PASSWORD_ARIA_DESCRIPTION},
    {"viewPasswordsButton", IDS_PASSWORD_MANAGER_UI_IMPORT_VIEW_PASSWORDS},
    {"weakPasswordsDescription",
     IDS_PASSWORD_MANAGER_UI_WEAK_PASSWORDS_DESCRIPTION},
    {"weakPasswordsEmpty", IDS_PASSWORD_MANAGER_UI_NO_WEAK_PASSWORDS},
    {"weakPasswordsTitle", IDS_PASSWORD_MANAGER_UI_HAS_WEAK_PASSWORDS},
    {"websiteLabel", IDS_PASSWORD_MANAGER_UI_WEBSITE_LABEL},
#if BUILDFLAG(IS_MAC)
    {"biometricAuthenticaionForFillingLabel",
     IDS_PASSWORD_MANAGER_UI_BIOMETRIC_AUTHENTICATION_FOR_FILLING_TOGGLE_LABEL_MAC},
    {"biometricAuthenticaionForFillingSublabel",
     IDS_PASSWORD_MANAGER_UI_BIOMETRIC_AUTHENTICATION_FOR_FILLING_TOGGLE_SUBLABEL_MAC},
#elif BUILDFLAG(IS_WIN)
    {"biometricAuthenticaionForFillingLabel",
     IDS_PASSWORD_MANAGER_UI_BIOMETRIC_AUTHENTICATION_FOR_FILLING_TOGGLE_LABEL_WIN},
    {"biometricAuthenticaionForFillingSublabel",
     IDS_PASSWORD_MANAGER_UI_BIOMETRIC_AUTHENTICATION_FOR_FILLING_TOGGLE_SUBLABEL_WIN},
#endif
  };
  for (const auto& str : kStrings)
    webui::AddLocalizedString(source, str.name, str.id);

  source->AddString(
      "passwordsSectionDescription",
      l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORDS_DESCRIPTION,
          base::ASCIIToUTF16(chrome::kPasswordManagerLearnMoreURL)));

  source->AddString(
      "checkupUrl",
      base::UTF8ToUTF16(
          password_manager::GetPasswordCheckupURL(
              password_manager::PasswordCheckupReferrer::kPasswordCheck)
              .spec()));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  source->AddBoolean("biometricAuthenticationForFillingToggleVisible",
                     password_manager_util::
                         ShouldBiometricAuthenticationForFillingToggleBeVisible(
                             g_browser_process->local_state()));
#endif

  source->AddBoolean("enablePasswordsImportM2",
                     base::FeatureList::IsEnabled(
                         password_manager::features::kPasswordsImportM2));

  source->AddBoolean(
      "enableSendPasswords",
      base::FeatureList::IsEnabled(password_manager::features::kSendPasswords));

  source->AddString("passwordManagerLearnMoreURL",
                    chrome::kPasswordManagerLearnMoreURL);

  ui::Accelerator undo_accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("undoDescription", l10n_util::GetStringFUTF16(
                                           IDS_UNDO_DESCRIPTION,
                                           undo_accelerator.GetShortcutText()));

  // Password details page timeouts in 5 minutes:
  source->AddString(
      "authTimedOutDescription",
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_AUTH_TIMED_OUT_DESCRIPTION),
          l10n_util::GetStringUTF16(
              IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE),
          syncer::kPasswordNotesAuthValidity.Get().InMinutes()));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Overwrite ubranded logo for Chrome-branded builds.
  source->AddResourcePath("images/password_manager_logo.svg",
                          IDR_CHROME_PASSWORD_MANAGER_LOGO);

  // This path is used in the manifest of the PasswordManager web app
  // (chrome/browser/resources/password_manager/
  // chrome_branded_manifest.webmanifest).
  source->AddResourcePath("images/password_manager_pwa_icon.svg",
                          IDR_CHROME_PASSWORD_MANAGER_PWA_ICON);
#endif

  source->AddString("trustedVaultOptInUrl", chrome::kSyncTrustedVaultOptInURL);
  source->AddString("trustedVaultLearnMoreUrl",
                    chrome::kSyncTrustedVaultLearnMoreURL);

  source->AddString("addShortcutDescription",
                    InsertBrandedPasswordManager(
                        IDS_PASSWORD_MANAGER_UI_ADD_SHORTCUT_DESCRIPTION));

  source->AddString("autosigninDescription",
                    InsertBrandedPasswordManager(
                        IDS_PASSWORD_MANAGER_UI_AUTOSIGNIN_TOGGLE_DESC));

  source->AddString(
      "emptyStateImportAccountStore",
      InsertBrandedPasswordManager(
          IDS_PASSWORD_MANAGER_UI_EMPTY_STATE_ACCOUNT_STORE_USERS));
  source->AddString("emptyStateImportDevice",
                    InsertBrandedPasswordManager(
                        IDS_PASSWORD_MANAGER_UI_EMPTY_STATE_SIGNEDOUT_USERS));

  source->AddString(
      "importPasswordsGenericDescription",
      InsertBrandedPasswordManager(
          IDS_PASSWORD_MANAGER_UI_IMPORT_DESCRIPTION_ACCOUNT_STORE_USERS));
  source->AddString(
      "importPasswordsDescriptionDevice",
      InsertBrandedPasswordManager(
          IDS_PASSWORD_MANAGER_UI_IMPORT_DESCRIPTION_SIGNEDOUT_USERS));
  source->AddString("importPasswordsConflictsDescription",
                    InsertBrandedPasswordManager(
                        IDS_PASSWORD_MANAGER_UI_IMPORT_CONFLICTS_DESCRIPTION));
  source->AddString("passwordsStoreOptionDevice",
                    InsertBrandedPasswordManager(
                        IDS_PASSWORD_MANAGER_UI_STORE_PICKER_OPTION_DEVICE));

  source->AddString(
      "importPasswordsLimitExceeded",
      l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_UI_IMPORT_ERROR_LIMIT_EXCEEDED,
          base::NumberToString16(
              password_manager::constants::kMaxPasswordsPerCSVFile)));

  source->AddString("importPasswordsHelpURL",
                    chrome::kPasswordManagerImportLearnMoreURL);

  source->AddBoolean(
      "canAddShortcut",
      web_app::WebAppProvider::GetForWebApps(profile) != nullptr &&
          web_app::AreWebAppsUserInstallable(profile));

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  return source;
}

void AddPluralStrings(content::WebUI* web_ui) {
  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "checkedPasswords", IDS_PASSWORD_MANAGER_UI_CHECKUP_RESULT);
  plural_string_handler->AddLocalizedString(
      "checkingPasswords", IDS_PASSWORD_MANAGER_UI_CHECKUP_RUNNING_LABEL);
  plural_string_handler->AddLocalizedString(
      "compromisedPasswords",
      IDS_PASSWORD_MANAGER_UI_COMPROMISED_PASSWORDS_COUNT);
  plural_string_handler->AddLocalizedString(
      "importPasswordsFailuresSummary",
      IDS_PASSWORD_MANAGER_UI_IMPORT_FAILURES_SUMMARY);
  plural_string_handler->AddLocalizedString(
      "importPasswordsBadRowsFormat",
      IDS_PASSWORD_MANAGER_UI_IMPORT_BAD_ROWS_FORMAT);
  plural_string_handler->AddLocalizedString(
      "importPasswordsSuccessSummaryAccount",
      IDS_PASSWORD_MANAGER_UI_IMPORT_SUCCESS_SUMMARY_ACCOUNT);
  plural_string_handler->AddLocalizedString(
      "importPasswordsSuccessSummaryDevice",
      IDS_PASSWORD_MANAGER_UI_IMPORT_SUCCESS_SUMMARY_DEVICE);
  plural_string_handler->AddLocalizedString(
      "importPasswordsConflictsTitle",
      IDS_PASSWORD_MANAGER_UI_IMPORT_CONFLICTS_TITLE);
  plural_string_handler->AddLocalizedString(
      "numberOfAccounts", IDS_PASSWORD_MANAGER_UI_NUMBER_OF_ACCOUNTS);
  plural_string_handler->AddLocalizedString(
      "numberOfPasswordReuse",
      IDS_PASSWORD_MANAGER_UI_NUMBER_OF_CREDENTIALS_WITH_REUSED_PASSWORD);
  plural_string_handler->AddLocalizedString(
      "reusedPasswords", IDS_PASSWORD_MANAGER_UI_REUSED_PASSWORDS_COUNT);
  plural_string_handler->AddLocalizedString(
      "weakPasswords", IDS_PASSWORD_MANAGER_UI_WEAK_PASSWORDS_COUNT);
  plural_string_handler->AddLocalizedString(
      "searchResults", IDS_PASSWORD_MANAGER_UI_SEARCH_RESULT);
  plural_string_handler->AddLocalizedString(
      "movePasswords", IDS_PASSWORD_MANAGER_UI_MOVE_PASSWORDS_TO_ACCOUNT);
  web_ui->AddMessageHandler(std::move(plural_string_handler));
}

}  // namespace

PasswordManagerUI::PasswordManagerUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://password-manager/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  passwords_private_delegate_ =
      extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(profile,
                                                                        true);
  web_ui->AddMessageHandler(
      std::make_unique<password_manager::SyncHandler>(profile));
  web_ui->AddMessageHandler(std::make_unique<ExtensionControlHandler>());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  web_ui->AddMessageHandler(
      std::make_unique<password_manager::PromoCardsHandler>(
          profile,
          password_manager::PromoCardInterface::GetAllPromoCardsForProfile(
              profile)));
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  web_ui->AddMessageHandler(std::make_unique<settings::PasskeysHandler>());
#endif
  auto* source = CreateAndAddPasswordsUIHTMLSource(profile, web_ui);
  AddPluralStrings(web_ui);
  ManagedUIHandler::Initialize(web_ui, source);
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
}

PasswordManagerUI::~PasswordManagerUI() = default;

// static
base::RefCountedMemory* PasswordManagerUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return static_cast<base::RefCountedMemory*>(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_PASSWORD_MANAGER_FAVICON, scale_factor));
}
