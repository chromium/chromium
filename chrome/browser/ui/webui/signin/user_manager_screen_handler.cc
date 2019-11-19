// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/user_manager_screen_handler.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

namespace {
// User dictionary keys.
const char kKeyUsername[] = "username";
const char kKeyDisplayName[]= "displayName";
const char kKeyEmailAddress[] = "emailAddress";
const char kKeyProfilePath[] = "profilePath";
const char kKeyPublicAccount[] = "publicAccount";
const char kKeyLegacySupervisedUser[] = "legacySupervisedUser";
const char kKeyChildUser[] = "childUser";
const char kKeyCanRemove[] = "canRemove";
const char kKeyIsOwner[] = "isOwner";
const char kKeyIsDesktop[] = "isDesktopUser";
const char kKeyAvatarUrl[] = "userImage";
const char kKeyNeedsSignin[] = "needsSignin";
const char kKeyHasLocalCreds[] = "hasLocalCreds";
const char kKeyIsProfileLoaded[] = "isProfileLoaded";

// JS API callback names.
const char kJsApiUserManagerInitialize[] = "userManagerInitialize";
const char kJsApiUserManagerAuthLaunchUser[] = "authenticatedLaunchUser";
const char kJsApiUserManagerLaunchGuest[] = "launchGuest";
const char kJsApiUserManagerLaunchUser[] = "launchUser";
const char kJsApiUserManagerRemoveUser[] = "removeUser";
const char kJsApiUserManagerLogRemoveUserWarningShown[] =
    "logRemoveUserWarningShown";
const char kJsApiUserManagerRemoveUserWarningLoadStats[] =
    "removeUserWarningLoadStats";
const char kJsApiUserManagerAreAllProfilesLocked[] =
    "areAllProfilesLocked";
const size_t kSigninAvatarIconSize = 180;
const int kMaxOAuthRetries = 3;

std::string GetAvatarImage(const ProfileAttributesEntry* entry) {
  bool is_gaia_picture = entry->IsUsingGAIAPicture() &&
                         entry->GetGAIAPicture() != nullptr;

  // If the avatar is too small (i.e. the old-style low resolution avatar),
  // it will be pixelated when displayed in the User Manager, so we should
  // return the placeholder avatar instead.
  gfx::Image avatar_image = entry->GetAvatarIcon();
  if (avatar_image.Width() <= profiles::kAvatarIconSize ||
      avatar_image.Height() <= profiles::kAvatarIconSize) {
    avatar_image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }
  gfx::Image resized_image = profiles::GetSizedAvatarIcon(
      avatar_image, is_gaia_picture, kSigninAvatarIconSize,
      kSigninAvatarIconSize);
  return webui::GetBitmapDataUrl(resized_image.AsBitmap());
}

bool IsGuestModeEnabled() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service->GetBoolean(prefs::kBrowserGuestModeEnabled);
}

bool IsAddPersonEnabled() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service->GetBoolean(prefs::kBrowserAddPersonEnabled);
}

// Executes the action specified by the URL's Hash parameter, if any. Deletes
// itself after the action would be performed.
class UrlHashHelper : public BrowserListObserver {
 public:
  UrlHashHelper(Browser* browser, const std::string& hash);
  ~UrlHashHelper() override;

  void ExecuteUrlHash();

  // BrowserListObserver overrides:
  void OnBrowserRemoved(Browser* browser) override;

 private:
  Browser* browser_;
  Profile* profile_;
  std::string hash_;

  DISALLOW_COPY_AND_ASSIGN(UrlHashHelper);
};

UrlHashHelper::UrlHashHelper(Browser* browser, const std::string& hash)
    : browser_(browser),
      profile_(browser->profile()),
      hash_(hash) {
  BrowserList::AddObserver(this);
}

UrlHashHelper::~UrlHashHelper() {
  BrowserList::RemoveObserver(this);
}

void UrlHashHelper::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_)
    browser_ = nullptr;
}

