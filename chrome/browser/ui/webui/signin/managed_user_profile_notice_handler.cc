// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_handler.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
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
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "base/feature_list.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#endif

namespace {
const int kAvatarSize = 100;
constexpr base::TimeDelta kLongProcessingThreshold = base::Seconds(5);

bool UseMultiscreen() {
#if BUILDFLAG(IS_CHROMEOS)
  return false;
#else
  return base::FeatureList::IsEnabled(
             profile_management::features::kOidcAuthProfileManagement) ||
         base::FeatureList::IsEnabled(
             features::kEnterpriseUpdatedProfileCreationScreen);
#endif
}

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

std::string GetManagedDeviceTitle() {
  std::optional<std::string> device_manager =
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

ManagedUserProfileNoticeHandler::ManagedUserProfileNoticeHandler(
    Browser* browser,
    ManagedUserProfileNoticeUI::ScreenType type,
    std::unique_ptr<signin::EnterpriseProfileCreationDialogParams> create_param)
    : browser_(browser),
      type_(type),
      profile_creation_required_by_policy_(
          create_param->profile_creation_required_by_policy),
#if !BUILDFLAG(IS_CHROMEOS)
      show_link_data_option_(create_param->show_link_data_option),
#endif
      email_(create_param->is_oidc_account
                 ? std::u16string()
                 : base::UTF8ToUTF16(create_param->account_info.email)),
      domain_name_(
          create_param->is_oidc_account
              ? std::string()
              : gaia::ExtractDomainName(create_param->account_info.email)),
      account_id_(create_param->account_info.account_id),
      done_callback_(std::move(create_param->done_callback)),
      retry_callback_(std::move(create_param->retry_callback)) {
  if (std::holds_alternative<signin::SigninChoiceWithConfirmationCallback>(
          create_param->process_user_choice_callback)) {
    process_user_choice_with_confirmation_callback_ =
        std::move(std::get<signin::SigninChoiceWithConfirmationCallback>(
            create_param->process_user_choice_callback));
    CHECK(process_user_choice_with_confirmation_callback_);
  }
  if (std::holds_alternative<signin::SigninChoiceCallback>(
          create_param->process_user_choice_callback)) {
    CHECK(std::get<signin::SigninChoiceCallback>(
        create_param->process_user_choice_callback));
    process_user_choice_with_confirmation_callback_ = base::BindOnce(
        [](signin::SigninChoiceCallback callback, signin::SigninChoice choice,
           signin::SigninChoiceOperationDoneCallback done) {
          std::move(callback).Run(choice);
          std::move(done).Run(
              signin::SigninChoiceOperationResult::SIGNIN_SILENT_SUCCESS);
        },
        std::move(std::get<signin::SigninChoiceCallback>(
            create_param->process_user_choice_callback)));
  }
  CHECK(browser_ ||
        type_ !=
            ManagedUserProfileNoticeUI::ScreenType::kEnterpriseAccountCreation);
  BrowserList::AddObserver(this);
}

ManagedUserProfileNoticeHandler::~ManagedUserProfileNoticeHandler() {
  BrowserList::RemoveObserver(this);
  if (!canceling_) {
    HandleCancel(base::Value::List());
  }
}

void ManagedUserProfileNoticeHandler::RegisterMessages() {
  profile_path_ = Profile::FromWebUI(web_ui())->GetPath();
  web_ui()->RegisterMessageCallback(
      "initialized",
      base::BindRepeating(&ManagedUserProfileNoticeHandler::HandleInitialized,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initializedWithSize",
      base::BindRepeating(
          &ManagedUserProfileNoticeHandler::HandleInitializedWithSize,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "proceed",
      base::BindRepeating(&ManagedUserProfileNoticeHandler::HandleProceed,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancel",
      base::BindRepeating(&ManagedUserProfileNoticeHandler::HandleCancel,
                          base::Unretained(this)));
}

void ManagedUserProfileNoticeHandler::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  UpdateProfileInfo(profile_path);
}

void ManagedUserProfileNoticeHandler::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  UpdateProfileInfo(profile_path);
}

void ManagedUserProfileNoticeHandler::OnProfileHostedDomainChanged(
    const base::FilePath& profile_path) {
  UpdateProfileInfo(profile_path);
}

void ManagedUserProfileNoticeHandler::OnBrowserRemoved(Browser* browser) {
  if (browser_ == browser)
    browser_ = nullptr;
}

void ManagedUserProfileNoticeHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.account_id == account_id_ && !info.account_image.IsEmpty()) {
    UpdateProfileInfo(profile_path_);
    observed_account_.Reset();
  }
}

void ManagedUserProfileNoticeHandler::OnJavascriptAllowed() {
  if (type_ !=
      ManagedUserProfileNoticeUI::ScreenType::kEnterpriseAccountCreation) {
    observed_profile_.Observe(
        &g_browser_process->profile_manager()->GetProfileAttributesStorage());
  } else {
    observed_account_.Observe(
        IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui())));
  }
}

