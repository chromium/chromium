// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_handler.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/web_ui.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace {

constexpr char kEnterprizeBadgeSource[] = "cr:domain";
constexpr char kSupervisedBadgeSource[] = "cr20:kite";

// Returns true if the account is managed (aka Enterprise, or Dasher).
bool IsManaged(const AccountInfo& info) {
  return info.hosted_domain != kNoHostedDomainFound;
}

// Returns true if the account capabilities are marked as supervised.
bool IsSupervisedUser(const AccountCapabilities& capabilities) {
  return capabilities.is_subject_to_parental_controls() ==
         signin::Tribool::kTrue;
}

SkColor GetProfileHighlightColor(Profile* profile) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  DCHECK(entry);

  return entry->GetProfileThemeColors().profile_highlight_color;
}

base::Value::Dict GetAccountInfoValue(const AccountInfo& info) {
  base::Value::Dict account_info_value;
  std::string_view avatar_badge = "";
  if (IsManaged(info)) {
    avatar_badge = kEnterprizeBadgeSource;
  } else if (IsSupervisedUser(info.capabilities) && base::FeatureList::IsEnabled(
      supervised_user::kShowKiteForSupervisedUsers)) {
    avatar_badge = kSupervisedBadgeSource;
  }
  account_info_value.Set("avatarBadge", avatar_badge);
  account_info_value.Set("pictureUrl", signin::GetAccountPictureUrl(info));
  return account_info_value;
}

}  // namespace

DiceWebSigninInterceptHandler::DiceWebSigninInterceptHandler(
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    base::OnceCallback<void(int)> show_widget_with_height_callback,
    base::OnceCallback<void(SigninInterceptionUserChoice)> completion_callback)
    : bubble_parameters_(bubble_parameters),
      show_widget_with_height_callback_(
          std::move(show_widget_with_height_callback)),
      completion_callback_(std::move(completion_callback)) {
  DCHECK(completion_callback_);
}

DiceWebSigninInterceptHandler::~DiceWebSigninInterceptHandler() = default;

void DiceWebSigninInterceptHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "accept",
      base::BindRepeating(&DiceWebSigninInterceptHandler::HandleAccept,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancel",
      base::BindRepeating(&DiceWebSigninInterceptHandler::HandleCancel,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "pageLoaded",
      base::BindRepeating(&DiceWebSigninInterceptHandler::HandlePageLoaded,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "chromeSigninPageLoaded",
      base::BindRepeating(
          &DiceWebSigninInterceptHandler::HandleChromeSigninPageLoaded,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initializedWithHeight",
      base::BindRepeating(
          &DiceWebSigninInterceptHandler::HandleInitializedWithHeight,
          base::Unretained(this)));
}

void DiceWebSigninInterceptHandler::OnJavascriptAllowed() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  identity_observation_.Observe(identity_manager);
}

void DiceWebSigninInterceptHandler::OnJavascriptDisallowed() {
  identity_observation_.Reset();
}

void DiceWebSigninInterceptHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (!info.IsValid())
    return;

  bool should_fire_event = false;
  if (info.account_id == intercepted_account().account_id) {
    should_fire_event = true;
    bubble_parameters_.intercepted_account = info;
  } else if (info.account_id == primary_account().account_id) {
    should_fire_event = true;
    bubble_parameters_.primary_account = info;
  }

  if (should_fire_event) {
    if (bubble_parameters_.interception_type ==
        WebSigninInterceptor::SigninInterceptionType::kChromeSignin) {
      // Updates might be needed if the picture URL is not yet ready.
      FireWebUIListener("interception-chrome-signin-parameters-changed",
                        GetInterceptionChromeSigninParametersValue());
      return;
    }

    FireWebUIListener("interception-parameters-changed",
                      GetInterceptionParametersValue());
  }
}

const AccountInfo& DiceWebSigninInterceptHandler::primary_account() {
  return bubble_parameters_.primary_account;
}

const AccountInfo& DiceWebSigninInterceptHandler::intercepted_account() {
  return bubble_parameters_.intercepted_account;
}