void UrlHashHelper::ExecuteUrlHash() {
  Browser* target_browser = browser_;
  if (!target_browser) {
    target_browser = chrome::FindLastActiveWithProfile(profile_);
    if (!target_browser)
      return;
  }

  if (hash_ == profiles::kUserManagerSelectProfileTaskManager)
    chrome::OpenTaskManager(target_browser);
  else if (hash_ == profiles::kUserManagerSelectProfileAboutChrome)
    chrome::ShowAboutChrome(target_browser);
  else if (hash_ == profiles::kUserManagerSelectProfileChromeSettings)
    chrome::ShowSettings(target_browser);
}

void HandleLogRemoveUserWarningShown(const base::ListValue* args) {
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER_SHOW_WARNING);
}

void DisplayErrorMessage(const base::string16 error_message,
                         content::WebUI* web_ui) {
  LoginUIServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui)->GetOriginalProfile())
      ->DisplayLoginResult(nullptr, error_message, base::string16());
  UserManagerProfileDialog::ShowDialogAndDisplayErrorMessage(
      web_ui->GetWebContents()->GetBrowserContext());
}

void RecordAuthenticatedLaunchUserEvent(
    const AuthenticatedLaunchUserEvent& event) {
  UMA_HISTOGRAM_ENUMERATION(kAuthenticatedLaunchUserEventMetricsName, event,
                            AuthenticatedLaunchUserEvent::EVENT_COUNT);
}

}  // namespace

const char kAuthenticatedLaunchUserEventMetricsName[] =
    "Signin.AuthenticatedLaunchUserEvent";

// ProfileUpdateObserver ------------------------------------------------------

class UserManagerScreenHandler::ProfileUpdateObserver
    : public ProfileAttributesStorage::Observer {
 public:
  ProfileUpdateObserver(
      ProfileManager* profile_manager, UserManagerScreenHandler* handler)
      : profile_manager_(profile_manager),
        user_manager_handler_(handler) {
    DCHECK(profile_manager_);
    DCHECK(user_manager_handler_);
    profile_manager_->GetProfileAttributesStorage().AddObserver(this);
  }

  ~ProfileUpdateObserver() override {
    DCHECK(profile_manager_);
    profile_manager_->GetProfileAttributesStorage().RemoveObserver(this);
  }

 private:
  // ProfileAttributesStorage::Observer implementation:
  // If any change has been made to a profile, propagate it to all the
  // visible user manager screens.
  void OnProfileAdded(const base::FilePath& profile_path) override {
    user_manager_handler_->SendUserList();
  }

  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const base::string16& profile_name) override {
    // TODO(noms): Change 'SendUserList' to 'removeUser' JS-call when
    // UserManager is able to find pod belonging to removed user.
    user_manager_handler_->SendUserList();
  }

  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override {
    user_manager_handler_->SendUserList();
  }

  void OnProfileAvatarChanged(const base::FilePath& profile_path) override {
    user_manager_handler_->SendUserList();
  }

  void OnProfileHighResAvatarLoaded(
      const base::FilePath& profile_path) override {
    user_manager_handler_->SendUserList();
  }

  void OnProfileSigninRequiredChanged(
      const base::FilePath& profile_path) override {
    user_manager_handler_->SendUserList();
  }

  void OnProfileIsOmittedChanged(
      const base::FilePath& profile_path) override {
    user_manager_handler_->SendUserList();
  }

  ProfileManager* profile_manager_;

  UserManagerScreenHandler* user_manager_handler_;  // Weak; owns us.

  DISALLOW_COPY_AND_ASSIGN(ProfileUpdateObserver);
};

// UserManagerScreenHandler ---------------------------------------------------

UserManagerScreenHandler::UserManagerScreenHandler() {
  profile_attributes_storage_observer_.reset(
      new UserManagerScreenHandler::ProfileUpdateObserver(
          g_browser_process->profile_manager(), this));

  // TODO(mahmadi): Remove the following once prefs are cleared for everyone.
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);

  const PrefService::Preference* guest_mode_enabled_pref =
      service->FindPreference(prefs::kBrowserGuestModeEnabled);
  const PrefService::Preference* add_person_enabled_pref =
      service->FindPreference(prefs::kBrowserAddPersonEnabled);

  if (guest_mode_enabled_pref->HasUserSetting() ||
      add_person_enabled_pref->HasUserSetting()) {
    service->ClearPref(guest_mode_enabled_pref->name());
    service->ClearPref(add_person_enabled_pref->name());
    base::RecordAction(
        base::UserMetricsAction("UserManager_Cleared_Legacy_User_Prefs"));
  }
}