void ManagedUserProfileNoticeHandler::OnJavascriptDisallowed() {
  observed_profile_.Reset();
  observed_account_.Reset();
}

void ManagedUserProfileNoticeHandler::HandleInitialized(
    const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, GetProfileInfoValue());
}

void ManagedUserProfileNoticeHandler::HandleInitializedWithSize(
    const base::Value::List& args) {
  AllowJavascript();

  if (browser_)
    signin::SetInitializedModalHeight(browser_, web_ui(), args);
}

void ManagedUserProfileNoticeHandler::HandleProceed(
    const base::Value::List& args) {
  CHECK_EQ(2u, args.size());
  AllowJavascript();
  bool use_existing_profile = args[1].GetIfBool().value_or(false);
  auto result = use_existing_profile ? signin::SIGNIN_CHOICE_CONTINUE
                                     : signin::SIGNIN_CHOICE_NEW_PROFILE;

  int state = args[0].GetIfInt().value_or(0);
  CHECK_NE(state, ManagedUserProfileNoticeHandler::State::kProcessing)
      << "User should not be able to click the proceed button while processing";
  if (state == ManagedUserProfileNoticeHandler::State::kValueProposition &&
      IsJavascriptAllowed()) {
    FireWebUIListener("on-state-changed",
                      ManagedUserProfileNoticeHandler::State::kDisclosure);
    return;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  if (show_link_data_option_ &&
      state == ManagedUserProfileNoticeHandler::State::kDisclosure &&
      IsJavascriptAllowed() && UseMultiscreen()) {
    FireWebUIListener(
        "on-state-changed",
        ManagedUserProfileNoticeHandler::State::kUserDataHandling);
    return;
  }
#endif
  if (process_user_choice_with_confirmation_callback_ &&
      state == ManagedUserProfileNoticeHandler::State::kDisclosure &&
      IsJavascriptAllowed()) {
    if (type_ == ManagedUserProfileNoticeUI::ScreenType::kEnterpriseOIDC) {
      processing_timer_.Start(
          FROM_HERE, kLongProcessingThreshold,
          base::BindOnce(&ManagedUserProfileNoticeHandler::OnLongProcessingTime,
                         base::Unretained(this)));
    }

    FireWebUIListener("on-state-changed",
                      ManagedUserProfileNoticeHandler::State::kProcessing);
  }

  if (process_user_choice_with_confirmation_callback_) {
    std::move(process_user_choice_with_confirmation_callback_)
        .Run(result, base::BindOnce(
                         &ManagedUserProfileNoticeHandler::OnUserChoiceHandled,
                         weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  if (done_callback_) {
    DisallowJavascript();
    std::move(done_callback_).Run();
  }
}

void ManagedUserProfileNoticeHandler::HandleCancel(
    const base::Value::List& args) {
  canceling_ = true;
  // Move the `done_callback_` here to avoid it being potentially destroyed
  // by `process_user_choice_with_confirmation_callback_` since it may destroy
  // `this`.
  if (IsJavascriptAllowed()) {
    DisallowJavascript();
  }
  auto done_callback = std::move(done_callback_);
  if (process_user_choice_with_confirmation_callback_) {
    std::move(process_user_choice_with_confirmation_callback_)
        .Run(signin::SIGNIN_CHOICE_CANCEL, base::DoNothing());
  }
  if (done_callback) {
    std::move(done_callback).Run();
  }
}

void ManagedUserProfileNoticeHandler::OnLongProcessingTime() {
  FireWebUIListener("on-long-processing");
}

void ManagedUserProfileNoticeHandler::UpdateProfileInfo(
    const base::FilePath& profile_path) {
  DCHECK(IsJavascriptAllowed());
  if (profile_path != profile_path_)
    return;
  FireWebUIListener("on-profile-info-changed", GetProfileInfoValue());
}

// static
std::string ManagedUserProfileNoticeHandler::GetManagedAccountTitleWithEmail(
    Profile* profile,
    ProfileAttributesEntry* entry,
    const std::string& account_domain_name,
    const std::u16string& email) {
  DCHECK(profile);
  DCHECK(entry);
  DCHECK(!email.empty());

#if !BUILDFLAG(IS_CHROMEOS)
  std::optional<std::string> account_manager =
      chrome::GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager =
      chrome::GetDeviceManagerIdentity();

  if (!signin_util::IsProfileSeparationEnforcedByProfile(
          profile, base::UTF16ToUTF8(email))) {
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
  }
  if (!profile->GetPrefs()->GetBoolean(
          prefs::kManagedAccountsSigninRestrictionScopeMachine) &&
      account_manager && !account_manager->empty()) {
    return l10n_util::GetStringFUTF8(
        IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_MANAGED_STRICT_SEPARATION,
        base::UTF8ToUTF16(*account_manager), email);
  }
  if (device_manager && !device_manager->empty()) {
    // The device is managed and requires profile separation.
    return l10n_util::GetStringFUTF8(
        IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_SEPARATION_DEVICE_MANAGED_BY,
        base::UTF8ToUTF16(*device_manager), email);
  }
  return l10n_util::GetStringFUTF8(
      IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_SEPARATION_DEVICE_MANAGED, email);
#else
  if (entry->GetHostedDomain() == kNoHostedDomainFound) {
    return std::string();
  }
  const std::string domain_name = entry->GetHostedDomain().empty()
                                      ? account_domain_name
                                      : entry->GetHostedDomain();
  return l10n_util::GetStringFUTF8(
      IDS_ENTERPRISE_PROFILE_WELCOME_ACCOUNT_EMAIL_MANAGED_BY, email,
      base::UTF8ToUTF16(domain_name));
#endif  //  !BUILDFLAG(IS_CHROMEOS)
}

base::Value::Dict ManagedUserProfileNoticeHandler::GetProfileInfoValue() {
  base::Value::Dict dict;
  dict.Set("pictureUrl", GetPictureUrl());

  std::string title =
      l10n_util::GetStringUTF8(IDS_ENTERPRISE_PROFILE_WELCOME_TITLE);
  std::string subtitle;
  std::string email;
  std::string account_name;
  std::string enterprise_info;
  ProfileAttributesEntry* entry = GetProfileEntry();

  switch (type_) {
    case ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncEnabled:
      dict.Set("showEnterpriseBadge", true);
      subtitle = GetManagedAccountTitle(entry, domain_name_);
      enterprise_info = l10n_util::GetStringUTF8(
          IDS_ENTERPRISE_PROFILE_WELCOME_MANAGED_DESCRIPTION_WITH_SYNC);
      dict.Set("proceedLabel", l10n_util::GetStringUTF8(
                                   IDS_PROFILE_PICKER_IPH_NEXT_BUTTON_LABEL));
      break;
    case ManagedUserProfileNoticeUI::ScreenType::kEntepriseAccountSyncDisabled:
      dict.Set("showEnterpriseBadge", true);
      subtitle = GetManagedAccountTitle(entry, domain_name_);
      enterprise_info = l10n_util ::GetStringUTF8(
          IDS_ENTERPRISE_PROFILE_WELCOME_MANAGED_DESCRIPTION_WITHOUT_SYNC);
      dict.Set("proceedLabel", l10n_util::GetStringUTF8(IDS_DONE));
      break;
    case ManagedUserProfileNoticeUI::ScreenType::kConsumerAccountSyncDisabled:
      dict.Set("showEnterpriseBadge", false);
      subtitle = GetManagedDeviceTitle();
      enterprise_info =
          l10n_util::GetStringUTF8(IDS_SYNC_DISABLED_CONFIRMATION_DETAILS);
      dict.Set("proceedLabel", l10n_util::GetStringUTF8(IDS_DONE));
      break;
    case ManagedUserProfileNoticeUI::ScreenType::kEnterpriseOIDC:
      title =
          l10n_util::GetStringUTF8(IDS_ENTERPRISE_WELCOME_PROFILE_SETUP_TITLE);
      dict.Set("showEnterpriseBadge", true);
      subtitle = l10n_util::GetStringUTF8(
          IDS_ENTERPRISE_PROFILE_WELCOME_PROFILE_SEPARATION_ACCOUNT_MANAGED);
      enterprise_info = l10n_util::GetStringUTF8(
          IDS_ENTERPRISE_PROFILE_WELCOME_MANAGED_DESCRIPTION_WITH_SYNC);
      dict.Set("proceedLabel",
               l10n_util::GetStringUTF8(
                   profile_creation_required_by_policy_
                       ? IDS_ENTERPRISE_PROFILE_WELCOME_CREATE_PROFILE_BUTTON
                       : IDS_APP_CONTINUE));
      break;
    case ManagedUserProfileNoticeUI::ScreenType::kEnterpriseAccountCreation:
      title = l10n_util::GetStringUTF8(
          profile_creation_required_by_policy_
              ? IDS_ENTERPRISE_WELCOME_PROFILE_REQUIRED_TITLE
              : IDS_ENTERPRISE_WELCOME_PROFILE_WILL_BE_MANAGED_TITLE);
      dict.Set("showEnterpriseBadge",
               !enterprise_util::IsKnownConsumerDomain(domain_name_));
      subtitle = GetManagedAccountTitleWithEmail(Profile::FromWebUI(web_ui()),
                                                 entry, domain_name_, email_);
      enterprise_info = l10n_util::GetStringUTF8(
          IDS_ENTERPRISE_PROFILE_WELCOME_MANAGED_DESCRIPTION_WITH_SYNC);
      dict.Set("proceedLabel",
               l10n_util::GetStringUTF8(
                   profile_creation_required_by_policy_
                       ? IDS_ENTERPRISE_PROFILE_WELCOME_CREATE_PROFILE_BUTTON
                       : IDS_APP_CONTINUE));

      AccountInfo account_info =
          IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
              ->FindExtendedAccountInfoByAccountId(account_id_);
      CHECK(!account_info.IsEmpty());
      dict.Set("continueAs", l10n_util::GetStringFUTF8(
                                 IDS_PROFILES_DICE_WEB_ONLY_SIGNIN_BUTTON,
                                 base::UTF8ToUTF16(account_info.given_name)));
      dict.Set("email", base::UTF16ToUTF8(email_));
      dict.Set("accountName", account_info.full_name);

#if !BUILDFLAG(IS_CHROMEOS)
      // We apply the checkLinkDataCheckboxByDefault to true value only if the
      // link data checkbox is visible and the policy
      // ProfileSeparationDataMigrationSettings is set to its OPTOUT value (2)
      // or the legacy policy EnterpriseProfileCreationKeepBrowsingData is set
      // to True.
      bool profile_separation_data_migration_settings_optout =
          Profile::FromWebUI(web_ui())->GetPrefs()->GetInteger(
              prefs::kProfileSeparationDataMigrationSettings) == 2;
      bool check_link_Data_checkbox_by_default_from_legacy_policy =
          g_browser_process->local_state()->GetBoolean(
              prefs::kEnterpriseProfileCreationKeepBrowsingData);
      dict.Set("checkLinkDataCheckboxByDefault",
               show_link_data_option_ &&
                   (profile_separation_data_migration_settings_optout ||
                    check_link_Data_checkbox_by_default_from_legacy_policy));
#endif
      break;
  }

  dict.Set("title", title);
  dict.Set("subtitle", subtitle);
  dict.Set("enterpriseInfo", enterprise_info);

  return dict;
}

ProfileAttributesEntry* ManagedUserProfileNoticeHandler::GetProfileEntry()
    const {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path_);
  DCHECK(entry);
  return entry;
}

std::string ManagedUserProfileNoticeHandler::GetPictureUrl() {
  std::optional<gfx::Image> icon;
  if (type_ ==
      ManagedUserProfileNoticeUI::ScreenType::kEnterpriseAccountCreation) {
    AccountInfo account_info =
        IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui()))
            ->FindExtendedAccountInfoByAccountId(account_id_);
    DCHECK(!account_info.IsEmpty());
    icon = account_info.account_image.IsEmpty()
               ? ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                     profiles::GetPlaceholderAvatarIconResourceID())
               : account_info.account_image;
  } else if (type_ == ManagedUserProfileNoticeUI::ScreenType::kEnterpriseOIDC) {
    icon = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }

  const int avatar_icon_size = kAvatarSize * web_ui()->GetDeviceScaleFactor();
  return webui::GetBitmapDataUrl(
      profiles::GetSizedAvatarIcon(
          icon.value_or(GetProfileEntry()->GetAvatarIcon(avatar_icon_size)),
          avatar_icon_size, avatar_icon_size)
          .AsBitmap());
}

