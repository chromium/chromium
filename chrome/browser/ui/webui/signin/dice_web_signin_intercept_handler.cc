// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_handler.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_ui.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
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

SkColor GetProfileHighlightColor(Profile* profile) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  DCHECK(entry);

  return entry->GetProfileThemeColors().profile_highlight_color;
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

void DiceWebSigninInterceptHandler::HandleAccept(
    const base::Value::List& args) {
  if (callback_)
    std::move(callback_).Run(SigninInterceptionUserChoice::kAccept);
}

void DiceWebSigninInterceptHandler::HandleCancel(
    const base::Value::List& args) {
  if (callback_)
    std::move(callback_).Run(SigninInterceptionUserChoice::kDecline);
}

void DiceWebSigninInterceptHandler::HandleGuest(const base::Value::List& args) {
  if (callback_)
    std::move(callback_).Run(SigninInterceptionUserChoice::kGuest);
}

void DiceWebSigninInterceptHandler::HandlePageLoaded(
    const base::Value::List& args) {
  AllowJavascript();

  // Update the account info and the images.
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo updated_info =
      identity_manager->FindExtendedAccountInfo(intercepted_account());
  if (!updated_info.IsEmpty())
    bubble_parameters_.intercepted_account = updated_info;
  updated_info = identity_manager->FindExtendedAccountInfo(primary_account());
  if (!updated_info.IsEmpty())
    bubble_parameters_.primary_account = updated_info;

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

base::Value::Dict DiceWebSigninInterceptHandler::GetAccountInfoValue(
    const AccountInfo& info) {
  std::string picture_url_to_load =
      info.account_image.IsEmpty()
          ? profiles::GetPlaceholderAvatarIconUrl()
          : webui::GetBitmapDataUrl(info.account_image.AsBitmap());
  base::Value::Dict account_info_value;
  account_info_value.Set("isManaged", IsManaged(info));
  account_info_value.Set("pictureUrl", picture_url_to_load);
  return account_info_value;
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
  parameters.Set("showGuestOption", bubble_parameters_.show_guest_option);
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
  if (bubble_parameters_.interception_type ==
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch) {
    return intercepted_account().given_name;
  }

  if (base::FeatureList::IsEnabled(kSigninInterceptBubbleV2))
    return std::string();

  if (bubble_parameters_.interception_type ==
          DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise &&
      IsManaged(intercepted_account())) {
    return intercepted_account().hosted_domain;
  }

  return intercepted_account().given_name;
}

std::string DiceWebSigninInterceptHandler::GetBodyTitle() {
  if (bubble_parameters_.interception_type ==
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch) {
    return l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_SWITCH_BUBBLE_TITLE);
  }

  if (base::FeatureList::IsEnabled(kSigninInterceptBubbleV2)) {
    return l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_CREATE_BUBBLE_TITLE_V2);
  }

  return l10n_util::GetStringUTF8(
      IDS_SIGNIN_DICE_WEB_INTERCEPT_CREATE_BUBBLE_TITLE);
}

std::string DiceWebSigninInterceptHandler::GetBodyText() {
  if (bubble_parameters_.interception_type ==
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch) {
    return l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_SWITCH_BUBBLE_DESC);
  }

  if (base::FeatureList::IsEnabled(kSigninInterceptBubbleV2)) {
    return l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_CONSUMER_BUBBLE_DESC_V2);
  }

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
      // Already handled.
    case DiceWebSigninInterceptor::SigninInterceptionType::
        kEnterpriseAcceptManagement:
    case DiceWebSigninInterceptor::SigninInterceptionType::kEnterpriseForced:
    case DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitchForced:
      NOTREACHED() << "This interception type is not handled by a bubble";
      return std::string();
  }
}

std::string DiceWebSigninInterceptHandler::GetConfirmButtonLabel() {
  if (bubble_parameters_.interception_type ==
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch) {
    return l10n_util::GetStringUTF8(
        IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CONFIRM_SWITCH_BUTTON_LABEL);
  }

  return l10n_util::GetStringUTF8(
      base::FeatureList::IsEnabled(kSigninInterceptBubbleV2)
          ? IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_NEW_PROFILE_BUTTON_LABEL_V2
          : IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_NEW_PROFILE_BUTTON_LABEL);
}

std::string DiceWebSigninInterceptHandler::GetCancelButtonLabel() {
  return l10n_util::GetStringUTF8(
      bubble_parameters_.interception_type ==
              DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch
          ? IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CANCEL_SWITCH_BUTTON_LABEL
          : IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CANCEL_BUTTON_LABEL);
}

std::string DiceWebSigninInterceptHandler::GetManagedDisclaimerText() {
  std::string learn_more_url =
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kSigninInterceptManagedDisclaimerLearnMoreURL),
          g_browser_process->GetApplicationLocale())
          .spec();
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
  if (bubble_parameters_.interception_type ==
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch)
    return false;

  return base::FeatureList::IsEnabled(kSigninInterceptBubbleV2);
}
