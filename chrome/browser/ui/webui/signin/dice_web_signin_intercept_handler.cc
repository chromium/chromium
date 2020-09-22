// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_handler.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
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
    base::OnceCallback<void(bool)> callback)
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
      "pageLoaded",
      base::BindRepeating(&DiceWebSigninInterceptHandler::HandlePageLoaded,
                          base::Unretained(this)));
}

void DiceWebSigninInterceptHandler::OnJavascriptAllowed() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()));
  identity_observer_.Add(identity_manager);
}

void DiceWebSigninInterceptHandler::OnJavascriptDisallowed() {
  identity_observer_.RemoveAll();
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
    std::move(callback_).Run(true);
}

void DiceWebSigninInterceptHandler::HandleCancel(const base::ListValue* args) {
  if (callback_)
    std::move(callback_).Run(false);
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
  if (primary_account().given_name.empty()) {
    ProfileAttributesEntry* entry = nullptr;
    g_browser_process->profile_manager()
        ->GetProfileAttributesStorage()
        .GetProfileAttributesWithPath(profile->GetPath(), &entry);
    bubble_parameters_.primary_account.given_name =
        base::UTF16ToUTF8(entry->GetName());
  }

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
  base::Value parameters(base::Value::Type::DICTIONARY);
  parameters.SetStringKey("headerText", GetHeaderText());
  parameters.SetStringKey("bodyTitle", GetBodyTitle());
  parameters.SetStringKey("bodyText", GetBodyText());
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
  switch (bubble_parameters_.interception_type) {
    case DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise:
      if (!IsManaged(primary_account())) {
        return l10n_util::GetStringUTF8(
            IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_BUBBLE_TITLE);
      }
      FALLTHROUGH;
    case DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser:
      return l10n_util::GetStringUTF8(
          IDS_SIGNIN_DICE_WEB_INTERCEPT_CONSUMER_BUBBLE_TITLE);
    case DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch:
      // TODO: use localized string once it's available.
      return "Switch profile";
  }
}

std::string DiceWebSigninInterceptHandler::GetBodyText() {
  switch (bubble_parameters_.interception_type) {
    case DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise:
      if (IsManaged(intercepted_account()) && IsManaged(primary_account())) {
        return l10n_util::GetStringFUTF8(
            IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_ENTERPRISE_BUBBLE_DESC,
            base::UTF8ToUTF16(intercepted_account().hosted_domain),
            base::UTF8ToUTF16(primary_account().hosted_domain));
      } else if (IsManaged(intercepted_account())) {
        return l10n_util::GetStringFUTF8(
            IDS_SIGNIN_DICE_WEB_INTERCEPT_ENTERPRISE_CONSUMER_BUBBLE_DESC,
            base::UTF8ToUTF16(intercepted_account().hosted_domain));
      } else {
        return l10n_util::GetStringFUTF8(
            IDS_SIGNIN_DICE_WEB_INTERCEPT_CONSUMER_ENTERPRISE_BUBBLE_DESC,
            base::UTF8ToUTF16(intercepted_account().given_name),
            base::UTF8ToUTF16(primary_account().hosted_domain));
      }
    case DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser:
      return l10n_util::GetStringFUTF8(
          IDS_SIGNIN_DICE_WEB_INTERCEPT_CONSUMER_BUBBLE_DESC,
          base::UTF8ToUTF16(intercepted_account().given_name));
    case DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch:
      // TODO: use localized string once it's available.
      return base::StringPrintf(
          "This account is already signed in in a different profile. Would "
          "you like to switch to %s's profile?",
          intercepted_account().given_name.c_str());
  }
}
