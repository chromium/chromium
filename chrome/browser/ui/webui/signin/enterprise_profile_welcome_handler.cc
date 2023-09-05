// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_handler.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/native_theme/native_theme.h"

namespace {
const int kAvatarSize = 100;

std::string GetManagedAccountTitle(ProfileAttributesEntry* entry,
                                   const std::string& account_domain_name) {
  DCHECK(entry);
  if (entry->GetHostedDomain() == kNoHostedDomainFound)
    return std::string();
  const std::string domain_name = entry->GetHostedDomain().empty()
                                      ? account_domain_name
                                      : entry->GetHostedDomain();
  return l10n_util::GetStringFUTF8(
      IDS_ENTERPRISE_PROFILE_WELCOME_ACCOUNT_MANAGED_BY,
      base::UTF8ToUTF16(domain_name));
}

std::string GetManagedAccountTitleWithEmail(
    Profile* profile,
    ProfileAttributesEntry* entry,
    const std::string& account_domain_name,
    const std::u16string& email) {
  DCHECK(profile);
  DCHECK(entry);
  DCHECK(!email.empty());

#if !BUILDFLAG(IS_CHROMEOS)
  absl::optional<std::string> account_manager =
      chrome::GetAccountManagerIdentity(profile);

  if (!signin_util::IsProfileSeparationEnforcedByProfile(profile)) {
    // The profile is managed but does not enforce profile separation. The
    // intercepted account requires it.
    if (account_manager && !account_manager->empty()) {
      return l10n_util::GetStringFUTF8(
          IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_MANAGED_SEPARATION,
          base::UTF8ToUTF16(*account_manager), email,
          base::UTF8ToUTF16(account_domain_name));
    }
    // The profile is not managed. The intercepted account requires profile
    // separation.
    return l10n_util::GetStringFUTF8(
        IDS_ENTERPRISE_PROFILE_WELCOME_ACCOUNT_EMAIL_MANAGED_BY, email,
        base::UTF8ToUTF16(account_domain_name));
  } else if (profile->GetPrefs()->GetBoolean(
                 prefs::kManagedAccountsSigninRestrictionScopeMachine)) {
    // The device is managed and requires profile separation.
    absl::optional<std::string> device_manager =
        chrome::GetDeviceManagerIdentity();
    if (device_manager && !device_manager->empty()) {
      return l10n_util::GetStringFUTF8(
          IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_SEPARATION_DEVICE_MANAGED_BY,
          base::UTF8ToUTF16(*device_manager), email);
    } else {
      return l10n_util::GetStringFUTF8(
          IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_SEPARATION_DEVICE_MANAGED,
          email);
    }
  } else {
    DCHECK(account_manager);
    DCHECK(!account_manager->empty());
    return l10n_util::GetStringFUTF8(
        IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_MANAGED_STRICT_SEPARATION,
        base::UTF8ToUTF16(*account_manager), email);
  }
#else
  if (entry->GetHostedDomain() == kNoHostedDomainFound)
    return std::string();
  const std::string domain_name = entry->GetHostedDomain().empty()
                                      ? account_domain_name
                                      : entry->GetHostedDomain();
  return l10n_util::GetStringFUTF8(
      IDS_ENTERPRISE_PROFILE_WELCOME_ACCOUNT_EMAIL_MANAGED_BY, email,
      base::UTF8ToUTF16(domain_name));
#endif  //  !BUILDFLAG(IS_CHROMEOS)
}

std::string GetManagedDeviceTitle() {
  absl::optional<std::string> device_manager =
      chrome::GetDeviceManagerIdentity();
  if (!device_manager)
    return std::string();
  if (device_manager->empty()) {
    return l10n_util::GetStringUTF8(
        IDS_ENTERPRISE_PROFILE_WELCOME_DEVICE_MANAGED);
  }
  return l10n_util::GetStringFUTF8(
      IDS_ENTERPRISE_PROFILE_WELCOME_DEVICE_MANAGED_BY,
      base::UTF8ToUTF16(*device_manager));
}

}  // namespace

EnterpriseProfileWelcomeHandler::EnterpriseProfileWelcomeHandler(
    Browser* browser,
    EnterpriseProfileWelcomeUI::ScreenType type,
    bool profile_creation_required_by_policy,
    bool show_link_data_option,
    const AccountInfo& account_info,
    signin::SigninChoiceCallback proceed_callback)
    : browser_(browser),
      type_(type),
      profile_creation_required_by_policy_(profile_creation_required_by_policy),
#if !BUILDFLAG(IS_CHROMEOS)
      show_link_data_option_(show_link_data_option),