void DiceWebSigninInterceptHandler::HandleAccept(
    const base::Value::List& args) {
  if (completion_callback_) {
    std::move(completion_callback_).Run(SigninInterceptionUserChoice::kAccept);
  }
}

void DiceWebSigninInterceptHandler::HandleCancel(
    const base::Value::List& args) {
  if (completion_callback_) {
    std::move(completion_callback_).Run(SigninInterceptionUserChoice::kDecline);
  }
}

void DiceWebSigninInterceptHandler::HandlePageLoaded(
    const base::Value::List& args) {
  AllowJavascript();

  UpdateExtendedAccountsInfo();

  // If there is no extended info for the primary account, populate with
  // reasonable defaults.
  if (primary_account().hosted_domain.empty())
    bubble_parameters_.primary_account.hosted_domain = kNoHostedDomainFound;
  if (primary_account().given_name.empty())
    bubble_parameters_.primary_account.given_name = primary_account().email;

  DCHECK(!args.empty());
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, GetInterceptionParametersValue());
}

void DiceWebSigninInterceptHandler::HandleChromeSigninPageLoaded(
    const base::Value::List& args) {
  AllowJavascript();

  // Image might not be loaded yet.
  UpdateExtendedAccountsInfo();

  DCHECK(!args.empty());
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id,
                            GetInterceptionChromeSigninParametersValue());
}

void DiceWebSigninInterceptHandler::HandleInitializedWithHeight(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1u, args.size());
  int height = args[0].GetInt();
  CHECK_GE(height, 0);

  if (show_widget_with_height_callback_) {
    std::move(show_widget_with_height_callback_).Run(height);
  }
}

void DiceWebSigninInterceptHandler::UpdateExtendedAccountsInfo() {
  // Update the account info and the images.
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  AccountInfo updated_info =
      identity_manager->FindExtendedAccountInfo(intercepted_account());
  if (!updated_info.IsEmpty()) {
    bubble_parameters_.intercepted_account = updated_info;
  }

  updated_info = identity_manager->FindExtendedAccountInfo(primary_account());
  if (!updated_info.IsEmpty()) {
    bubble_parameters_.primary_account = updated_info;
  }
}

base::Value::Dict
DiceWebSigninInterceptHandler::GetInterceptionChromeSigninParametersValue() {
  base::Value::Dict parameters;
  parameters.Set("title", GetChromeSigninTitle());
  parameters.Set("subtitle", GetChromeSigninSubtitle());
  parameters.Set("email", intercepted_account().email);
  parameters.Set("fullName", intercepted_account().full_name);
  parameters.Set("givenName", intercepted_account().given_name);
  parameters.Set("pictureUrl",
                 signin::GetAccountPictureUrl(intercepted_account()));

  std::string_view managed_user_badge = "";
  if (IsSupervisedUser(intercepted_account().capabilities) &&
      base::FeatureList::IsEnabled(
          supervised_user::kShowKiteForSupervisedUsers)) {
    managed_user_badge = kSupervisedBadgeSource;
  }
  parameters.Set("managedUserBadge", managed_user_badge);
  return parameters;
}

base::Value::Dict
DiceWebSigninInterceptHandler::GetInterceptionParametersValue() {
  base::Value::Dict parameters;
  parameters.Set("headerText", GetHeaderText());
  parameters.Set("bodyTitle", GetBodyTitle());
  parameters.Set("bodyText", GetBodyText());
  parameters.Set("confirmButtonLabel", GetConfirmButtonLabel());
  parameters.Set("cancelButtonLabel", GetCancelButtonLabel());
  parameters.Set("managedDisclaimerText", GetManagedDisclaimerText());
  parameters.Set("interceptedAccount",
                 GetAccountInfoValue(intercepted_account()));
  parameters.Set("primaryAccount", GetAccountInfoValue(primary_account()));
  parameters.Set("interceptedProfileColor",
                 color_utils::SkColorToRgbaString(
                     bubble_parameters_.profile_highlight_color));
  parameters.Set("primaryProfileColor",
                 color_utils::SkColorToRgbaString(
                     GetProfileHighlightColor(Profile::FromWebUI(web_ui()))));
  parameters.Set("useV2Design", GetShouldUseV2Design());
  parameters.Set("showManagedDisclaimer",
                 bubble_parameters_.show_managed_disclaimer);

  parameters.Set("headerTextColor",
                 color_utils::SkColorToRgbaString(GetProfileForegroundTextColor(
                     bubble_parameters_.profile_highlight_color)));
  return parameters;
}

