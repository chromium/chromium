// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/test_files_request_filter.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/gaia_auth_host_resources.h"
#include "chrome/grit/gaia_auth_host_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/resources/grit/webui_resources.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/chromeos/edu_account_login_handler_chromeos.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler_chromeos.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#else
#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"
#endif  // defined(OS_CHROMEOS)

namespace {

constexpr char kResourcesGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/";

#if defined(OS_CHROMEOS)
void AddEduStrings(content::WebUIDataSource* source,
                   const base::string16& username) {
  source->AddLocalizedString("okButton", IDS_APP_OK);
  source->AddLocalizedString("backButton", IDS_EDU_LOGIN_BACK);
  source->AddLocalizedString("nextButton", IDS_EDU_LOGIN_NEXT);

  source->AddLocalizedString("parentsListTitle", IDS_EDU_LOGIN_WELCOME_TITLE_2);
  source->AddLocalizedString("parentsListBody", IDS_EDU_LOGIN_WELCOME_BODY_2);
  source->AddLocalizedString("reauthBody", IDS_EDU_LOGIN_WELCOME_REAUTH_BODY);
  source->AddLocalizedString("parentSigninTitle",
                             IDS_EDU_LOGIN_PARENT_SIGNIN_TITLE);
  source->AddString(
      "parentSigninBody",
      l10n_util::GetStringFUTF16(IDS_EDU_LOGIN_PARENT_SIGNIN_BODY, username));
  source->AddLocalizedString("parentSigninPasswordLabel",
                             IDS_EDU_LOGIN_PARENT_SIGNIN_PASSWORD_LABEL);
  source->AddLocalizedString(
      "parentSigninPasswordError",
      IDS_EDU_LOGIN_PARENT_SIGNIN_PASSWORD_ERROR_MESSAGE);
  source->AddLocalizedString(
      "parentSigninAccountRecoveryText",
      IDS_EDU_LOGIN_PARENT_SIGNIN_ACCOUNT_RECOVERY_LINK_TEXT);
  source->AddLocalizedString("parentSigninPasswordShow",
                             IDS_EDU_LOGIN_PARENT_SIGNIN_PASSWORD_SHOW);
  source->AddLocalizedString("parentSigninPasswordHide",
                             IDS_EDU_LOGIN_PARENT_SIGNIN_PASSWORD_HIDE);
  source->AddString("parentSigninAccountRecoveryUrl",
                    chrome::kAccountRecoveryURL);

  source->AddLocalizedString("parentInfoTitle", IDS_EDU_LOGIN_INFO_TITLE);
  source->AddLocalizedString("parentInfoParentSettingsText",
                             IDS_EDU_LOGIN_INFO_PARENT_SETTINGS);
  source->AddString(
      "parentInfoBody",
      l10n_util::GetStringFUTF16(
          IDS_EDU_LOGIN_INFO_BODY,
          base::ASCIIToUTF16(chrome::kGsuiteTermsEducationPrivacyURL)));
  source->AddLocalizedString("coexistenceTitle",
                             IDS_EDU_LOGIN_INFO_COEXISTENCE_TITLE);
  source->AddLocalizedString("coexistenceBody",
                             IDS_EDU_LOGIN_INFO_COEXISTENCE_BODY);
}
#endif  // defined(OS_CHROMEOS)

content::WebUIDataSource* CreateWebUIDataSource() {
  content::WebUIDataSource* source =
        content::WebUIDataSource::Create(chrome::kChromeUIChromeSigninHost);
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kGaiaAuthHostResources, kGaiaAuthHostResourcesSize),
      kResourcesGeneratedPath, IDR_INLINE_LOGIN_HTML);

  // Only add a filter when runing as test.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const bool is_running_test = command_line->HasSwitch(::switches::kTestName) ||
                               command_line->HasSwitch(::switches::kTestType);
  if (is_running_test) {
    source->SetRequestFilter(test::GetTestShouldHandleRequest(),
                             test::GetTestFilesRequestFilter());
  }

  static constexpr webui::ResourcePath kResources[] = {
    {"inline_login_app.js", IDR_INLINE_LOGIN_APP_JS},
    {"inline_login_browser_proxy.js", IDR_INLINE_LOGIN_BROWSER_PROXY_JS},
#if defined(OS_CHROMEOS)
    {"gaia_action_buttons.js", IDR_GAIA_ACTION_BUTTONS_JS},
    {"error_screen.js", IDR_ACCOUNT_MANAGER_COMPONENTS_ERROR_SCREEN_JS},
    {"edu", IDR_EDU_LOGIN_EDU_LOGIN_HTML},
    {"app.js", IDR_EDU_LOGIN_EDU_LOGIN_JS},
    {"edu_login_button.js", IDR_EDU_LOGIN_EDU_LOGIN_BUTTON_JS},
    {"edu_login_template.js", IDR_EDU_LOGIN_EDU_LOGIN_TEMPLATE_JS},
    {"edu_login_css.js", IDR_EDU_LOGIN_EDU_LOGIN_CSS_JS},
    {"icons.js", IDR_EDU_LOGIN_ICONS_JS},
    {"browser_proxy.js", IDR_EDU_LOGIN_BROWSER_PROXY_JS},
    {"edu_login_util.js", IDR_EDU_LOGIN_EDU_LOGIN_UTIL_JS},
    {"edu_login_coexistence_info.js",
     IDR_EDU_LOGIN_EDU_LOGIN_COEXISTENCE_INFO_JS},
    {"edu_login_parents.js", IDR_EDU_LOGIN_EDU_LOGIN_PARENTS_JS},
    {"edu_login_parent_signin.js", IDR_EDU_LOGIN_EDU_LOGIN_PARENT_SIGNIN_JS},
    {"edu_login_parent_info.js", IDR_EDU_LOGIN_EDU_LOGIN_PARENT_INFO_JS},
    {"edu_login_signin.js", IDR_EDU_LOGIN_EDU_LOGIN_SIGNIN_JS},
    {"edu_login_error.js", IDR_EDU_LOGIN_EDU_LOGIN_ERROR_JS},
    // Resources for the server-based edu coexistence flow.
    {"edu-coexistence", IDR_EDU_COEXISTENCE_EDU_COEXISTENCE_HTML},
    {"edu_coexistence_app.js", IDR_EDU_COEXISTENCE_EDU_COEXISTENCE_APP_JS},
    {"edu_coexistence_ui.js", IDR_EDU_COEXISTENCE_EDU_COEXISTENCE_UI_JS},
    {"edu_coexistence_controller.js",
     IDR_EDU_COEXISTENCE_EDU_COEXISTENCE_CONTROLLER_JS},
    {"chromeos/add_supervision/post_message_api.m.js",
     IDR_ADD_SUPERVISION_POST_MESSAGE_API_M_JS},

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"googleg.svg", IDR_ACCOUNT_MANAGER_WELCOME_GOOGLE_LOGO_SVG},
#endif
    {"family_link_logo.svg", IDR_FAMILY_LINK_LOGO_SVG},