#endif
      email_(base::UTF8ToUTF16(account_info.email)),
      domain_name_(gaia::ExtractDomainName(account_info.email)),
      account_id_(account_info.account_id),
      proceed_callback_(std::move(proceed_callback)) {
  DCHECK(proceed_callback_);
  DCHECK(
      browser_ ||
      type_ !=
          EnterpriseProfileWelcomeUI::ScreenType::kEnterpriseAccountCreation);
  BrowserList::AddObserver(this);
}

EnterpriseProfileWelcomeHandler::~EnterpriseProfileWelcomeHandler() {
  BrowserList::RemoveObserver(this);
  HandleCancel(base::Value::List());
}

void EnterpriseProfileWelcomeHandler::RegisterMessages() {
  profile_path_ = Profile::FromWebUI(web_ui())->GetPath();
  web_ui()->RegisterMessageCallback(
      "initialized",
      base::BindRepeating(&EnterpriseProfileWelcomeHandler::HandleInitialized,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initializedWithSize",
      base::BindRepeating(
          &EnterpriseProfileWelcomeHandler::HandleInitializedWithSize,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "proceed",
      base::BindRepeating(&EnterpriseProfileWelcomeHandler::HandleProceed,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancel",
      base::BindRepeating(&EnterpriseProfileWelcomeHandler::HandleCancel,
                          base::Unretained(this)));
}

void EnterpriseProfileWelcomeHandler::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  UpdateProfileInfo(profile_path);
}

void EnterpriseProfileWelcomeHandler::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  UpdateProfileInfo(profile_path);
}

void EnterpriseProfileWelcomeHandler::OnProfileHostedDomainChanged(
    const base::FilePath& profile_path) {
  UpdateProfileInfo(profile_path);
}

void EnterpriseProfileWelcomeHandler::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    browser_ = nullptr;
}

void EnterpriseProfileWelcomeHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.account_id == account_id_ && !info.account_image.IsEmpty()) {
    UpdateProfileInfo(profile_path_);
    observed_account_.Reset();
  }
}

void EnterpriseProfileWelcomeHandler::OnJavascriptAllowed() {
  if (type_ !=
      EnterpriseProfileWelcomeUI::ScreenType::kEnterpriseAccountCreation) {
    observed_profile_.Observe(
        &g_browser_process->profile_manager()->GetProfileAttributesStorage());
  } else {
    observed_account_.Observe(
        IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui())));
  }
}

void EnterpriseProfileWelcomeHandler::OnJavascriptDisallowed() {
  observed_profile_.Reset();
  observed_account_.Reset();
}

void EnterpriseProfileWelcomeHandler::HandleInitialized(
    const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, GetProfileInfoValue());
}

void EnterpriseProfileWelcomeHandler::HandleInitializedWithSize(
    const base::Value::List& args) {
  AllowJavascript();

  if (browser_)
    signin::SetInitializedModalHeight(browser_, web_ui(), args);
}

void EnterpriseProfileWelcomeHandler::HandleProceed(
    const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  if (proceed_callback_) {
    bool use_existing_profile = args[0].GetIfBool().value_or(false);
    std::move(proceed_callback_)
        .Run(use_existing_profile ? signin::SIGNIN_CHOICE_CONTINUE
                                  : signin::SIGNIN_CHOICE_NEW_PROFILE);
  }
}

void EnterpriseProfileWelcomeHandler::HandleCancel(
    const base::Value::List& args) {
  if (proceed_callback_)
    std::move(proceed_callback_).Run(signin::SIGNIN_CHOICE_CANCEL);
}

void EnterpriseProfileWelcomeHandler::UpdateProfileInfo(
    const base::FilePath& profile_path) {
  DCHECK(IsJavascriptAllowed());
  if (profile_path != profile_path_)
    return;
  FireWebUIListener("on-profile-info-changed", GetProfileInfoValue());
}

