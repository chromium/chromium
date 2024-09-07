// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/signin/inline_login_ui.h"

#include <memory>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/test_files_request_filter.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/gaia_auth_host_resources.h"
#include "chrome/grit/gaia_auth_host_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/inline_login_resources.h"
#include "chrome/grit/inline_login_resources_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/ash/edu_coexistence/edu_coexistence_login_handler.h"
#include "chrome/browser/ui/webui/signin/ash/edu_account_login_handler.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_handler_impl.h"
#include "chrome/grit/arc_account_picker_resources.h"
#include "chrome/grit/arc_account_picker_resources_map.h"
#include "chrome/grit/edu_coexistence_resources.h"
#include "chrome/grit/edu_coexistence_resources_map.h"
#include "chrome/grit/gaia_action_buttons_resources.h"
#include "chrome/grit/gaia_action_buttons_resources_map.h"
#include "chrome/grit/supervision_resources.h"
#include "chrome/grit/supervision_resources_map.h"
#include "components/account_manager_core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"
#else
#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AddEduStrings(content::WebUIDataSource* source,
                   const std::u16string& username) {
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

  // Strings for server based EDU Coexistence flow.
  source->AddLocalizedString("supervisedUserOfflineTitle",
                             IDS_SUPERVISED_USER_OFFLINE_TITLE);
  source->AddLocalizedString("supervisedUserOfflineDescription",
                             IDS_SUPERVISED_USER_OFFLINE_DESCRIPTION);
  source->AddLocalizedString("supervisedUserErrorTitle",
                             IDS_SUPERVISED_USER_ERROR_TITLE);
  source->AddLocalizedString("supervisedUserErrorDescription",
                             IDS_SUPERVISED_USER_ERROR_DESCRIPTION);
  source->AddLocalizedString("loadingMessage", IDS_LOGIN_GAIA_LOADING_MESSAGE);
  source->AddLocalizedString(
      "addSchoolAccountLabel",
      IDS_ACCOUNT_MANAGER_DIALOG_ADD_SCHOOL_ACCOUNT_LABEL);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void CreateAndAddWebUIDataSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIChromeSigninHost);

  source->AddResourcePaths(
      base::make_span(kInlineLoginResources, kInlineLoginResourcesSize));
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kGaiaAuthHostResources, kGaiaAuthHostResourcesSize),
      IDR_INLINE_LOGIN_INLINE_LOGIN_HTML);
  // TODO(crbug.com/40250068): Remove this when saml_password_attributes.js is
  // made TrustedTypes compliant.
  source->DisableTrustedTypesCSP();
  // Necessary since this UI sends XML Http requests.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc, "connect-src *;");