UserManagerScreenHandler::~UserManagerScreenHandler() {
  BrowserList::RemoveObserver(this);
}

void UserManagerScreenHandler::HandleInitialize(const base::ListValue* args) {
  // If the URL has a hash parameter, store it for later.
  args->GetString(0, &url_hash_);

  SendUserList();
  web_ui()->CallJavascriptFunctionUnsafe(
      "cr.ui.UserManager.showUserManagerScreen",
      base::Value(IsGuestModeEnabled()), base::Value(IsAddPersonEnabled()));
}

void UserManagerScreenHandler::HandleAuthenticatedLaunchUser(
    const base::ListValue* args) {
  const base::Value* profile_path_value;
  if (!args->Get(0, &profile_path_value))
    return;

  base::FilePath profile_path;
  if (!base::GetValueAsFilePath(*profile_path_value, &profile_path))
    return;

  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_path, &entry)) {
    return;
  }

  base::string16 email_address;
  if (!args->GetString(1, &email_address))
    return;

  std::string password;
  if (!args->GetString(2, &password))
    return;

  authenticating_profile_path_ = profile_path;
  email_address_ = base::UTF16ToUTF8(email_address);

  // Only try to validate locally or check the password change detection
  // if we actually have a local credential saved.
  if (!entry->GetLocalAuthCredentials().empty()) {
    RecordAuthenticatedLaunchUserEvent(
        AuthenticatedLaunchUserEvent::LOCAL_REAUTH_DIALOG);
    if (LocalAuth::ValidateLocalAuthCredentials(entry, password)) {
      ReportAuthenticationResult(true, ProfileMetrics::AUTH_LOCAL);
      return;
    }

    // This could be a mis-typed password or typing a new password while we
    // still have a hash of the old one.  The new way of checking a password
    // change makes use of a token so we do that... if it's available.
    if (!oauth_client_) {
      oauth_client_ = std::make_unique<gaia::GaiaOAuthClient>(
          content::BrowserContext::GetDefaultStoragePartition(
              web_ui()->GetWebContents()->GetBrowserContext())
              ->GetURLLoaderFactoryForBrowserProcess());
    }

    const std::string token = entry->GetPasswordChangeDetectionToken();
    if (!token.empty()) {
      oauth_client_->GetTokenHandleInfo(token, kMaxOAuthRetries, this);
      return;
    }
  }

  content::BrowserContext* browser_context =
      web_ui()->GetWebContents()->GetBrowserContext();

  if (!email_address_.empty()) {
    // In order to support the upgrade case where we have a local hash but no
    // password token, the user must perform a full online reauth.
    RecordAuthenticatedLaunchUserEvent(
        AuthenticatedLaunchUserEvent::GAIA_REAUTH_DIALOG);
    UserManagerProfileDialog::ShowUnlockDialogWithProfilePath(
        browser_context, email_address_, profile_path);
  } else if (entry->IsSigninRequired() && entry->IsSupervised()) {
    // Supervised profile will only be locked when force-sign-in is enabled
    // and it shouldn't be unlocked. Display the error message directly via
    // the system profile to avoid profile creation.
    RecordAuthenticatedLaunchUserEvent(
        AuthenticatedLaunchUserEvent::SUPERVISED_PROFILE_BLOCKED_WARNING);
    DisplayErrorMessage(
        l10n_util::GetStringUTF16(IDS_SUPERVISED_USER_NOT_ALLOWED_BY_POLICY),
        web_ui());
  } else if (entry->IsSigninRequired() && signin_util::IsForceSigninEnabled() &&
             entry->GetActiveTime() != base::Time()) {
    // If force-sign-in is enabled, do not allow users to sign in to a
    // pre-existing locked profile, as this may force unexpected profile data
    // merge. We consider a profile as pre-existing if it has been actived
    // previously. A pre-existed profile can still be used if it has been signed
    // in with an email address matched RestrictSigninToPattern policy already.
    RecordAuthenticatedLaunchUserEvent(
        AuthenticatedLaunchUserEvent::USED_PROFILE_BLOCKED_WARNING);
    LoginUIServiceFactory::GetForProfile(
        Profile::FromWebUI(web_ui())->GetOriginalProfile())
        ->SetProfileBlockingErrorMessage();
    UserManagerProfileDialog::ShowDialogAndDisplayErrorMessage(
        web_ui()->GetWebContents()->GetBrowserContext());
  } else {
    // Fresh sign in via user manager without existing email address.
    DCHECK(signin_util::IsForceSigninEnabled());
    RecordAuthenticatedLaunchUserEvent(
        AuthenticatedLaunchUserEvent::FORCED_PRIMARY_SIGNIN_DIALOG);
    UserManagerProfileDialog::ShowForceSigninDialog(browser_context,
                                                    profile_path);
  }
}