bool DiceWebSigninInterceptHandler::ShouldShowManagedDeviceVersion() {
  // This checks if the current profile is managed, which is a conservative
  // approximation of whether the new profile will be managed (this is because
  // the current profile may have policies coming from Sync, but the new profile
  // won't have Sync enabled, at least initially).
  // There are two possible improvements of this approximation:
  // - checking the browser policies that are not specific to this profile (e.g.
  //   by supporting the nullptr profile in BrowserManagementService)
  // - or anticipating that the user may enable Sync in the new profile and
  //   check the cloud policies attached to the intercepted account (requires
  //   network requests).
  return policy::ManagementServiceFactory::GetForProfile(
             Profile::FromWebUI(web_ui()))
             ->IsManaged() ||
         policy::ManagementServiceFactory::GetForPlatform()->IsManaged();
}

std::string DiceWebSigninInterceptHandler::GetHeaderText() {
  return (bubble_parameters_.interception_type ==
          WebSigninInterceptor::SigninInterceptionType::kProfileSwitch)
             ? intercepted_account().given_name
             : std::string();
}

std::string DiceWebSigninInterceptHandler::GetChromeSigninTitle() {
  // Set the title depending on whether the user is supervised. Note that
  // calling code waits for Account Capabilities to be fetched (with a timeout),
  // so Account Capabilities will be available for the vast majority of users.
  if (base::FeatureList::IsEnabled(
          supervised_user::kCustomProfileStringsForSupervisedUsers) &&
      bubble_parameters_.intercepted_account.capabilities
              .is_subject_to_parental_controls() == signin::Tribool::kTrue) {
    return l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_TITLE_SUPERVISED);
  }
  return l10n_util::GetStringUTF8(
      IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_TITLE);
}

std::string DiceWebSigninInterceptHandler::GetChromeSigninSubtitle() {
  // Set the subtitle depending on whether the user is supervised. Note that
  // calling code waits for Account Capabilities to be fetched (with a timeout),
  // so Account Capabilities will be available for the vast majority of users.
  if (base::FeatureList::IsEnabled(
          supervised_user::kCustomProfileStringsForSupervisedUsers) &&
      bubble_parameters_.intercepted_account.capabilities
              .is_subject_to_parental_controls() == signin::Tribool::kTrue) {
    return l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_SUBTITLE_SUPERVISED);
  }
  return l10n_util::GetStringUTF8(
      IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_SUBTITLE);
}

std::string DiceWebSigninInterceptHandler::GetBodyTitle() {
  if (bubble_parameters_.interception_type ==
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch) {
    return l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_SWITCH_BUBBLE_TITLE);
  }

  return l10n_util::GetStringUTF8(
      switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
          ? IDS_SIGNIN_DICE_WEB_INTERCEPT_CREATE_BUBBLE_TITLE_V2
          : IDS_SIGNIN_DICE_WEB_INTERCEPT_CREATE_BUBBLE_TITLE);
}

