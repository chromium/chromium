// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_error_ui.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_error_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/text_elider.h"

SigninErrorUI::SigninErrorUI(content::WebUI* web_ui)
    : SigninWebDialogUI(web_ui) {
  Profile* webui_profile = Profile::FromWebUI(web_ui);
  if (webui_profile->GetOriginalProfile()->IsSystemProfile()) {
    InitializeMessageHandlerForUserManager();
  }
}

void SigninErrorUI::InitializeMessageHandlerWithBrowser(Browser* browser) {
  DCHECK(browser);
  Initialize(browser, false /* is_system_profile */);
}

void SigninErrorUI::InitializeMessageHandlerForUserManager() {
  Initialize(nullptr, true /* is_system_profile */);
}

void SigninErrorUI::Initialize(Browser* browser, bool is_system_profile) {
  Profile* webui_profile = Profile::FromWebUI(web_ui());
  Profile* signin_profile;
  std::unique_ptr<SigninErrorHandler> handler =
      std::make_unique<SigninErrorHandler>(browser, is_system_profile);

  if (is_system_profile) {
    signin_profile = g_browser_process->profile_manager()->GetProfileByPath(
        UserManager::GetSigninProfilePath());
    // Sign in is completed before profile creation.
    if (!signin_profile)
      signin_profile = webui_profile->GetOriginalProfile();
  } else {
    signin_profile = webui_profile;
  }

  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISigninErrorHost);
  source->UseStringsJs();
  source->SetDefaultResource(IDR_SIGNIN_ERROR_HTML);
  source->AddResourcePath("signin_error.js", IDR_SIGNIN_ERROR_JS);
  source->AddResourcePath("signin_shared_old_css.html",
                          IDR_SIGNIN_SHARED_OLD_CSS_HTML);
  source->AddBoolean("isSystemProfile", is_system_profile);

  // Retrieve the last signin error message and email used.
  LoginUIService* login_ui_service =
      LoginUIServiceFactory::GetForProfile(signin_profile);
  const base::string16 last_login_result(
      login_ui_service->GetLastLoginResult());
  const base::string16 email = login_ui_service->GetLastLoginErrorEmail();
  const bool is_profile_blocked =
      login_ui_service->IsDisplayingProfileBlockedErrorMessage();
  if (is_profile_blocked) {
    source->AddLocalizedString("signinErrorTitle",
                               IDS_OLD_PROFILES_DISABLED_TITLE);
  } else if (email.empty()) {
    source->AddLocalizedString("signinErrorTitle", IDS_SIGNIN_ERROR_TITLE);
  } else {
    int title_string_id =
        AccountConsistencyModeManager::IsDiceEnabledForProfile(signin_profile)
            ? IDS_SIGNIN_ERROR_DICE_EMAIL_TITLE
            : IDS_SIGNIN_ERROR_EMAIL_TITLE;
    source->AddString("signinErrorTitle",
                      l10n_util::GetStringFUTF16(title_string_id, email));
  }

  source->AddString("signinErrorMessage", base::string16());
  source->AddString("profileBlockedMessage", base::string16());
  source->AddString("profileBlockedAddPersonSuggestion", base::string16());
  source->AddString("profileBlockedRemoveProfileSuggestion", base::string16());

  // Tweak the dialog UI depending on whether the signin error is
  // username-in-use error and the error UI is shown with a browser window.
  base::string16 existing_name;
  if (is_profile_blocked) {
    source->AddLocalizedString("profileBlockedMessage",
                               IDS_OLD_PROFILES_DISABLED_MESSAGE);
    std::string allowed_domain = signin_ui_util::GetAllowedDomain(
        g_browser_process->local_state()->GetString(
            prefs::kGoogleServicesUsernamePattern));
    if (allowed_domain.empty()) {
      source->AddLocalizedString(
          "profileBlockedAddPersonSuggestion",
          IDS_OLD_PROFILES_DISABLED_ADD_PERSON_SUGGESTION);
    } else {
      source->AddString(
          "profileBlockedAddPersonSuggestion",
          l10n_util::GetStringFUTF16(
              IDS_OLD_PROFILES_DISABLED_ADD_PERSON_SUGGESTION_WITH_DOMAIN,
              base::ASCIIToUTF16(allowed_domain)));
    }

    source->AddLocalizedString("profileBlockedRemoveProfileSuggestion",
                               IDS_OLD_PROFILES_DISABLED_REMOVED_OLD_PROFILE);
  } else if (!is_system_profile &&
             last_login_result.compare(l10n_util::GetStringUTF16(
                 IDS_SYNC_USER_NAME_IN_USE_ERROR)) == 0) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    if (profile_manager) {
      std::vector<ProfileAttributesEntry*> entries =
          profile_manager->GetProfileAttributesStorage()
              .GetAllProfilesAttributes();
      DCHECK(!email.empty());
      for (const ProfileAttributesEntry* entry : entries) {
        if (!entry->IsAuthenticated())
          continue;

        if (gaia::AreEmailsSame(base::UTF16ToUTF8(email),
                                base::UTF16ToUTF8(entry->GetUserName()))) {
          handler->set_duplicate_profile_path(entry->GetPath());
          existing_name = entry->GetName();
        }
      }
    }
    DCHECK(!existing_name.empty());
    source->AddString(
        "signinErrorMessage",
        l10n_util::GetStringFUTF16(IDS_SYNC_USER_NAME_IN_USE_BY_ERROR,
                                   existing_name));
    // Elide the existing name for the switch user button label.
    existing_name =
        gfx::TruncateString(existing_name, 10, gfx::CHARACTER_BREAK);
  } else {
    source->AddString("signinErrorMessage", last_login_result);
  }

  // Add button label strings.
  source->AddString("signinErrorSwitchLabel",
                    l10n_util::GetStringFUTF16(
                        IDS_SIGNIN_ERROR_SWITCH_BUTTON_LABEL, existing_name));
  source->AddLocalizedString("signinErrorLearnMore", IDS_LEARN_MORE);
  source->AddLocalizedString("signinErrorCloseLabel",
                             IDS_SIGNIN_ERROR_CLOSE_BUTTON_LABEL);
  source->AddLocalizedString("signinErrorOkLabel",
                             IDS_SIGNIN_ERROR_OK_BUTTON_LABEL);

  base::DictionaryValue strings;
  webui::SetLoadTimeDataDefaults(g_browser_process->GetApplicationLocale(),
                                 &strings);
  source->AddLocalizedStrings(strings);

  content::WebUIDataSource::Add(webui_profile, source);
  web_ui()->AddMessageHandler(std::move(handler));
}
