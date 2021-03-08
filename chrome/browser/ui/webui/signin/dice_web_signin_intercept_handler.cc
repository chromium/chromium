// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_handler.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace {

// Returns true if the account is managed (aka Enterprise, or Dasher).
bool IsManaged(const AccountInfo& info) {
  return info.hosted_domain != kNoHostedDomainFound;
}

}  // namespace

DiceWebSigninInterceptHandler::DiceWebSigninInterceptHandler(
    const DiceWebSigninInterceptor::Delegate::BubbleParameters&
        bubble_parameters,
    base::OnceCallback<void(SigninInterceptionUserChoice)> callback)
    : bubble_parameters_(bubble_parameters), callback_(std::move(callback)) {
  DCHECK(callback_);
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
      "guest", base::BindRepeating(&DiceWebSigninInterceptHandler::HandleGuest,
                                   base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "pageLoaded",
      base::BindRepeating(&DiceWebSigninInterceptHandler::HandlePageLoaded,
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

void DiceWebSigninInterceptHandler::HandleAccept(const base::ListValue* args) {
  if (callback_)
    std::move(callback_).Run(SigninInterceptionUserChoice::kAccept);
}

void DiceWebSigninInterceptHandler::HandleCancel(const base::ListValue* args) {
  if (callback_)
    std::move(callback_).Run(SigninInterceptionUserChoice::kDecline);
}

void DiceWebSigninInterceptHandler::HandleGuest(const base::ListValue* args) {
  if (callback_)
    std::move(callback_).Run(SigninInterceptionUserChoice::kGuest);
}

void DiceWebSigninInterceptHandler::HandlePageLoaded(
    const base::ListValue* args) {
  AllowJavascript();

  // Update the account info and the images.
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  base::Optional<AccountInfo> updated_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          intercepted_account());
  if (updated_info)
    bubble_parameters_.intercepted_account = updated_info.value();
  updated_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          primary_account());
  if (updated_info)
    bubble_parameters_.primary_account = updated_info.value();

  // If there is no extended info for the primary account, populate with
  // reasonable defaults.
  if (primary_account().hosted_domain.empty())
    bubble_parameters_.primary_account.hosted_domain = kNoHostedDomainFound;
  if (primary_account().given_name.empty())
    bubble_parameters_.primary_account.given_name = primary_account().email;

  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, GetInterceptionParametersValue());
}

base::Value DiceWebSigninInterceptHandler::GetAccountInfoValue(
    const AccountInfo& info) {
  std::string picture_url_to_load =
      info.account_image.IsEmpty()
          ? profiles::GetPlaceholderAvatarIconUrl()
          : webui::GetBitmapDataUrl(info.account_image.AsBitmap());
  base::Value account_info_value(base::Value::Type::DICTIONARY);
  account_info_value.SetBoolKey("isManaged", IsManaged(info));
  account_info_value.SetStringKey("pictureUrl", picture_url_to_load);
  return account_info_value;
}

base::Value DiceWebSigninInterceptHandler::GetInterceptionParametersValue() {
  bool is_switch =
      bubble_parameters_.interception_type ==
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch;
  int confirmButtonStringID =
      is_switch
          ? IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CONFIRM_SWITCH_BUTTON_LABEL
          : IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_NEW_PROFILE_BUTTON_LABEL;
  int cancelButtonStringID =
      is_switch
          ? IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CANCEL_SWITCH_BUTTON_LABEL
          : IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CANCEL_BUTTON_LABEL;
  base::Value parameters(base::Value::Type::DICTIONARY);
  parameters.SetStringKey("headerText", GetHeaderText());
  parameters.SetStringKey("bodyTitle", GetBodyTitle());
  parameters.SetStringKey("bodyText", GetBodyText());
  parameters.SetStringKey("confirmButtonLabel",
                          l10n_util::GetStringUTF8(confirmButtonStringID));
  parameters.SetStringKey("cancelButtonLabel",
                          l10n_util::GetStringUTF8(cancelButtonStringID));
  parameters.SetBoolKey("showGuestOption",
                        bubble_parameters_.show_guest_option);
  parameters.SetKey("interceptedAccount",
                    GetAccountInfoValue(intercepted_account()));
  parameters.SetStringKey("headerBackgroundColor",
                          color_utils::SkColorToRgbaString(
                              bubble_parameters_.profile_highlight_color));
  parameters.SetStringKey(
      "headerTextColor",
      color_utils::SkColorToRgbaString(GetProfileForegroundTextColor(
          bubble_parameters_.profile_highlight_color)));
  return parameters;
}