#if BUILDFLAG(IS_CHROMEOS_ASH)
  source->AddResourcePaths(base::make_span(kArcAccountPickerResources,
                                           kArcAccountPickerResourcesSize));
  source->AddResourcePaths(base::make_span(kGaiaActionButtonsResources,
                                           kGaiaActionButtonsResourcesSize));
  source->AddResourcePaths(
      base::make_span(kEduCoexistenceResources, kEduCoexistenceResourcesSize));
  source->AddResourcePaths(
      base::make_span(kSupervisionResources, kSupervisionResourcesSize));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Only add a filter when runing as test.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const bool is_running_test = command_line->HasSwitch(::switches::kTestName) ||
                               command_line->HasSwitch(::switches::kTestType);
  if (is_running_test) {
    source->SetRequestFilter(test::GetTestShouldHandleRequest(),
                             test::GetTestFilesRequestFilter());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  static constexpr webui::ResourcePath kResources[] = {
    {"account_manager_shared.css.js", IDR_ACCOUNT_MANAGER_SHARED_CSS_JS},
    {"error_screen.html.js",
     IDR_ACCOUNT_MANAGER_COMPONENTS_ERROR_SCREEN_HTML_JS},
    {"error_screen.js", IDR_ACCOUNT_MANAGER_COMPONENTS_ERROR_SCREEN_JS},
    // Resources for the server-based edu coexistence flow.
    {"edu-coexistence", IDR_EDU_COEXISTENCE_EDU_COEXISTENCE_HTML},

    {"account_manager_signin_blocked_by_policy.svg",
     IDS_ACCOUNT_MANAGER_SIGNIN_BLOCKED_BY_POLICY_SVG},

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"account_manager_welcome_1x.png", IDR_ACCOUNT_MANAGER_WELCOME_1X_PNG},
    {"account_manager_welcome_2x.png", IDR_ACCOUNT_MANAGER_WELCOME_2X_PNG},
    {"googleg.svg", IDR_ACCOUNT_MANAGER_WELCOME_GOOGLE_LOGO_SVG},
#endif
  };
  source->AddResourcePaths(kResources);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"accessibleCloseButtonLabel", IDS_SIGNIN_ACCESSIBLE_CLOSE_BUTTON},
    {"accessibleBackButtonLabel", IDS_SIGNIN_ACCESSIBLE_BACK_BUTTON},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"title", IDS_ACCOUNT_MANAGER_DIALOG_TITLE},
    {"ok", IDS_APP_OK},
    {"nextButtonLabel", IDS_ACCOUNT_MANAGER_DIALOG_NEXT_BUTTON},
    {"accountManagerDialogWelcomeTitle",
     IDS_ACCOUNT_MANAGER_DIALOG_WELCOME_TITLE},
    {"accountManagerDialogWelcomeCheckbox",
     IDS_ACCOUNT_MANAGER_DIALOG_WELCOME_CHECKBOX},
    {"accountManagerDialogArcAccountPickerTitle",
     IDS_ACCOUNT_MANAGER_DIALOG_ARC_ACCOUNT_PICKER_TITLE},
    {"addAccountLabel", IDS_ACCOUNT_MANAGER_DIALOG_ADD_ACCOUNT_LABEL},
    {"accountUseInArcButtonLabel",
     IDS_SETTINGS_ACCOUNT_MANAGER_USE_IN_ARC_BUTTON_LABEL},
    {"accountManagerErrorNoInternetTitle",
     IDS_ACCOUNT_MANAGER_ERROR_NO_INTERNET_TITLE},
    {"accountManagerErrorNoInternetBody",
     IDS_ACCOUNT_MANAGER_ERROR_NO_INTERNET_BODY},
    {"accountManagerErrorCannotAddAccountTitle",
     IDS_ACCOUNT_MANAGER_ERROR_CANNOT_ADD_ACCOUNT_TITLE},
    {"accountManagerErrorCannotAddAccountBody",
     IDS_ACCOUNT_MANAGER_ERROR_CANNOT_ADD_ACCOUNT_BODY},
    {"accountManagerDialogSigninBlockedByPolicyTitle",
     IDS_ACCOUNT_MANAGER_DIALOG_SIGNIN_BLOCKED_BY_POLICY_TITLE},
    {"accountManagerDialogSigninBlockedByPolicyBody",
     IDS_ACCOUNT_MANAGER_DIALOG_SIGNIN_BLOCKED_BY_POLICY_BODY},
    {"accountManagerDialogSigninErrorTitle",
     IDS_ACCOUNT_MANAGER_DIALOG_SIGNIN_ERROR_TITLE},
    {"accountManagerDialogSigninErrorBody",
     IDS_ACCOUNT_MANAGER_DIALOG_SIGNIN_ERROR_BODY},
    {"accountManagerDialogSigninBlockedByPolicyImageAlt",
     IDS_ACCOUNT_MANAGER_DIALOG_SIGNIN_BLOCKED_BY_POLICY_IMAGE_ALT},
    {"accountManagerDialogSigninSpinnerText",
     IDS_ACCOUNT_MANAGER_DIALOG_SIGNIN_SPINNER_TEXT},
#else
    {"title", IDS_CHROME_SIGNIN_TITLE},