void UserManagerScreenHandler::HandleRemoveUser(const base::ListValue* args) {
  DCHECK(args);
  const base::Value* profile_path_value;
  if (!args->Get(0, &profile_path_value)) {
    NOTREACHED();
    return;
  }

  base::FilePath profile_path;
  if (!base::GetValueAsFilePath(*profile_path_value, &profile_path)) {
    NOTREACHED();
    return;
  }

  DCHECK(profiles::IsMultipleProfilesEnabled());

  if (!signin_util::IsForceSigninEnabled() &&
      profiles::AreAllNonChildNonSupervisedProfilesLocked()) {
    web_ui()->CallJavascriptFunctionUnsafe(
        "cr.webUIListenerCallback", base::Value("show-error-dialog"),
        base::Value(l10n_util::GetStringUTF8(
            IDS_USER_MANAGER_REMOVE_PROFILE_PROFILES_LOCKED_ERROR)));
    return;
  }

  // The callback is run if the only profile has been deleted, and a new
  // profile has been created to replace it.
  webui::DeleteProfileAtPath(profile_path,
                             ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
}

void UserManagerScreenHandler::HandleLaunchGuest(const base::ListValue* args) {
  if (IsGuestModeEnabled()) {
    profiles::SwitchToGuestProfile(
        base::Bind(&UserManagerScreenHandler::OnSwitchToProfileComplete,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    // The UI should have prevented the user from allowing the selection of
    // guest mode.
    NOTREACHED();
  }
}

void UserManagerScreenHandler::HandleAreAllProfilesLocked(
    const base::ListValue* args) {
  std::string webui_callback_id;
  CHECK_EQ(1U, args->GetSize());
  bool success = args->GetString(0, &webui_callback_id);
  DCHECK(success);

  AllowJavascript();
  ResolveJavascriptCallback(
      base::Value(webui_callback_id),
      base::Value(profiles::AreAllNonChildNonSupervisedProfilesLocked()));
}

void UserManagerScreenHandler::HandleLaunchUser(const base::ListValue* args) {
  const base::Value* profile_path_value = NULL;
  if (!args->Get(0, &profile_path_value))
    return;

  base::FilePath profile_path;
  if (!base::GetValueAsFilePath(*profile_path_value, &profile_path))
    return;

  ProfileAttributesEntry* entry;
  if (!g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_path, &entry)) {
    NOTREACHED();
    return;
  }

  // It's possible that a user breaks into the user-manager page using the
  // JavaScript Inspector and causes a "locked" profile to call this
  // unauthenticated version of "launch" instead of the proper one.  Thus,
  // we have to validate in (secure) C++ code that it really is a profile
  // not needing authentication.  If it is, just ignore the "launch" request.
  if (entry->IsSigninRequired())
    return;
  ProfileMetrics::LogProfileAuthResult(ProfileMetrics::AUTH_UNNECESSARY);

  profiles::SwitchToProfile(
      profile_path, false, /* reuse any existing windows */
      base::Bind(&UserManagerScreenHandler::OnSwitchToProfileComplete,
                 weak_ptr_factory_.GetWeakPtr()),
      ProfileMetrics::SWITCH_PROFILE_MANAGER);
}

void UserManagerScreenHandler::HandleRemoveUserWarningLoadStats(
    const base::ListValue* args) {
  const base::Value* profile_path_value;

  if (!args->Get(0, &profile_path_value))
    return;

  base::Time start_time = base::Time::Now();
  base::FilePath profile_path;

  if (!base::GetValueAsFilePath(*profile_path_value, &profile_path))
    return;

  base::Value return_profile_path(profile_path.value());
  Profile* profile = g_browser_process->profile_manager()->
      GetProfileByPath(profile_path);

  if (profile) {
    GatherStatistics(start_time, profile);
  } else {
    g_browser_process->profile_manager()->LoadProfileByPath(
        profile_path, false,
        base::Bind(&UserManagerScreenHandler::GatherStatistics,
                   weak_ptr_factory_.GetWeakPtr(), start_time));
  }
}

void UserManagerScreenHandler::GatherStatistics(base::Time start_time,
                                                Profile* profile) {
  if (profile) {
    ProfileStatisticsFactory::GetForProfile(profile)->GatherStatistics(
        base::Bind(&UserManagerScreenHandler::RemoveUserDialogLoadStatsCallback,
                   weak_ptr_factory_.GetWeakPtr(), profile->GetPath(),
                   start_time));
  }
}

void UserManagerScreenHandler::RemoveUserDialogLoadStatsCallback(
    base::FilePath profile_path,
    base::Time start_time,
    profiles::ProfileCategoryStats result) {
  // Copy result into return_value.
  base::DictionaryValue return_value;
  for (const auto& item : result) {
    auto stat = std::make_unique<base::DictionaryValue>();
    stat->SetKey("count", base::Value(item.count));
    return_value.SetWithoutPathExpansion(item.category, std::move(stat));
  }
  if (result.size() == profiles::kProfileStatisticsCategories.size()) {
    // All categories are finished.
    UMA_HISTOGRAM_TIMES("Profile.RemoveUserWarningStatsTime",
                        base::Time::Now() - start_time);
  }
  web_ui()->CallJavascriptFunctionUnsafe("updateRemoveWarningDialog",
                                         base::Value(profile_path.value()),
                                         return_value);
}

void UserManagerScreenHandler::OnGetTokenInfoResponse(
    std::unique_ptr<base::DictionaryValue> token_info) {
  // Password is unchanged so user just mistyped it.  Ask again.
  ReportAuthenticationResult(false, ProfileMetrics::AUTH_FAILED);
}

void UserManagerScreenHandler::OnOAuthError() {
  // Password has changed.  Go through online signin flow.
  DCHECK(!email_address_.empty());
  oauth_client_.reset();
  UserManagerProfileDialog::ShowUnlockDialog(
      web_ui()->GetWebContents()->GetBrowserContext(), email_address_);
}

void UserManagerScreenHandler::OnNetworkError(int response_code) {
  // Inconclusive but can't do real signin without being online anyway.
    oauth_client_.reset();
    ReportAuthenticationResult(false, ProfileMetrics::AUTH_FAILED_OFFLINE);
}

void UserManagerScreenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kJsApiUserManagerInitialize,
      base::BindRepeating(&UserManagerScreenHandler::HandleInitialize,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kJsApiUserManagerAuthLaunchUser,
      base::BindRepeating(
          &UserManagerScreenHandler::HandleAuthenticatedLaunchUser,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kJsApiUserManagerLaunchGuest,
      base::BindRepeating(&UserManagerScreenHandler::HandleLaunchGuest,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kJsApiUserManagerLaunchUser,
      base::BindRepeating(&UserManagerScreenHandler::HandleLaunchUser,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kJsApiUserManagerRemoveUser,
      base::BindRepeating(&UserManagerScreenHandler::HandleRemoveUser,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kJsApiUserManagerLogRemoveUserWarningShown,
      base::BindRepeating(&HandleLogRemoveUserWarningShown));
  web_ui()->RegisterMessageCallback(
      kJsApiUserManagerRemoveUserWarningLoadStats,
      base::BindRepeating(
          &UserManagerScreenHandler::HandleRemoveUserWarningLoadStats,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kJsApiUserManagerAreAllProfilesLocked,
      base::BindRepeating(&UserManagerScreenHandler::HandleAreAllProfilesLocked,
                          base::Unretained(this)));

  // Unused callbacks from screen_account_picker.js
  web_ui()->RegisterMessageCallback("accountPickerReady", base::DoNothing());
  web_ui()->RegisterMessageCallback("loginUIStateChanged", base::DoNothing());
  web_ui()->RegisterMessageCallback("hideCaptivePortal", base::DoNothing());
  web_ui()->RegisterMessageCallback("getTabletModeState", base::DoNothing());
  // Unused callbacks from display_manager.js
  web_ui()->RegisterMessageCallback("showAddUser", base::DoNothing());
  web_ui()->RegisterMessageCallback("updateCurrentScreen", base::DoNothing());
  web_ui()->RegisterMessageCallback("loginVisible", base::DoNothing());
  // Unused callbacks from user_pod_row.js
  web_ui()->RegisterMessageCallback("focusPod", base::DoNothing());
  web_ui()->RegisterMessageCallback("noPodFocused", base::DoNothing());
}

void UserManagerScreenHandler::OnBrowserAdded(Browser* browser) {
  // Only respond to one Browser Opened event.
  BrowserList::RemoveObserver(this);

  // Unlock the profile after browser opens so startup can read the lock bit.
  // Any necessary authentication must have been successful to reach this point.
  ProfileAttributesEntry* entry = nullptr;
  if (!browser->profile()->IsGuestSession()) {
    bool has_entry = g_browser_process->profile_manager()
                         ->GetProfileAttributesStorage()
                         .GetProfileAttributesWithPath(
                             browser->profile()->GetPath(), &entry);
    DCHECK(has_entry);
    // If force sign in is enabled and profile is not signed in, do not close
    // UserManager and unlock profile.
    if (signin_util::IsForceSigninEnabled() && !entry->IsAuthenticated())
      return;
    entry->SetIsSigninRequired(false);
  }

  if (!url_hash_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&UrlHashHelper::ExecuteUrlHash,
                       base::Owned(new UrlHashHelper(browser, url_hash_))));
  }

  // This call is last as it deletes this object.
  UserManager::Hide();
}

void UserManagerScreenHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  // For Control Bar.
  localized_strings->SetString("signedIn",
      l10n_util::GetStringUTF16(IDS_SCREEN_LOCK_ACTIVE_USER));
  localized_strings->SetString("addUser",
      l10n_util::GetStringUTF16(IDS_ADD_USER_BUTTON));
  localized_strings->SetString("cancel", l10n_util::GetStringUTF16(IDS_CANCEL));
  localized_strings->SetString(
      "browseAsGuest", l10n_util::GetStringUTF16(IDS_BROWSE_AS_GUEST_BUTTON));
  localized_strings->SetString("addSupervisedUser",
      l10n_util::GetStringUTF16(IDS_CREATE_LEGACY_SUPERVISED_USER_MENU_LABEL));

  // For AccountPickerScreen.
  localized_strings->SetString("screenType", "login-add-user");
  localized_strings->SetString("highlightStrength", "normal");
  localized_strings->SetString("title",
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  localized_strings->SetString("passwordHint",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_EMPTY_PASSWORD_TEXT));
  localized_strings->SetString("signingIn",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_SIGNING_IN));
  localized_strings->SetString("podMenuButtonAccessibleName",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_MENU_BUTTON_ACCESSIBLE_NAME));
  localized_strings->SetString("podMenuRemoveItemAccessibleName",
      l10n_util::GetStringUTF16(
          IDS_LOGIN_POD_MENU_REMOVE_ITEM_ACCESSIBLE_NAME));
  localized_strings->SetString("removeUser",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_BUTTON));
  localized_strings->SetString("passwordFieldAccessibleName",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME));

  // For AccountPickerScreen, the remove user warning overlay.
  localized_strings->SetString("removeUserWarningButtonTitle",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_BUTTON));
  localized_strings->SetString(
      "removeUserWarningTextNonSync",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_NONSYNC));
  localized_strings->SetString("removeUserWarningTextHistory",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_HISTORY));
  localized_strings->SetString("removeUserWarningTextPasswords",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_PASSWORDS));
  localized_strings->SetString("removeUserWarningTextBookmarks",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_BOOKMARKS));
  localized_strings->SetString(
      "removeUserWarningTextAutofill",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_AUTOFILL));
  localized_strings->SetString("removeUserWarningTextCalculating",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_CALCULATING));
  localized_strings->SetString(
      "removeUserWarningTextSync",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_USER_REMOVE_WARNING_SYNC));
  localized_strings->SetString("removeLegacySupervisedUserWarningText",
      l10n_util::GetStringFUTF16(
          IDS_LOGIN_POD_LEGACY_SUPERVISED_USER_REMOVE_WARNING,
          base::UTF8ToUTF16(
              chrome::kLegacySupervisedUserManagementDisplayURL)));
  localized_strings->SetString(
      "removeNonOwnerUserWarningText",
      l10n_util::GetStringUTF16(IDS_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING));

  // Strings needed for the User Manager tutorial slides.
  localized_strings->SetString("tutorialNext",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_NEXT));
  localized_strings->SetString("tutorialDone",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_DONE));
  localized_strings->SetString("slideWelcomeTitle",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_INTRO_TITLE));
  localized_strings->SetString("slideWelcomeText",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_INTRO_TEXT));
  localized_strings->SetString("slideYourChromeTitle",
      l10n_util::GetStringUTF16(
          IDS_USER_MANAGER_TUTORIAL_SLIDE_YOUR_CHROME_TITLE));
  localized_strings->SetString("slideYourChromeText", l10n_util::GetStringUTF16(
      IDS_USER_MANAGER_TUTORIAL_SLIDE_YOUR_CHROME_TEXT));
  localized_strings->SetString("slideGuestsTitle",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_GUEST_TITLE));
  localized_strings->SetString("slideGuestsText",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_GUEST_TEXT));
  localized_strings->SetString("slideFriendsTitle",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_FRIENDS_TITLE));
  localized_strings->SetString("slideFriendsText",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_FRIENDS_TEXT));
  localized_strings->SetString("slideCompleteTitle",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_OUTRO_TITLE));
  localized_strings->SetString("slideCompleteText",
      l10n_util::GetStringUTF16(IDS_USER_MANAGER_TUTORIAL_SLIDE_OUTRO_TEXT));
  localized_strings->SetString("slideCompleteUserNotFound",
      l10n_util::GetStringUTF16(
          IDS_USER_MANAGER_TUTORIAL_SLIDE_OUTRO_USER_NOT_FOUND));
  localized_strings->SetString("slideCompleteAddUser",
      l10n_util::GetStringUTF16(
          IDS_USER_MANAGER_TUTORIAL_SLIDE_OUTRO_ADD_USER));

  // Strings needed for the user_pod_template public account div, but not ever
  // actually displayed for desktop users.
  localized_strings->SetString("publicAccountReminder", "");
  localized_strings->SetString("publicSessionLanguageAndInput", "");
  localized_strings->SetString("publicAccountEnter", "");
  localized_strings->SetString("publicAccountEnterAccessibleName", "");
  localized_strings->SetString("publicAccountMonitoringWarning", "");
  localized_strings->SetString("publicAccountLearnMore", "");
  localized_strings->SetString("publicAccountMonitoringInfo", "");
  localized_strings->SetString("publicAccountMonitoringInfoItem1", "");
  localized_strings->SetString("publicAccountMonitoringInfoItem2", "");
  localized_strings->SetString("publicAccountMonitoringInfoItem3", "");
  localized_strings->SetString("publicAccountMonitoringInfoItem4", "");
  localized_strings->SetString("publicSessionSelectLanguage", "");
  localized_strings->SetString("publicSessionSelectKeyboard", "");
  localized_strings->SetString("signinBannerText", "");
  localized_strings->SetString("multiProfilesRestrictedPolicyTitle", "");
  localized_strings->SetString("multiProfilesNotAllowedPolicyMsg", "");
  localized_strings->SetString("multiProfilesPrimaryOnlyPolicyMsg", "");
  localized_strings->SetString("multiProfilesOwnerPrimaryOnlyMsg", "");

  // Error message when trying to add a profile while all profiles are locked.
  localized_strings->SetString("addProfileAllProfilesLockedError",
      l10n_util::GetStringUTF16(
          IDS_USER_MANAGER_ADD_PROFILE_PROFILES_LOCKED_ERROR));
  // Error message when trying to browse as guest while all profiles are locked.
  localized_strings->SetString("browseAsGuestAllProfilesLockedError",
      l10n_util::GetStringUTF16(
          IDS_USER_MANAGER_GO_GUEST_PROFILES_LOCKED_ERROR));

  base::string16 prompt_message;
  if (signin_util::IsForceSigninEnabled()) {
    prompt_message = l10n_util::GetStringUTF16(IDS_USER_MANAGER_PROMPT_MESSAGE);
  }

  localized_strings->SetString("userManagerPromptMessage", prompt_message);
}