base::Value::Dict EnterpriseProfileWelcomeHandler::GetProfileInfoValue() {
  base::Value::Dict dict;
  dict.Set("pictureUrl", GetPictureUrl());

  std::string title =
      l10n_util::GetStringUTF8(IDS_ENTERPRISE_PROFILE_WELCOME_TITLE);
  std::string subtitle;
  std::string enterprise_info;
  bool show_cancel_button = true;
  ProfileAttributesEntry* entry = GetProfileEntry();

  switch (type_) {
    case EnterpriseProfileWelcomeUI::ScreenType::kEntepriseAccountSyncEnabled:
      dict.Set("showEnterpriseBadge", true);
      subtitle = GetManagedAccountTitle(entry, domain_name_);
      enterprise_info = l10n_util::GetStringUTF8(
          IDS_ENTERPRISE_PROFILE_WELCOME_MANAGED_DESCRIPTION_WITH_SYNC);
      dict.Set("proceedLabel", l10n_util::GetStringUTF8(
                                   IDS_PROFILE_PICKER_IPH_NEXT_BUTTON_LABEL));
      break;
    case EnterpriseProfileWelcomeUI::ScreenType::kEntepriseAccountSyncDisabled:
      dict.Set("showEnterpriseBadge", true);
      subtitle = GetManagedAccountTitle(entry, domain_name_);
      enterprise_info = l10n_util ::GetStringUTF8(
          IDS_ENTERPRISE_PROFILE_WELCOME_MANAGED_DESCRIPTION_WITHOUT_SYNC);
      dict.Set("proceedLabel", l10n_util::GetStringUTF8(IDS_DONE));
      break;
    case EnterpriseProfileWelcomeUI::ScreenType::kConsumerAccountSyncDisabled:
      dict.Set("showEnterpriseBadge", false);
      subtitle = GetManagedDeviceTitle();
      enterprise_info =
          l10n_util::GetStringUTF8(IDS_SYNC_DISABLED_CONFIRMATION_DETAILS);
      dict.Set("proceedLabel", l10n_util::GetStringUTF8(IDS_DONE));
      break;
    case EnterpriseProfileWelcomeUI::ScreenType::kEnterpriseAccountCreation:
      title = l10n_util::GetStringUTF8(
          profile_creation_required_by_policy_
              ? IDS_ENTERPRISE_WELCOME_PROFILE_REQUIRED_TITLE
              : IDS_ENTERPRISE_WELCOME_PROFILE_WILL_BE_MANAGED_TITLE);
      dict.Set("showEnterpriseBadge", false);
      subtitle = GetManagedAccountTitleWithEmail(Profile::FromWebUI(web_ui()),
                                                 entry, domain_name_, email_);
      enterprise_info = l10n_util::GetStringUTF8(
          IDS_ENTERPRISE_PROFILE_WELCOME_MANAGED_DESCRIPTION_WITH_SYNC);
      dict.Set("proceedLabel",
               l10n_util::GetStringUTF8(
                   profile_creation_required_by_policy_
                       ? IDS_ENTERPRISE_PROFILE_WELCOME_CREATE_PROFILE_BUTTON
                       : IDS_WELCOME_SIGNIN_VIEW_SIGNIN));
#if !BUILDFLAG(IS_CHROMEOS)
      dict.Set("checkLinkDataCheckboxByDefault",
               show_link_data_option_ &&
                   g_browser_process->local_state()->GetBoolean(
                       prefs::kEnterpriseProfileCreationKeepBrowsingData));
#endif
      break;
  }

  dict.Set("title", title);
  dict.Set("subtitle", subtitle);
  dict.Set("enterpriseInfo", enterprise_info);
  dict.Set("showCancelButton", show_cancel_button);

  return dict;
}

ProfileAttributesEntry* EnterpriseProfileWelcomeHandler::GetProfileEntry()
    const {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path_);
  DCHECK(entry);
  return entry;
}

std::string EnterpriseProfileWelcomeHandler::GetPictureUrl() {
  absl::optional<gfx::Image> icon;
  if (type_ ==
      EnterpriseProfileWelcomeUI::ScreenType::kEnterpriseAccountCreation) {
    AccountInfo account_info =
        IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
            ->FindExtendedAccountInfoByAccountId(account_id_);
    DCHECK(!account_info.IsEmpty());
    icon = account_info.account_image.IsEmpty()
               ? ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                     profiles::GetPlaceholderAvatarIconResourceID())
               : account_info.account_image;
  }

  const int avatar_icon_size = kAvatarSize * web_ui()->GetDeviceScaleFactor();
  return webui::GetBitmapDataUrl(
      profiles::GetSizedAvatarIcon(
          icon.value_or(GetProfileEntry()->GetAvatarIcon(avatar_icon_size)),
          avatar_icon_size, avatar_icon_size)
          .AsBitmap());
}

EnterpriseProfileWelcomeUI::ScreenType
EnterpriseProfileWelcomeHandler::GetTypeForTesting() {
  return type_;
}

void EnterpriseProfileWelcomeHandler::CallProceedCallbackForTesting(
    signin::SigninChoice choice) {
  if (proceed_callback_)
    std::move(proceed_callback_).Run(choice);
}