std::string DiceWebSigninInterceptHandler::GetBodyText() {
  if (bubble_parameters_.interception_type ==
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch) {
    return switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
               ? l10n_util::GetStringFUTF8(
                     IDS_SIGNIN_DICE_WEB_INTERCEPT_SWITCH_BUBBLE_DESC_V2,
                     base::UTF8ToUTF16(intercepted_account().email))
               : l10n_util::GetStringUTF8(
                     IDS_SIGNIN_DICE_WEB_INTERCEPT_SWITCH_BUBBLE_DESC);
  }

  CHECK(bubble_parameters_.interception_type ==
            WebSigninInterceptor::SigninInterceptionType::kEnterprise ||
        bubble_parameters_.interception_type ==
            WebSigninInterceptor::SigninInterceptionType::kMultiUser)
      << (bubble_parameters_.interception_type ==
                  WebSigninInterceptor::SigninInterceptionType::kChromeSignin
              ? "Chrome Signin interception strings are handled by "
                "GetInterceptionChromeSigninParametersValue()"
              : "This interception type is not handled by a bubble");

  bool enterprise_interception =
      bubble_parameters_.interception_type ==
      WebSigninInterceptor::SigninInterceptionType::kEnterprise;
  if (enterprise_interception && intercepted_account().IsEmpty()) {
    return l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_BUBBLE_DESC_MANAGED_BY_TOKEN);
  }

  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    return l10n_util::GetStringFUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_CREATE_BUBBLE_DESC,
        base::UTF8ToUTF16(primary_account().given_name),
        base::UTF8ToUTF16(intercepted_account().email));
  }

  if (enterprise_interception) {
    return ShouldShowManagedDeviceVersion()
               ? l10n_util::GetStringFUTF8(
                     IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_BUBBLE_DESC_MANAGED_DEVICE,
                     base::UTF8ToUTF16(intercepted_account().email))
               : l10n_util::GetStringFUTF8(
                     IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_BUBBLE_DESC,
                     base::UTF8ToUTF16(primary_account().email));
  }

  // `WebSigninInterceptor::SigninInterceptionType::kMultiUser` interception.
  return ShouldShowManagedDeviceVersion()
             ? l10n_util::GetStringFUTF8(
                   IDS_SIGNIN_DICE_WEB_INTERCEPT_CONSUMER_BUBBLE_DESC_MANAGED_DEVICE,
                   base::UTF8ToUTF16(primary_account().given_name),
                   base::UTF8ToUTF16(intercepted_account().email))
             : l10n_util::GetStringFUTF8(
                   IDS_SIGNIN_DICE_WEB_INTERCEPT_CONSUMER_BUBBLE_DESC,
                   base::UTF8ToUTF16(primary_account().given_name));
}

std::string DiceWebSigninInterceptHandler::GetConfirmButtonLabel() {
  if (bubble_parameters_.interception_type ==
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch) {
    return l10n_util::GetStringUTF8(
        switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
            ? IDS_SIGNIN_DICE_WEB_INTERCEPT_SWITCH_BUBBLE_CONTINUE_BUTTON_LABEL
            : IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CONFIRM_SWITCH_BUTTON_LABEL);
  }

  return l10n_util::GetStringUTF8(
      switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
          ? IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CREATE_PROFILE_BUTTON_LABEL
          : IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_NEW_PROFILE_BUTTON_LABEL);
}

std::string DiceWebSigninInterceptHandler::GetCancelButtonLabel() {
  if (bubble_parameters_.interception_type ==
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch) {
    return l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CANCEL_SWITCH_BUTTON_LABEL);
  }

  return l10n_util::GetStringUTF8(
      switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
          ? IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_STAY_HERE_BUTTON_LABEL
          : IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CANCEL_BUTTON_LABEL);
}

std::string DiceWebSigninInterceptHandler::GetManagedDisclaimerText() {
  std::string learn_more_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kSigninInterceptManagedDisclaimerLearnMoreURL),
          g_browser_process->GetApplicationLocale())
          .spec();

  if (intercepted_account().IsEmpty()) {
    return l10n_util::GetStringFUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_MANAGED_DISCLAIMER,
        base::ASCIIToUTF16(learn_more_url));
  }

  std::string manager_domain = intercepted_account().IsManaged()
                                   ? intercepted_account().hosted_domain
                                   : std::string();
  if (manager_domain.empty())
    manager_domain = chrome::GetDeviceManagerIdentity().value_or(std::string());

  if (manager_domain.empty()) {
    return l10n_util::GetStringFUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_MANAGED_DISCLAIMER,
        base::ASCIIToUTF16(learn_more_url));
  }

  return l10n_util::GetStringFUTF8(
      IDS_SIGNIN_DICE_WEB_INTERCEPT_MANAGED_BY_DISCLAIMER,
      base::ASCIIToUTF16(manager_domain), base::ASCIIToUTF16(learn_more_url));
}

bool DiceWebSigninInterceptHandler::GetShouldUseV2Design() {
  return bubble_parameters_.interception_type !=
             WebSigninInterceptor::SigninInterceptionType::kProfileSwitch;
}