void UserManagerScreenHandler::SendUserList() {
  base::ListValue users_list;
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetAllProfilesAttributesSortedByName();

  for (const ProfileAttributesEntry* entry : entries) {
    // Don't show profiles still in the middle of being set up as new legacy
    // supervised users.
    if (entry->IsOmitted())
      continue;

    auto profile_value = std::make_unique<base::DictionaryValue>();
    base::FilePath profile_path = entry->GetPath();

    profile_value->SetString(kKeyUsername, entry->GetUserName());
    profile_value->SetString(kKeyEmailAddress, entry->GetUserName());
    profile_value->SetString(kKeyDisplayName,
                             profiles::GetAvatarNameForProfile(profile_path));
    profile_value->SetKey(kKeyProfilePath,
                          base::CreateFilePathValue(profile_path));
    profile_value->SetBoolean(kKeyPublicAccount, false);
    profile_value->SetBoolean(kKeyLegacySupervisedUser,
                              entry->IsLegacySupervised());
    profile_value->SetBoolean(kKeyChildUser, entry->IsChild());
    profile_value->SetBoolean(kKeyNeedsSignin, entry->IsSigninRequired());
    profile_value->SetBoolean(kKeyHasLocalCreds,
                              !entry->GetLocalAuthCredentials().empty());
    profile_value->SetBoolean(kKeyIsOwner, false);
    profile_value->SetBoolean(kKeyCanRemove, true);
    profile_value->SetBoolean(kKeyIsDesktop, true);
    profile_value->SetString(kKeyAvatarUrl, GetAvatarImage(entry));

    // GetProfileByPath returns a pointer if the profile is fully loaded, NULL
    // otherwise.
    Profile* profile =
        g_browser_process->profile_manager()->GetProfileByPath(profile_path);
    profile_value->SetBoolean(kKeyIsProfileLoaded, profile != nullptr);

    users_list.Append(std::move(profile_value));
  }

  web_ui()->CallJavascriptFunctionUnsafe("login.AccountPickerScreen.loadUsers",
                                         users_list,
                                         base::Value(IsGuestModeEnabled()));

  // This is the latest C++ code we have in the flow to show the UserManager.
  // This may be invoked more than once per UserManager lifetime; the
  // UserManager will ensure all relevant logging only happens once.
  UserManager::OnUserManagerShown();
}

