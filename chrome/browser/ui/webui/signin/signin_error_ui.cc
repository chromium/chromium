// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_error_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_error_handler.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/text_elider.h"

bool SigninErrorUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return !profile->IsOffTheRecord() || profile->IsSystemProfile();
}

SigninErrorUI::SigninErrorUI(content::WebUI* web_ui)
    : SigninWebDialogUI(web_ui) {
  const GURL& url = web_ui->GetWebContents()->GetVisibleURL();
  if (HasFromProfilePickerURLParameter(url)) {
    InitializeMessageHandlerForProfilePicker();
  }
}

void SigninErrorUI::InitializeMessageHandlerWithBrowser(Browser* browser) {
  DCHECK(browser);
  Initialize(browser, /*from_profile_picker=*/false);
}

void SigninErrorUI::InitializeMessageHandlerForProfilePicker() {
  Initialize(nullptr, /*from_profile_picker=*/true);
}

void SigninErrorUI::Initialize(Browser* browser, bool from_profile_picker) {
  Profile* webui_profile = Profile::FromWebUI(web_ui());
  std::unique_ptr<SigninErrorHandler> handler =
      std::make_unique<SigninErrorHandler>(browser, from_profile_picker);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      webui_profile, chrome::kChromeUISigninErrorHost);
  webui::EnableTrustedTypesCSP(source);
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  source->SetDefaultResource(IDR_SIGNIN_SIGNIN_ERROR_SIGNIN_ERROR_HTML);
  static constexpr webui::ResourcePath kResources[] = {
      {"signin_error_app.js", IDR_SIGNIN_SIGNIN_ERROR_SIGNIN_ERROR_APP_JS},
      {"signin_error_app.html.js",
       IDR_SIGNIN_SIGNIN_ERROR_SIGNIN_ERROR_APP_HTML_JS},
      {"signin_error_app.css.js",
       IDR_SIGNIN_SIGNIN_ERROR_SIGNIN_ERROR_APP_CSS_JS},
      {"signin_error.js", IDR_SIGNIN_SIGNIN_ERROR_SIGNIN_ERROR_JS},
      {"signin_shared.css.js", IDR_SIGNIN_SIGNIN_SHARED_CSS_JS},
      {"signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS},
  };
  source->AddResourcePaths(kResources);
  source->AddBoolean("fromProfilePicker", from_profile_picker);

  // Retrieve the last signin error message and email used.
  LoginUIService* login_ui_service =
      LoginUIServiceFactory::GetForProfile(webui_profile);
  const SigninUIError last_login_error = login_ui_service->GetLastLoginError();
  const bool is_profile_blocked =
      last_login_error.type() == SigninUIError::Type::kProfileIsBlocked;
  if (is_profile_blocked) {
    source->AddLocalizedString("signinErrorTitle",
                               IDS_OLD_PROFILES_DISABLED_TITLE);
  } else if (last_login_error.email().empty()) {
    // TODO(crbug.com/40150925): investigate whether an empty email
    // string is ever passed and possibly add a DCHECK.
    source->AddLocalizedString("signinErrorTitle", IDS_SIGNIN_ERROR_TITLE);
  } else {
    source->AddString("signinErrorTitle",
                      l10n_util::GetStringFUTF16(IDS_SIGNIN_ERROR_EMAIL_TITLE,
                                                 last_login_error.email()));
  }

  source->AddString("signinErrorMessage", std::u16string());
  source->AddString("profileBlockedMessage", std::u16string());
  source->AddString("profileBlockedAddPersonSuggestion", std::u16string());
  source->AddString("profileBlockedRemoveProfileSuggestion", std::u16string());

  // Tweak the dialog UI depending on whether the signin error is
  // username-in-use error and the error UI is shown with a browser window.
  std::u16string existing_name;
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
  } else if (!from_profile_picker &&
             last_login_error.type() ==
                 SigninUIError::Type::kAccountAlreadyUsedByAnotherProfile) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(
                last_login_error.another_profile_path());
    DCHECK(entry);
    DCHECK(entry->IsAuthenticated() || entry->CanBeManaged());
    handler->set_duplicate_profile_path(entry->GetPath());
    existing_name = entry->GetName();
    source->AddString("signinErrorMessage",
                      l10n_util::GetStringFUTF16(
                          IDS_SYNC_USER_NAME_IN_USE_BY_ERROR, existing_name));
    // Elide the existing name for the switch user button label.
    existing_name =
        gfx::TruncateString(existing_name, 10, gfx::CHARACTER_BREAK);
  } else {
    source->AddString("signinErrorMessage", last_login_error.message());
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

  base::Value::Dict strings;
  webui::SetLoadTimeDataDefaults(g_browser_process->GetApplicationLocale(),
                                 &strings);
  source->AddLocalizedStrings(strings);
  web_ui()->AddMessageHandler(std::move(handler));
}