bool DiceWebSigninInterceptHandler::ShouldShowManagedDeviceVersion() const {
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
  return policy::PlatformManagementService::GetInstance()
                 .GetManagementAuthorityTrustworthiness() >
             policy::ManagementAuthorityTrustworthiness::NONE ||
         policy::BrowserManagementService(Profile::FromWebUI(web_ui()))
                 .GetManagementAuthorityTrustworthiness() >
             policy::ManagementAuthorityTrustworthiness::NONE;
}

std::string DiceWebSigninInterceptHandler::GetHeaderText() {
  switch (bubble_parameters_.interception_type) {
    case DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise:
      return IsManaged(intercepted_account())
                 ? intercepted_account().hosted_domain
                 : intercepted_account().given_name;
    case DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser:
    case DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch:
      return intercepted_account().given_name;
  }
}

std::string DiceWebSigninInterceptHandler::GetBodyTitle() {
  if (bubble_parameters_.interception_type ==
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch) {
    return l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_SWITCH_BUBBLE_TITLE);
  }

  // For profile creations, the title is controlled by an experiment. Expected
  // values for the parameter are 1, 2 or 3.
  // The version 3 is specific to the "consumer" bubble and is not supported by
  // the enterprise bubble (which defaults to version 1 in that case).
  int string_version = base::GetFieldTrialParamByFeatureAsInt(
      kDiceWebSigninInterceptionFeature, "title_version",
      /*default_value=*/1);

  int string_id = IDS_SIGNIN_DICE_WEB_INTERCEPT_CREATE_BUBBLE_TITLE_V1;
  switch (string_version) {
    case 2:
      string_id = IDS_SIGNIN_DICE_WEB_INTERCEPT_CREATE_BUBBLE_TITLE_V2;
      break;
    case 3:
      // Only use version 3 for consumer bubble.
      if (bubble_parameters_.interception_type ==
          DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser) {
        string_id = IDS_SIGNIN_DICE_WEB_INTERCEPT_CREATE_BUBBLE_TITLE_V3;
      }
      break;
    default:
      // For default or invalid parameters, there is nothing to do.
      break;
  }

  return l10n_util::GetStringUTF8(string_id);
}

std::string DiceWebSigninInterceptHandler::GetBodyText() {
  switch (bubble_parameters_.interception_type) {
    case DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise:
      return ShouldShowManagedDeviceVersion()
                 ? l10n_util::GetStringFUTF8(
                       IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_BUBBLE_DESC_MANAGED_DEVICE,
                       base::UTF8ToUTF16(intercepted_account().email))
                 : l10n_util::GetStringFUTF8(
                       IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_BUBBLE_DESC,
                       base::UTF8ToUTF16(primary_account().email));
    case DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser:
      return ShouldShowManagedDeviceVersion()
                 ? l10n_util::GetStringFUTF8(
                       IDS_SIGNIN_DICE_WEB_INTERCEPT_CONSUMER_BUBBLE_DESC_MANAGED_DEVICE,
                       base::UTF8ToUTF16(primary_account().given_name),
                       base::UTF8ToUTF16(intercepted_account().email))
                 : l10n_util::GetStringFUTF8(
                       IDS_SIGNIN_DICE_WEB_INTERCEPT_CONSUMER_BUBBLE_DESC,
                       base::UTF8ToUTF16(primary_account().given_name));
    case DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch:
      return l10n_util::GetStringUTF8(
          IDS_SIGNIN_DICE_WEB_INTERCEPT_SWITCH_BUBBLE_DESC);
  }
}