void UserManagerScreenHandler::ReportAuthenticationResult(
    bool success,
    ProfileMetrics::ProfileAuth auth) {
  ProfileMetrics::LogProfileAuthResult(auth);
  email_address_.clear();

  if (success) {
    profiles::SwitchToProfile(
        authenticating_profile_path_, true,
        base::Bind(&UserManagerScreenHandler::OnSwitchToProfileComplete,
                   weak_ptr_factory_.GetWeakPtr()),
        ProfileMetrics::SWITCH_PROFILE_UNLOCK);
  } else {
    web_ui()->CallJavascriptFunctionUnsafe(
        "cr.ui.UserManager.showSignInError", base::Value(0),
        base::Value(l10n_util::GetStringUTF8(
            auth == ProfileMetrics::AUTH_FAILED_OFFLINE
                ? IDS_LOGIN_ERROR_AUTHENTICATING_OFFLINE
                : IDS_LOGIN_ERROR_AUTHENTICATING)),
        base::Value(""), base::Value(0));
  }
}

// This callback is run after switching to a new profile has finished. This
// means either a new browser has been created (but not the window), or an
// existing one has been found. The HideUserManager task needs to be posted
// since closing the User Manager before the window is created can flakily
// cause Chrome to close.
void UserManagerScreenHandler::OnSwitchToProfileComplete(
    Profile* profile, Profile::CreateStatus profile_create_status) {
  Browser* browser = chrome::FindAnyBrowser(profile, false);
  if (browser && browser->window())
    OnBrowserAdded(browser);
  else
    BrowserList::AddObserver(this);
}