#endif  // defined(OS_CHROMEOS)
  };

  webui::AddResourcePathsBulk(source, kResources);

  source->AddLocalizedString("title", IDS_CHROME_SIGNIN_TITLE);
  source->AddLocalizedString(
      "accessibleCloseButtonLabel", IDS_SIGNIN_ACCESSIBLE_CLOSE_BUTTON);
  source->AddLocalizedString(
      "accessibleBackButtonLabel", IDS_SIGNIN_ACCESSIBLE_BACK_BUTTON);
#if defined(OS_CHROMEOS)
  source->AddLocalizedString("accountManagerErrorNoInternetTitle",
                             IDS_ACCOUNT_MANAGER_ERROR_NO_INTERNET_TITLE);
  source->AddLocalizedString("accountManagerErrorNoInternetBody",
                             IDS_ACCOUNT_MANAGER_ERROR_NO_INTERNET_BODY);
  source->AddLocalizedString(
      "accountManagerErrorCannotAddAccountTitle",
      IDS_ACCOUNT_MANAGER_ERROR_CANNOT_ADD_ACCOUNT_TITLE);
  source->AddLocalizedString("accountManagerErrorCannotAddAccountBody",
                             IDS_ACCOUNT_MANAGER_ERROR_CANNOT_ADD_ACCOUNT_BODY);
#endif
  return source;
}

// Returns whether |url| can be displayed in a chrome://chrome-signin tab,
// depending on the signin reason that is encoded in the url.
bool IsValidChromeSigninReason(const GURL& url) {
#if defined(OS_CHROMEOS)
  return true;
#else
  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(url);

  switch (reason) {
    case signin_metrics::Reason::REASON_FORCED_SIGNIN_PRIMARY_ACCOUNT:
    case signin_metrics::Reason::REASON_UNLOCK:
      // Used by the user manager.
      return true;
    case signin_metrics::Reason::REASON_FETCH_LST_ONLY:
#if defined(OS_WIN)
      // Used by the Google Credential Provider for Windows.
      return true;
#else
      return false;
#endif
    case signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT:
    case signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT:
    case signin_metrics::Reason::REASON_REAUTHENTICATION:
    case signin_metrics::Reason::REASON_UNKNOWN_REASON:
      return false;
    case signin_metrics::Reason::REASON_MAX:
      NOTREACHED();
      return false;
  }
  NOTREACHED();
#endif  // defined(OS_CHROMEOS)
}

}  // namespace

InlineLoginUI::InlineLoginUI(content::WebUI* web_ui) : WebDialogUI(web_ui) {
  if (!IsValidChromeSigninReason(web_ui->GetWebContents()->GetVisibleURL()))
    return;

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = CreateWebUIDataSource();
#if defined(OS_CHROMEOS)
  base::string16 username =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile)->GetGivenName();
  AddEduStrings(source, username);
#endif  // defined(OS_CHROMEOS)
  content::WebUIDataSource::Add(profile, source);

#if defined(OS_CHROMEOS)
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::InlineLoginHandlerChromeOS>(
          base::BindRepeating(&WebDialogUIBase::CloseDialog,
                              weak_factory_.GetWeakPtr(), nullptr /* args */)));
  web_ui->AddMessageHandler(std::make_unique<chromeos::EduAccountLoginHandler>(
      base::BindRepeating(&WebDialogUIBase::CloseDialog,
                          weak_factory_.GetWeakPtr(), nullptr /* args */)));
#else
  web_ui->AddMessageHandler(std::make_unique<InlineLoginHandlerImpl>());
#endif  // defined(OS_CHROMEOS)

  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  content::WebContents* contents = web_ui->GetWebContents();
  // Required for intercepting extension function calls when the page is loaded
  // in a bubble (not a full tab, thus tab helpers are not registered
  // automatically).
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      contents);
  extensions::TabHelper::CreateForWebContents(contents);
  // Ensure that the login UI has a tab ID, which will allow the GAIA auth
  // extension's background script to tell it apart from iframes injected by
  // other extensions.
  CreateSessionServiceTabHelper(contents);
}

InlineLoginUI::~InlineLoginUI() {}