ManagedUserProfileNoticeUI::ScreenType
ManagedUserProfileNoticeHandler::GetTypeForTesting() {
  return type_;
}

void ManagedUserProfileNoticeHandler::CallProceedCallbackForTesting(
    signin::SigninChoice choice) {
  if (process_user_choice_with_confirmation_callback_) {
    std::move(process_user_choice_with_confirmation_callback_)
        .Run(choice, base::BindOnce(
                         &ManagedUserProfileNoticeHandler::OnUserChoiceHandled,
                         weak_ptr_factory_.GetWeakPtr()));
  }
}

void ManagedUserProfileNoticeHandler::OnUserChoiceHandled(
    signin::SigninChoiceOperationResult result) {
  if (!UseMultiscreen() && done_callback_) {
    DisallowJavascript();
    std::move(done_callback_).Run();
    return;
  }

  if (type_ == ManagedUserProfileNoticeUI::ScreenType::kEnterpriseOIDC) {
    processing_timer_.Stop();
  }

  switch (result) {
    case signin::SigninChoiceOperationResult::SIGNIN_TIMEOUT:
      FireWebUIListener("on-state-changed",
                        ManagedUserProfileNoticeHandler::State::kTimeout);
      break;

    case signin::SigninChoiceOperationResult::SIGNIN_SILENT_SUCCESS:
      if (done_callback_) {
        DisallowJavascript();
        std::move(done_callback_).Run();
      }
      break;

    case signin::SigninChoiceOperationResult::SIGNIN_ERROR:
      FireWebUIListener("on-state-changed",
                        ManagedUserProfileNoticeHandler::State::kError);
      break;

    case signin::SigninChoiceOperationResult::SIGNIN_CONFIRM_SUCCESS:
      FireWebUIListener("on-state-changed",
                        ManagedUserProfileNoticeHandler::State::kSuccess);
      break;
  }
}