#endif
  };
  source->AddLocalizedStrings(kLocalizedStrings);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  source->AddBoolean(
      "secondaryGoogleAccountSigninAllowed",
      profile->GetPrefs()->GetBoolean(
          ::account_manager::prefs::kSecondaryGoogleAccountSigninAllowed));
  source->AddBoolean(
      "isArcAccountRestrictionsEnabled",
      ash::AccountAppsAvailability::IsArcAccountRestrictionsEnabled());
  // The "Apps Settings" link points to Apps > Manage your apps.
  source->AddString(
      "accountManagerDialogArcToggleLabel",
      l10n_util::GetStringFUTF16(
          IDS_ACCOUNT_MANAGER_DIALOG_ARC_TOGGLE_LABEL,
          base::UTF8ToUTF16(
              chrome::GetOSSettingsUrl(
                  chromeos::settings::mojom::kAppManagementSubpagePath)
                  .spec())));
  source->AddString(
      "accountManagerDialogArcAccountPickerBody",
      l10n_util::GetStringFUTF16(
          IDS_ACCOUNT_MANAGER_DIALOG_ARC_ACCOUNT_PICKER_BODY,
          base::UTF8ToUTF16(
              chrome::GetOSSettingsUrl(
                  chromeos::settings::mojom::kMyAccountsSubpagePath)
                  .spec())));
  source->AddBoolean(
      "shouldSkipWelcomePage",
      ash::AccountAppsAvailability::IsArcAccountRestrictionsEnabled()
          ? false
          : profile->GetPrefs()->GetBoolean(
                ash::prefs::kShouldSkipInlineLoginWelcomePage));
  if (ash::AccountAppsAvailability::IsArcAccountRestrictionsEnabled()) {
    int message_id = IDS_ACCOUNT_MANAGER_DIALOG_WELCOME_BODY_V2_WITHOUT_GUEST;
    // Offer browser guest mode or device guest mode, if available.
    if (profiles::IsGuestModeEnabled()) {
      message_id = IDS_ACCOUNT_MANAGER_DIALOG_WELCOME_BODY_V2_WITH_GUEST_MODE;
    } else if (user_manager::UserManager::Get()->IsGuestSessionAllowed()) {
      message_id =
          IDS_ACCOUNT_MANAGER_DIALOG_WELCOME_BODY_V2_WITH_DEVICE_GUEST_MODE;
    }

    source->AddString(
        "accountManagerDialogWelcomeBody",
        l10n_util::GetStringFUTF16(
            message_id,
            // "add a new person" link:
            chrome::kAddNewUserURL,
            // Device type:
            ui::GetChromeOSDeviceName(),
            // Settings > Accounts link:
            base::UTF8ToUTF16(
                chrome::GetOSSettingsUrl(
                    chromeos::settings::mojom::kMyAccountsSubpagePath)
                    .spec())));

    source->AddString(
        "accountManagerDialogWelcomeBodyArc",
        l10n_util::GetStringFUTF16(
            IDS_ACCOUNT_MANAGER_DIALOG_WELCOME_BODY_ARC,
            // "add a new person" link:
            chrome::kAddNewUserURL,
            // Device type:
            ui::GetChromeOSDeviceName(),
            // "Apps Settings" link:
            base::UTF8ToUTF16(
                chrome::GetOSSettingsUrl(
                    chromeos::settings::mojom::kAppManagementSubpagePath)
                    .spec())));
  } else {
    bool is_incognito_enabled =
        (IncognitoModePrefs::GetAvailability(profile->GetPrefs()) !=
         policy::IncognitoModeAvailability::kDisabled);
    int message_id =
        is_incognito_enabled
            ? IDS_ACCOUNT_MANAGER_DIALOG_WELCOME_BODY
            : IDS_ACCOUNT_MANAGER_DIALOG_WELCOME_BODY_WITHOUT_INCOGNITO;
    source->AddString(
        "accountManagerDialogWelcomeBody",
        l10n_util::GetStringFUTF16(
            message_id,
            base::UTF8ToUTF16(
                chrome::GetOSSettingsUrl(
                    chromeos::settings::mojom::kMyAccountsSubpagePath)
                    .spec()),
            ui::GetChromeOSDeviceName()));
  }

  source->AddBoolean("isChild",
                     user_manager::UserManager::Get()->IsLoggedInAsChildUser());

  user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);
  source->AddString("userName", user->GetGivenName());
  source->AddString("accountManagerOsSettingsUrl",
                    chrome::GetOSSettingsUrl(
                        chromeos::settings::mojom::kMyAccountsSubpagePath)
                        .spec());

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      "frame-src chrome://webui-test/;");

  std::u16string username =
      ash::ProfileHelper::Get()->GetUserByProfile(profile)->GetGivenName();
  AddEduStrings(source, username);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Returns whether |url| can be displayed in a chrome://chrome-signin web
// contents, depending on the signin reason that is encoded in the url.
bool IsValidChromeSigninReason(const GURL& url) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(url);

  switch (reason) {
    case signin_metrics::Reason::kForcedSigninPrimaryAccount:
    case signin_metrics::Reason::kReauthentication:
      // Used by the profile picker.
      return true;
    case signin_metrics::Reason::kFetchLstOnly:
#if BUILDFLAG(IS_WIN)
      // Used by the Google Credential Provider for Windows.
      return true;
#else
      return false;
#endif
    case signin_metrics::Reason::kSigninPrimaryAccount:
    case signin_metrics::Reason::kAddSecondaryAccount:
    case signin_metrics::Reason::kUnknownReason:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

InlineLoginUI::InlineLoginUI(content::WebUI* web_ui) : WebDialogUI(web_ui) {
  // Always instantiate the WebUIDataSource so that tests pulling deps from
  // from chrome://chrome-signin/gaia_auth_host/ can work.
  Profile* profile = Profile::FromWebUI(web_ui);
  CreateAndAddWebUIDataSource(profile);

  if (!IsValidChromeSigninReason(web_ui->GetWebContents()->GetVisibleURL()))
    return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui->AddMessageHandler(
      std::make_unique<ash::InlineLoginHandlerImpl>(base::BindRepeating(
          &WebDialogUIBase::CloseDialog, weak_factory_.GetWeakPtr(),
          base::Value::List() /* args */)));
  if (profile->IsChild()) {
    web_ui->AddMessageHandler(
        std::make_unique<ash::EduCoexistenceLoginHandler>(base::BindRepeating(
            &WebDialogUIBase::CloseDialog, weak_factory_.GetWeakPtr(),
            base::Value::List() /* args */)));
  }

#else
  web_ui->AddMessageHandler(std::make_unique<InlineLoginHandlerImpl>());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  content::WebContents* contents = web_ui->GetWebContents();
  extensions::TabHelper::CreateForWebContents(contents);
  // Ensure that the login UI has a tab ID, which will allow the GAIA auth
  // extension's background script to tell it apart from iframes injected by
  // other extensions.
  CreateSessionServiceTabHelper(contents);
}

InlineLoginUI::~InlineLoginUI() {}
